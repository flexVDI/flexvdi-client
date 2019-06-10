/*
    Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

    This file is part of flexVDI Client.

    flexVDI Client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    flexVDI Client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <math.h>
#include <cups/cups.h>
#include "printclient-priv.h"
#include "PPDGenerator.h"
#include "flexvdi-port.h"


int flexvdi_get_printer_list(GSList ** printers) {
    int i;
    cups_dest_t * dests, * dest;
    int num_dests = cupsGetDests(&dests);
    *printers = NULL;
    for (i = num_dests, dest = dests; i > 0; --i, ++dest) {
        if (dest->instance) {
            char * full_instance = g_strconcat(dest->name, "/", dest->instance, NULL);
            *printers = g_slist_prepend(*printers, full_instance);
        } else {
            *printers = g_slist_prepend(*printers, g_strdup(dest->name));
        }
    }
    cupsFreeDests(num_dests, dests);
    return TRUE;
}


typedef struct CupsPrinter {
    cups_dest_t * dests, * dest;
    int num_dests;
    cups_dinfo_t * dinfo;
    http_t * http;
} CupsPrinter;


static CupsPrinter * cups_printer_new(const char * printer) {
    CupsPrinter * cups = (CupsPrinter *)g_malloc0(sizeof(CupsPrinter));
    g_autofree gchar * name = g_strdup(printer), * instance;
    if ((instance = g_strrstr(name, "/")) != NULL)
        *instance++ = '\0';
    cups->num_dests = cupsGetDests(&cups->dests);
    if (cups->dests) {
        cups->dest = cupsGetDest(name, instance, cups->num_dests, cups->dests);
        if (cups->dest) {
            cups->http = cupsConnectDest(cups->dest, CUPS_DEST_FLAGS_NONE,
                                         30000, NULL, NULL, 0, NULL, NULL);
            if (cups->http) {
                cups->dinfo = cupsCopyDestInfo(cups->http, cups->dest);
            }
        }
    }
    if (!cups->dinfo)
        g_warning("Failed to contact CUPS for printer %s", printer);
    return cups;
}


static void cups_printer_delete(CupsPrinter * cups) {
    cupsFreeDestInfo(cups->dinfo);
    httpClose(cups->http);
    cupsFreeDests(cups->num_dests, cups->dests);
    g_free(cups);
}


static ipp_attribute_t * cups_printer_attr_supported(CupsPrinter * cups, const char * attr_name) {
    return cupsFindDestSupported(cups->http, cups->dest, cups->dinfo, attr_name);
}


static ipp_attribute_t * cups_printer_attr_default(CupsPrinter * cups, const char * attr_name) {
    return cupsFindDestDefault(cups->http, cups->dest, cups->dinfo, attr_name);
}


static int cups_printer_attr_has_others(CupsPrinter * cups, const char * attr_name, const char * value) {
    ipp_attribute_t * attr = cups_printer_attr_supported(cups, attr_name);
    if (attr) {
        int i = ippGetCount(attr) - 1;
        g_autofree gchar * ivalue = g_utf8_casefold(value, -1);
        for (; i >= 0; --i) {
            g_autofree gchar * cmpString = g_utf8_casefold(ippGetString(attr, i, NULL), -1);
            if (strcmp(cmpString, ivalue) != 0)
                return TRUE;
        }
    }
    return FALSE;
}


static void cups_printer_get_resolutions(PPDGenerator * ppd, CupsPrinter * cups) {
    ipp_attribute_t * attr = cups_printer_attr_supported(cups, "printer-resolution");
    if (attr) {
        int i = ippGetCount(attr) - 1, yres;
        ipp_res_t units;
        while (i >= 0) {
            ppd_generator_add_resolution(ppd, ippGetResolution(attr, i--, &yres, &units));
        }
        if ((attr = cups_printer_attr_default(cups, "printer-resolution"))) {
            ppd_generator_set_default_resolution(ppd, ippGetResolution(attr, 0, &yres, &units));
        }
    }
}


static void cups_printer_get_papers(PPDGenerator * ppd, CupsPrinter * cups) {
    cups_size_t size;
    double f = 29704.0 / 842.0;
    int i, count = cupsGetDestMediaCount(cups->http, cups->dest, cups->dinfo, 0);
    for (i = 0; i < count; ++i) {
        if (cupsGetDestMediaByIndex(cups->http, cups->dest, cups->dinfo, i, 0, &size)) {
            const char * name = cupsLocalizeDestMedia(cups->http, cups->dest, cups->dinfo, 0, &size);
            // XXX There is a bug until Cups 2.2 that adds (Borderless) in the wrong case
            gchar ** name_parts = g_strsplit(name, " (", -1);
            ppd_generator_add_paper_size(ppd, g_strdup(name_parts[0]),
                round(size.width / f), round(size.length / f),
                round(size.left / f), round(size.bottom / f),
                round((size.width - size.right) / f),
                round((size.length - size.top) / f));
            g_strfreev(name_parts);
        }
    }

    ipp_attribute_t * attr = cups_printer_attr_default(cups, CUPS_MEDIA);
    const char * default_media = attr ? ippGetString(attr, 0, NULL) : "a4";
    ppd_generator_set_default_paper_size(ppd, g_strdup(default_media));
}


static char * capitalize_first(const char * str) {
    char tmp[7];
    tmp[g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(str)), tmp)] = '\0';
    return g_strconcat(tmp, g_utf8_next_char(str), NULL);
}


static void cups_printer_get_media_sources(PPDGenerator * ppd, CupsPrinter * cups) {
    ipp_attribute_t * attr = cups_printer_attr_supported(cups, CUPS_MEDIA_SOURCE);
    if (attr) {
        int i, count = ippGetCount(attr);
        for (i = 0; i < count; ++i) {
            ppd_generator_add_tray(ppd, capitalize_first(ippGetString(attr, i, NULL)));
        }
        if ((attr = cups_printer_attr_default(cups, CUPS_MEDIA_SOURCE))) {
            ppd_generator_set_default_tray(ppd, capitalize_first(ippGetString(attr, 0, NULL)));
        }
    }
}


static void cups_printer_get_media_types(PPDGenerator * ppd, CupsPrinter * cups) {
    ipp_attribute_t * attr = cups_printer_attr_supported(cups, CUPS_MEDIA_TYPE);
    if (attr) {
        int i, count = ippGetCount(attr);
        for (i = 0; i < count; ++i) {
            ppd_generator_add_media_type(ppd, capitalize_first(ippGetString(attr, i, NULL)));
        }
        if ((attr = cups_printer_attr_default(cups, CUPS_MEDIA_TYPE))) {
            ppd_generator_set_default_media_type(ppd, capitalize_first(ippGetString(attr, 0, NULL)));
        }
    }
}


char * get_ppd_file(const char * printer) {
    char * result = NULL;
    PPDGenerator * ppd = ppd_generator_new(printer);
    if (ppd) {
        CupsPrinter * cups = cups_printer_new(printer);
        if (cups->dinfo) {
            ppd_generator_set_color(ppd, cups_printer_attr_has_others(cups, CUPS_PRINT_COLOR_MODE, "monochrome"));
            ppd_generator_set_duplex(ppd, cups_printer_attr_has_others(cups, CUPS_SIDES, "one-sided"));
            cups_printer_get_resolutions(ppd, cups);
            cups_printer_get_papers(ppd, cups);
            cups_printer_get_media_sources(ppd, cups);
            cups_printer_get_media_types(ppd, cups);
            result = g_strdup(ppd_generator_run(ppd));
        }
        cups_printer_delete(cups);
    }
    g_object_unref(ppd);
    return result;
}


static int cups_printer_get_media_option(CupsPrinter * cups, char * job_options,
                                         int num_options, cups_option_t ** options) {
    g_autofree gchar * media = get_job_options(job_options, "media");
    if (media) {
        int width, length, result;
        cups_size_t size;
        if (sscanf(media, "%dx%d", &width, &length) < 2) {
            result = cupsGetDestMediaByName(cups->http, cups->dest, cups->dinfo,
                                            media, CUPS_MEDIA_FLAGS_DEFAULT, &size);
        } else {
            result = cupsGetDestMediaBySize(cups->http, cups->dest, cups->dinfo,
                                            width, length, CUPS_MEDIA_FLAGS_DEFAULT, &size);
        }
        if (result) {
            return cupsAddOption("media", size.media, num_options, options);
        }
    }
    ipp_attribute_t * attr = cups_printer_attr_default(cups, "media");
    const char * default_media = attr ? ippGetString(attr, 0, NULL) : "a4";
    return cupsAddOption("media", default_media, num_options, options);
}


static int cups_printer_get_media_source_opt(CupsPrinter * cups, char * job_options,
                                             int num_options, cups_option_t ** options) {
    g_autofree gchar * media_source = get_job_options(job_options, "media-source");
    ipp_attribute_t * attr = cups_printer_attr_supported(cups, CUPS_MEDIA_SOURCE);
    if (media_source) {
        int value = atoi(media_source);
        if (attr) {
            if (value < 0 || value >= ippGetCount(attr)) value = 0;
            num_options = cupsAddOption(CUPS_MEDIA_SOURCE, ippGetString(attr, value, NULL),
                                        num_options, options);
        }
    }
    return num_options;
}


static int cups_printer_get_media_type_opt(CupsPrinter * cups, char * job_options,
                                           int num_options, cups_option_t ** options) {
    g_autofree gchar * media_type = get_job_options(job_options, "media-type");
    ipp_attribute_t * attr = cups_printer_attr_supported(cups, CUPS_MEDIA_TYPE);
    if (media_type) {
        int value = atoi(media_type);
        if (attr) {
            if (value < 0 || value >= ippGetCount(attr)) value = 0;
            num_options = cupsAddOption(CUPS_MEDIA_TYPE, ippGetString(attr, value, NULL),
                                        num_options, options);
        }
    }
    return num_options;
}


static int cups_printer_job_options_to_cups(CupsPrinter * cups, char * job_options,
                                            cups_option_t ** options) {
    g_autofree gchar * sides = get_job_options(job_options, "sides"),
                     * copies = get_job_options(job_options, "copies"),
                     * nocollate = get_job_options(job_options, "noCollate"),
                     * resolution = get_job_options(job_options, "Resolution"),
                     * color = get_job_options(job_options, "color");

    *options = NULL;
    int num_options = 0;

    // Media size
    num_options = cups_printer_get_media_option(cups, job_options, num_options, options);
    num_options = cups_printer_get_media_source_opt(cups, job_options, num_options, options);
    num_options = cups_printer_get_media_type_opt(cups, job_options, num_options, options);
    if (sides) num_options = cupsAddOption("sides", sides, num_options, options);
    if (nocollate) num_options = cupsAddOption("Collate", "False", num_options, options);
    else num_options = cupsAddOption("Collate", "True", num_options, options);
    if (resolution) num_options = cupsAddOption("Resolution", resolution, num_options, options);
    if (copies) num_options = cupsAddOption("copies", copies, num_options, options);
    if (color) num_options = cupsAddOption(CUPS_PRINT_COLOR_MODE, "color", num_options, options);
    else num_options = cupsAddOption(CUPS_PRINT_COLOR_MODE, "monochrome", num_options, options);

    g_debug("%d options", num_options);
    return num_options;
}


int print_job(PrintJob * job) {
    g_autofree gchar * printer = get_job_options(job->options, "printer");
    g_autofree gchar * title = get_job_options(job->options, "title");
    int result = FALSE;

    if (printer) {
        CupsPrinter * cups = cups_printer_new(printer);
        if (cups->dinfo) {
            cups_option_t * options;
            int num_options = cups_printer_job_options_to_cups(cups, job->options, &options), i;
            cupsPrintFile2(cups->http, printer, job->name, title ? title : "",
                           num_options, options);
            for (i = 0; i < num_options; ++i) {
                g_debug("%s = %s", options[i].name, options[i].value);
            }
            result = TRUE;
        }
        cups_printer_delete(cups);
    }

    return result;
}
