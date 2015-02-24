#include <stdio.h>
#include <glib.h>
#include <cups/cups.h>
#include "printclient.h"
#include "PPDGenerator.h"


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "xdg-open %s", file);
    // TODO: on Mac OS X, the command is 'open'
    system(command);
}


int flexvdiSpiceGetPrinterList(GSList ** printerList) {
    int i;
    cups_dest_t * dests, * dest;
    int numDests = cupsGetDests(&dests);
    char instance[256];
    *printerList = NULL;
    for (i = numDests, dest = dests; i > 0; --i, ++dest) {
        char * name;
        if (dest->instance) {
            g_snprintf(instance, 256, "%s/%s\n", dest->name, dest->instance);
            *printerList = g_slist_prepend(*printerList, g_strdup(instance));
        } else {
            *printerList = g_slist_prepend(*printerList, g_strdup(dest->name));
        }
    }
    cupsFreeDests(numDests, dests);
    return 0;
}


char * getPPDFile(const char * printer) {
    PPDGenerator * ppd = newPPDGenerator(printer);
    char * result = NULL;

    cups_dest_t * dests, * dest;
    cups_dinfo_t * dinfo;
    http_t * http;
    ipp_attribute_t * attr;
    int i, count;

    char * name = g_strdup(printer), * instance;
    if ((instance = g_strrstr(name, "/")) != NULL)
        *instance++ = '\0';
    int numDests = cupsGetDests(&dests);
    dest = cupsGetDest(name, instance, numDests, dests);
    g_free(name);
    http = cupsConnectDest(dest, CUPS_DEST_FLAGS_NONE, 30000, NULL, NULL, 0, NULL, NULL);
    dinfo = cupsCopyDestInfo(http, dest);

    if (dinfo) {
        if ((attr = cupsFindDestSupported(http, dest, dinfo, CUPS_PRINT_COLOR_MODE)) != NULL) {
            i = ippGetCount(attr) - 1;
            while (i >= 0 && !g_ascii_strcasecmp(ippGetString(attr, i, NULL), "monochrome")) --i;
            ppdSetColor(ppd, i >= 0);
        }

        if ((attr = cupsFindDestSupported(http, dest, dinfo, CUPS_SIDES)) != NULL) {
            i = ippGetCount(attr) - 1;
            while (i >= 0 && !g_ascii_strcasecmp(ippGetString(attr, i, NULL), "one-sided")) --i;
            ppdSetDuplex(ppd, i >= 0);
        }

        if ((attr = cupsFindDestSupported(http, dest, dinfo, "printer-resolution")) != NULL) {
            i = ippGetCount(attr) - 1;
            int yres;
            ipp_res_t units;
            while (i >= 0) {
                ppdAddResolution(ppd, ippGetResolution(attr, i--, &yres, &units));
            }
            if ((attr = cupsFindDestDefault(http, dest, dinfo, "printer-resolution")) != NULL) {
                ppdSetDefaultResolution(ppd, ippGetResolution(attr, 0, &yres, &units));
            }
        }

        cups_size_t size;
        i = cupsGetDestMediaCount(http, dest, dinfo, CUPS_MEDIA_FLAGS_DEFAULT) - 1;
        while (i >= 0) {
            cupsGetDestMediaByIndex(http, dest, dinfo, i--, CUPS_MEDIA_FLAGS_DEFAULT, &size);
            const char * media = pwgMediaForPWG(size.media)->ppd;
            ppdAddPaperSize(ppd, media ? media : size.media, size.width, size.length);
        }
        if (cupsGetDestMediaDefault(http, dest, dinfo, CUPS_MEDIA_FLAGS_DEFAULT, &size)) {
            const char * media = pwgMediaForPWG(size.media)->ppd;
            ppdSetDefaultPaperSize(ppd, media ? media : size.media);
        }
        // TODO: get media margins

        if ((attr = cupsFindDestSupported(http, dest, dinfo, CUPS_MEDIA_SOURCE)) != NULL) {
            i = ippGetCount(attr) - 1;
            while (i >= 0) {
                ppdAddTray(ppd, ippGetString(attr, i--, NULL));
            }
            if ((attr = cupsFindDestDefault(http, dest, dinfo, CUPS_MEDIA_SOURCE)) != NULL) {
                ppdSetDefaultTray(ppd, ippGetString(attr, 0, NULL));
            }
        }

        if ((attr = cupsFindDestSupported(http, dest, dinfo, CUPS_MEDIA_TYPE)) != NULL) {
            i = ippGetCount(attr) - 1;
            while (i >= 0) {
                ppdAddMediaType(ppd, ippGetString(attr, i--, NULL));
            }
            if ((attr = cupsFindDestDefault(http, dest, dinfo, CUPS_MEDIA_TYPE)) != NULL) {
                ppdSetDefaultMediaType(ppd, ippGetString(attr, 0, NULL));
            }
        }

        result = g_strdup(generatePPD(ppd));
    }

    cupsFreeDestInfo(dinfo);
    httpClose(http);
    cupsFreeDests(numDests, dests);
    deletePPDGenerator(ppd);
    return result;
}


void printJob(PrintJob * job) {
    openWithApp(job->name);
}
