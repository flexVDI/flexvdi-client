#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <math.h>
#include <cups/cups.h>
#include "printclient.h"
#include "PPDGenerator.h"
#include "flexvdi-port.h"


int flexvdi_get_printer_list(GSList ** printerList) {
    int i;
    cups_dest_t * dests, * dest;
    int numDests = cupsGetDests(&dests);
    *printerList = NULL;
    for (i = numDests, dest = dests; i > 0; --i, ++dest) {
        if (dest->instance) {
            char * fullInstance = g_strconcat(dest->name, "/", dest->instance, NULL);
            *printerList = g_slist_prepend(*printerList, fullInstance);
        } else {
            *printerList = g_slist_prepend(*printerList, g_strdup(dest->name));
        }
    }
    cupsFreeDests(numDests, dests);
    return TRUE;
}


typedef struct CupsConnection {
    cups_dest_t * dests, * dest;
    int numDests;
    cups_dinfo_t * dinfo;
    http_t * http;
} CupsConnection;


static CupsConnection * openCups(const char * printer) {
    CupsConnection * cups = (CupsConnection *)g_malloc0(sizeof(CupsConnection));
    char * name = g_strdup(printer), * instance;
    if ((instance = g_strrstr(name, "/")) != NULL)
        *instance++ = '\0';
    cups->numDests = cupsGetDests(&cups->dests);
    if (cups->dests) {
        cups->dest = cupsGetDest(name, instance, cups->numDests, cups->dests);
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
    g_free(name);
    return cups;
}


static void closeCups(CupsConnection * cups) {
    cupsFreeDestInfo(cups->dinfo);
    httpClose(cups->http);
    cupsFreeDests(cups->numDests, cups->dests);
    g_free(cups);
}


static ipp_attribute_t * ippIsSupported(CupsConnection * cups, const char * attrName) {
    return cupsFindDestSupported(cups->http, cups->dest, cups->dinfo, attrName);
}


static ipp_attribute_t * ippGetDefault(CupsConnection * cups, const char * attrName) {
    return cupsFindDestDefault(cups->http, cups->dest, cups->dinfo, attrName);
}


static int ippHasOtherThan(CupsConnection * cups, const char * attrName, const char * value) {
    ipp_attribute_t * attr = ippIsSupported(cups, attrName);
    if (attr) {
        int i = ippGetCount(attr) - 1, isEqual = 1;
        char * ivalue = g_utf8_casefold(value, -1);
        for (; i >= 0 && isEqual; --i) {
            char * cmpString = g_utf8_casefold(ippGetString(attr, i, NULL), -1);
            isEqual = !strcmp(cmpString, ivalue);
            g_free(cmpString);
        }
        g_free(ivalue);
        return i >= 0;
    }
    return FALSE;
}


static void getResolutions(PPDGenerator * ppd, CupsConnection * cups) {
    ipp_attribute_t * attr = ippIsSupported(cups, "printer-resolution");
    if (attr) {
        int i = ippGetCount(attr) - 1, yres;
        ipp_res_t units;
        while (i >= 0) {
            ppdAddResolution(ppd, ippGetResolution(attr, i--, &yres, &units));
        }
        if ((attr = ippGetDefault(cups, "printer-resolution"))) {
            ppdSetDefaultResolution(ppd, ippGetResolution(attr, 0, &yres, &units));
        }
    }
}


static double g_match_info_fetch_double(GMatchInfo * matchInfo, int index) {
    gchar * word = g_match_info_fetch(matchInfo, index);
    double result = strtod(word, NULL);
    g_free(word);
    return result;
}


static void getPapers(PPDGenerator * ppd, const char * printer) {
    const char * ppdFileName = cupsGetPPD(printer);
    char * ppdContents = NULL;
    if (g_file_get_contents(ppdFileName, &ppdContents, NULL, NULL)) {
        GRegex * pdRegex = g_regex_new("\\*PaperDimension\\s+([^:]+):\\s*\""
                                       "([0-9.]+)\\s+([0-9.]+)",
                                       0, 0, NULL);
        GMatchInfo * pdMatchInfo;
        g_regex_match(pdRegex, ppdContents, 0, &pdMatchInfo);
        while (g_match_info_matches(pdMatchInfo)) {
            gchar * paperName = g_match_info_fetch(pdMatchInfo, 1);
            double width = g_match_info_fetch_double(pdMatchInfo, 2);
            double length = g_match_info_fetch_double(pdMatchInfo, 3);
            double left = 0.0, bottom = 0.0, right = width, top = length;

            gchar * iaRegexStr = g_strconcat("\\*ImageableArea\\s+", paperName, ":\\s*\""
                                             "([0-9.]+)\\s+([0-9.]+)\\s+([0-9.]+)\\s+([0-9.]+)", NULL);
            GRegex * iaRegex = g_regex_new(iaRegexStr, 0, 0, NULL);
            GMatchInfo * iaMatchInfo;
            g_regex_match(iaRegex, ppdContents, 0, &iaMatchInfo);
            if (g_match_info_matches(iaMatchInfo)) {
                left = g_match_info_fetch_double(iaMatchInfo, 1);
                bottom = g_match_info_fetch_double(iaMatchInfo, 2);
                right = g_match_info_fetch_double(iaMatchInfo, 3);
                top = g_match_info_fetch_double(iaMatchInfo, 4);
            }
            g_match_info_free(iaMatchInfo);
            g_regex_unref(iaRegex);
            g_free(iaRegexStr);

            char * delim = strchr(paperName, '/');
            if (delim) {
                delim = g_strdup(delim + 1);
                g_free(paperName);
                paperName = delim;
            }
            ppdAddPaperSize(ppd, paperName, width, length,
                            left, bottom, right, top);
            g_match_info_next(pdMatchInfo, NULL);
        }
        g_match_info_free(pdMatchInfo);
        g_regex_unref(pdRegex);

        pdRegex = g_regex_new("\\*DefaultPaperDimension:\\s+(\\S+)", 0, 0, NULL);
        g_regex_match(pdRegex, ppdContents, 0, &pdMatchInfo);
        if (g_match_info_matches(pdMatchInfo)) {
            ppdSetDefaultPaperSize(ppd, g_match_info_fetch(pdMatchInfo, 1));
        }
        g_match_info_free(pdMatchInfo);
        g_regex_unref(pdRegex);
    }
    unlink(ppdFileName);
}


static char * capitalizeFirst(const char * str) {
    char tmp[7];
    tmp[g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(str)), tmp)] = '\0';
    return g_strconcat(tmp, g_utf8_next_char(str), NULL);
}


static void getMediaSources(PPDGenerator * ppd, CupsConnection * cups) {
    ipp_attribute_t * attr = ippIsSupported(cups, CUPS_MEDIA_SOURCE);
    if (attr) {
        int i, count = ippGetCount(attr);
        for (i = 0; i < count; ++i) {
            ppdAddTray(ppd, capitalizeFirst(ippGetString(attr, i, NULL)));
        }
        if ((attr = ippGetDefault(cups, CUPS_MEDIA_SOURCE))) {
            ppdSetDefaultTray(ppd, capitalizeFirst(ippGetString(attr, 0, NULL)));
        }
    }
}


static void getMediaTypes(PPDGenerator * ppd, CupsConnection * cups) {
    ipp_attribute_t * attr = ippIsSupported(cups, CUPS_MEDIA_TYPE);
    if (attr) {
        int i, count = ippGetCount(attr);
        for (i = 0; i < count; ++i) {
            ppdAddMediaType(ppd, capitalizeFirst(ippGetString(attr, i, NULL)));
        }
        if ((attr = ippGetDefault(cups, CUPS_MEDIA_TYPE))) {
            ppdSetDefaultMediaType(ppd, capitalizeFirst(ippGetString(attr, 0, NULL)));
        }
    }
}


char * getPPDFile(const char * printer) {
    char * result = NULL;
    PPDGenerator * ppd = newPPDGenerator(printer);
    if (ppd) {
        CupsConnection * cups = openCups(printer);
        if (cups->dinfo) {
            ppdSetColor(ppd, ippHasOtherThan(cups, CUPS_PRINT_COLOR_MODE, "monochrome"));
            ppdSetDuplex(ppd, ippHasOtherThan(cups, CUPS_SIDES, "one-sided"));
            getResolutions(ppd, cups);
            getPapers(ppd, printer);
            getMediaSources(ppd, cups);
            getMediaTypes(ppd, cups);
            result = g_strdup(generatePPD(ppd));
        }
        closeCups(cups);
    }
    deletePPDGenerator(ppd);
    return result;
}


static int getMediaOption(CupsConnection * cups, char * jobOptions,
                          int numOptions, cups_option_t ** options) {
    char * media = getJobOption(jobOptions, "media");
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
        g_free(media);
        if (result) {
            return cupsAddOption("media", size.media, numOptions, options);
        }
    }
    ipp_attribute_t * attr = ippGetDefault(cups, "media");
    const char * defaultMedia = attr ? ippGetString(attr, 0, NULL) : "a4";
    return cupsAddOption("media", defaultMedia, numOptions, options);
}


static int getMediaSourceOption(CupsConnection * cups, char * jobOptions,
                                int numOptions, cups_option_t ** options) {
    char * media_source = getJobOption(jobOptions, "media-source");
    ipp_attribute_t * attr = ippIsSupported(cups, CUPS_MEDIA_SOURCE);
    if (media_source) {
        int value = atoi(media_source);
        if (attr) {
            if (value < 0 || value >= ippGetCount(attr)) value = 0;
            numOptions = cupsAddOption(CUPS_MEDIA_SOURCE, ippGetString(attr, value, NULL),
                                       numOptions, options);
        }
        g_free(media_source);
    }
    return numOptions;
}


static int getMediaTypeOption(CupsConnection * cups, char * jobOptions,
                              int numOptions, cups_option_t ** options) {
    char * media_type = getJobOption(jobOptions, "media-type");
    ipp_attribute_t * attr = ippIsSupported(cups, CUPS_MEDIA_TYPE);
    if (media_type) {
        int value = atoi(media_type);
        if (attr) {
            if (value < 0 || value >= ippGetCount(attr)) value = 0;
            numOptions = cupsAddOption(CUPS_MEDIA_TYPE, ippGetString(attr, value, NULL),
                                       numOptions, options);
        }
        g_free(media_type);
    }
    return numOptions;
}


static int jobOptionsToCups(CupsConnection * cups, char * jobOptions,
                            cups_option_t ** options) {
    char * sides = getJobOption(jobOptions, "sides"),
         * copies = getJobOption(jobOptions, "copies"),
         * nocollate = getJobOption(jobOptions, "noCollate"),
         * resolution = getJobOption(jobOptions, "Resolution"),
         * color = getJobOption(jobOptions, "color");

    *options = NULL;
    int numOptions = 0;

    // Media size
    numOptions = getMediaOption(cups, jobOptions, numOptions, options);
    numOptions = getMediaSourceOption(cups, jobOptions, numOptions, options);
    numOptions = getMediaTypeOption(cups, jobOptions, numOptions, options);
    if (sides) numOptions = cupsAddOption("sides", sides, numOptions, options);
    if (nocollate) numOptions = cupsAddOption("Collate", "False", numOptions, options);
    else numOptions = cupsAddOption("Collate", "True", numOptions, options);
    if (resolution) numOptions = cupsAddOption("Resolution", resolution, numOptions, options);
    if (copies) numOptions = cupsAddOption("copies", copies, numOptions, options);
    if (color) numOptions = cupsAddOption(CUPS_PRINT_COLOR_MODE, "color", numOptions, options);
    else numOptions = cupsAddOption(CUPS_PRINT_COLOR_MODE, "monochrome", numOptions, options);

    g_free(sides);
    g_free(nocollate);
    g_free(resolution);
    g_free(copies);
    g_free(color);
    g_debug("%d options", numOptions);
    return numOptions;
}


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "xdg-open %s", file);
    // TODO: on Mac OS X, the command is 'open'
    system(command);
}


void printJob(PrintJob * job) {
    char * printer = getJobOption(job->options, "printer");
    char * title = getJobOption(job->options, "title");

    if (printer) {
        CupsConnection * cups = openCups(printer);
        if (cups->dinfo) {
            cups_option_t * options;
            int numOptions = jobOptionsToCups(cups, job->options, &options), i;
            cupsPrintFile2(cups->http, printer, job->name, title ? title : "",
                           numOptions, options);
            for (i = 0; i < numOptions; ++i) {
                g_debug("%s = %s", options[i].name, options[i].value);
            }
        } else openWithApp(job->name);
        closeCups(cups);
    } else openWithApp(job->name);

    g_free(printer);
    g_free(title);
}
