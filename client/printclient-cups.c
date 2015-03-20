#include <stdio.h>
#include <glib.h>
#include <cups/cups.h>
#include "printclient.h"
#include "PPDGenerator.h"
#include "flexvdi-spice.h"


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "xdg-open %s", file);
    // TODO: on Mac OS X, the command is 'open'
    system(command);
}


void flexvdiSpiceGetPrinterList(GSList ** printerList) {
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


static char * getPrettyName(const char * pwg) {
    gunichar dash = g_utf8_get_char("_");
    const char * start, * end;
    char * name, * prettyName, * i, * j;
    int len, capitalize;

    // Get middle component
    start = g_utf8_strchr(pwg, -1, dash);
    if (!start) start = pwg;
    end = g_utf8_strchr(++start, -1, dash);
    len = (end ? end - start : strlen(start)) + 1;
    name = (char *)g_malloc(len);
    g_strlcpy(name, start, len);
    // Turn _ into spaces
    i = name;
    while((i = g_utf8_strchr(i, -1, dash))) *i++ = ' ';
    // For each '-', remove it and capitalize next letter
    // First compute length
    len = 0;
    capitalize = TRUE;
    for (i = name; *i != '\0'; i = g_utf8_next_char(i)) {
        if (*i != '-') {
            if (capitalize)
                len += g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(i)), NULL);
            else
                len += g_unichar_to_utf8(g_utf8_get_char(i), NULL);
        }
        capitalize = *i == '-';
    }
    ++len;
    // Now write the name
    prettyName = (char *)g_malloc(len);
    capitalize = TRUE;
    for (i = name, j = prettyName; *i != '\0'; i = g_utf8_next_char(i)) {
        if (*i != '-') {
            if (capitalize)
                j += g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(i)), j);
            else
                j += g_unichar_to_utf8(g_utf8_get_char(i), j);
        }
        capitalize = *i == '-';
    }
    *j = '\0';
    return prettyName;
}


static void getPapers(PPDGenerator * ppd, CupsConnection * cups) {
    ipp_attribute_t * attr = ippIsSupported(cups, "media");
    if (attr) {
        int i = ippGetCount(attr) - 1;
        while (i >= 0) {
            pwg_media_t * size = pwgMediaForPWG(ippGetString(attr, i--, NULL));
            if (g_str_has_prefix(size->pwg, "custom")) continue;
            ppdAddPaperSize(ppd, size->ppd ? g_strdup(size->ppd) : getPrettyName(size->pwg),
                            size->width * 72 / 2540, size->length * 72 / 2540);
        }
        if ((attr = ippGetDefault(cups, "media"))) {
            pwg_media_t * size = pwgMediaForPWG(ippGetString(attr, 0, NULL));
            ppdSetDefaultPaperSize(ppd,
                size->ppd ? g_strdup(size->ppd) : getPrettyName(size->pwg));
        }
    }
    // TODO: get media margins
}


char * capitalizeFirst(const char * str) {
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
    CupsConnection * cups = openCups(printer);

    if (cups->dinfo) {
        ppdSetColor(ppd, ippHasOtherThan(cups, CUPS_PRINT_COLOR_MODE, "monochrome"));
        ppdSetDuplex(ppd, ippHasOtherThan(cups, CUPS_SIDES, "one-sided"));
        getResolutions(ppd, cups);
        getPapers(ppd, cups);
        getMediaSources(ppd, cups);
        getMediaTypes(ppd, cups);
        result = g_strdup(generatePPD(ppd));
    }

    closeCups(cups);
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
    flexvdiLog(L_DEBUG, "%d options", numOptions);
    return numOptions;
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
                flexvdiLog(L_DEBUG, "%s = %s", options[i].name, options[i].value);
            }
        } else openWithApp(job->name);
        closeCups(cups);
    } else openWithApp(job->name);

    g_free(printer);
    g_free(title);
}
