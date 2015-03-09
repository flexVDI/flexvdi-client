#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <windows.h>
#include <wchar.h>
#include <winspool.h>
#include "printclient.h"
#include "PPDGenerator.h"
#include "flexvdi-spice.h"


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "start %s", file);
    system(command);
}


static PRINTER_INFO_2 * getPrinterHandleInfo(HANDLE printer) {
    PRINTER_INFO_2 * pinfo = NULL;
    DWORD needed = 0;
    GetPrinter(printer, 2, NULL, 0, &needed);
    if (needed > 0) {
        pinfo = (PRINTER_INFO_2 *)g_malloc(needed);
        if (!GetPrinter(printer, 2, (LPBYTE)pinfo, needed, &needed)) {
            g_free(pinfo);
            pinfo = NULL;
        }
    }
    return pinfo;
}


static PRINTER_INFO_2 * getPrinterInfo(const char * printerUtf8) {
    PRINTER_INFO_2 * pinfo = NULL;
    HANDLE printerH;
    wchar_t * printer = g_utf8_to_utf16(printerUtf8, -1, NULL, NULL, NULL);
    if (OpenPrinter(printer, &printerH, NULL)) {
        pinfo = getPrinterHandleInfo(printerH);
        ClosePrinter(printerH);
    }
    g_free(printer);
    return pinfo;
}


static int getPrinterCap(PRINTER_INFO_2 * pinfo, WORD cap, wchar_t * buffer) {
    return DeviceCapabilities(pinfo->pPrinterName, pinfo->pPortName,
                              cap, buffer, pinfo->pDevMode);
}


int flexvdiSpiceGetPrinterList(GSList ** printerList) {
    int result = 0, i;
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    *printerList = NULL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    BYTE * buffer = (BYTE *)g_malloc(needed);
    PRINTER_INFO_2 * pinfo = (PRINTER_INFO_2 *)buffer;
    if (needed && EnumPrinters(flags, NULL, 2, buffer, needed, &needed, &returned)) {
        for (i = returned - 1; i >= 0; --i) {
            *printerList = g_slist_prepend(*printerList,
                g_utf16_to_utf8(pinfo[i].pPrinterName, -1, NULL, NULL, NULL));
        }
    } else result = 1;
    g_free(buffer);
    return result;
}


static void getResolutions(PRINTER_INFO_2 * pinfo, PPDGenerator * ppd) {
    DEVMODE * defaults = pinfo->pDevMode;
    int i, numResolutions = getPrinterCap(pinfo, DC_ENUMRESOLUTIONS, NULL);
    if (numResolutions > 0) {
        LONG resolutions[numResolutions * 2];
        getPrinterCap(pinfo, DC_ENUMRESOLUTIONS, (wchar_t *)resolutions);
        for (i = 0; i < numResolutions; ++i) {
            ppdAddResolution(ppd, resolutions[i*2]);
        }
    }
    int defaultResolution = 0;
    if (defaults->dmFields | DM_PRINTQUALITY)
        defaultResolution = defaults->dmPrintQuality;
    if (defaultResolution <= 0 && defaults->dmFields | DM_YRESOLUTION)
        defaultResolution = defaults->dmYResolution;
    if (defaultResolution > 0)
        ppdSetDefaultResolution(ppd, defaultResolution);
}


static void getPaper(PRINTER_INFO_2 * pinfo, PPDGenerator * ppd) {
    int i, numPaperSizes = getPrinterCap(pinfo, DC_PAPERNAMES, NULL);
    if (numPaperSizes > 0) {
        wchar_t paperNames[numPaperSizes * 64];
        POINT paperSizes[numPaperSizes];
        getPrinterCap(pinfo, DC_PAPERNAMES, paperNames);
        getPrinterCap(pinfo, DC_PAPERSIZE, (wchar_t *)paperSizes);
        for (i = 0; i < numPaperSizes; ++i) {
            char * paperNameUtf8 = g_utf16_to_utf8(&paperNames[i*64], 64, NULL, NULL, NULL);
            if (!strstr(paperNameUtf8, "Custom")) {
                ppdAddPaperSize(ppd, paperNameUtf8,
                                paperSizes[i].x * 72 / 254, paperSizes[i].y * 72 / 254);
            }
            else g_free(paperNameUtf8);
        }
        ppdSetDefaultPaperSize(ppd, g_strdup("A4")); // TODO
    }
}


static void getMediaSources(PRINTER_INFO_2 * pinfo, PPDGenerator * ppd) {
    DEVMODE * defaults = pinfo->pDevMode;
    int i, numTrays = getPrinterCap(pinfo, DC_BINNAMES, NULL);
    if (numTrays > 0) {
        wchar_t trayNames[numTrays * 24];
        getPrinterCap(pinfo, DC_BINNAMES, trayNames);
        for (i = 0; i < numTrays; ++i) {
            // TODO: Auto may be included more than once
            char * trayNameUtf8 = g_utf16_to_utf8(&trayNames[i*24], 24, NULL, NULL, NULL);
            ppdAddTray(ppd, trayNameUtf8);
        }
        int defaultTray = 0;
        if (defaults->dmFields | DM_DEFAULTSOURCE) {
            defaultTray = defaults->dmDefaultSource - DMBIN_USER;
            if (defaultTray < 0 || defaultTray >= numTrays)
                defaultTray = 0;
        }
        char * trayNameUtf8 = g_utf16_to_utf8(&trayNames[defaultTray*24], 24,
                                              NULL, NULL, NULL);
        ppdSetDefaultTray(ppd, trayNameUtf8);
    }
}


static void getMediaTypes(PRINTER_INFO_2 * pinfo, PPDGenerator * ppd) {
    DEVMODE * defaults = pinfo->pDevMode;
    int i, numMediaTypes = getPrinterCap(pinfo, DC_MEDIATYPENAMES, NULL);
    if (numMediaTypes > 0) {
        wchar_t mediaTypeNames[numMediaTypes * 64];
        getPrinterCap(pinfo, DC_MEDIATYPENAMES, mediaTypeNames);
        for (i = 0; i < numMediaTypes; ++i) {
            char * mediaTypeNameUtf8 = g_utf16_to_utf8(&mediaTypeNames[i*64], 64,
                                                       NULL, NULL, NULL);
            ppdAddMediaType(ppd, mediaTypeNameUtf8);
        }
        int defaultType = 0;
        if (defaults->dmFields | DM_MEDIATYPE) {
            defaultType = pinfo->pDevMode->dmMediaType - DMMEDIA_USER;
            if (defaultType < 0 || defaultType >= numMediaTypes)
                defaultType = 0;
        }
        char * mediaTypeNameUtf8 = g_utf16_to_utf8(&mediaTypeNames[defaultType*64], 64,
                                                    NULL, NULL, NULL);
        ppdSetDefaultMediaType(ppd, mediaTypeNameUtf8);
    }
}


char * getPPDFile(const char * printer) {
    PPDGenerator * ppd = newPPDGenerator(printer);
    char * result = NULL;

    PRINTER_INFO_2 * pinfo = getPrinterInfo(printer);
    if (pinfo) {
        ppdSetColor(ppd, getPrinterCap(pinfo, DC_COLORDEVICE, NULL));
        ppdSetDuplex(ppd, getPrinterCap(pinfo, DC_DUPLEX, NULL));
        getResolutions(pinfo, ppd);
        getPaper(pinfo, ppd);
        getMediaSources(pinfo, ppd);
        getMediaTypes(pinfo, ppd);
        g_free(pinfo);
        result = g_strdup(generatePPD(ppd));
    }

    deletePPDGenerator(ppd);
    return result;
}


void printJob(PrintJob * job) {
    openWithApp(job->name);
}
