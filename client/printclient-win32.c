#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <windows.h>
#include <winspool.h>
#include "printclient.h"
#include "PPDGenerator.h"


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "start %s", file);
    system(command);
}


static PRINTER_INFO_2A * getPrinterInfo(const char * printer) {
    PRINTER_INFO_2A * pinfo = NULL;
    HANDLE printerH;
    DWORD needed = 0;
    if (OpenPrinterA((char *)printer, &printerH, NULL)) {
        GetPrinterA(printerH, 2, NULL, 0, &needed);
        if (needed > 0) {
            pinfo = (PRINTER_INFO_2A *)g_malloc(needed);
            if (!GetPrinterA(printerH, 2, (LPBYTE)pinfo, needed, &needed)) {
                g_free(pinfo);
                pinfo = NULL;
            }
        }
        ClosePrinter(printerH);
    }
    return pinfo;
}


static int getPrinterCap(PRINTER_INFO_2A * pinfo, WORD cap, char * buffer) {
    return DeviceCapabilitiesA(pinfo->pPrinterName, pinfo->pPortName,
                               cap, buffer, pinfo->pDevMode);
}


int flexvdiSpiceGetPrinterList(GSList ** printerList) {
    int result = 0;
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    EnumPrintersA(flags, NULL, 2, NULL, 0, &needed, &returned);
    *printerList = NULL;
    if (needed > 0) {
        gchar * buffer = g_malloc(needed);
        PRINTER_INFO_2A * pinfo = (PRINTER_INFO_2A *)buffer;
        if (EnumPrintersA(flags, NULL, 2, (LPBYTE)pinfo, needed, &needed, &returned)) {
            int i;
            for (i = returned - 1; i >= 0; --i) {
                *printerList = g_slist_prepend(*printerList, g_strdup(pinfo[i].pPrinterName));
            }
        } else result = 1;
        g_free(buffer);
    } else result = 1;
    return result;
}


void getResolutions(PRINTER_INFO_2A * pinfo, PPDGenerator * ppd) {
    DEVMODEA * defaults = pinfo->pDevMode;
    int i, numResolutions = getPrinterCap(pinfo, DC_ENUMRESOLUTIONS, NULL);
    if (numResolutions > 0) {
        LONG resolutions[numResolutions * 2];
        getPrinterCap(pinfo, DC_ENUMRESOLUTIONS, (char *)resolutions);
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


void getPaper(PRINTER_INFO_2A * pinfo, PPDGenerator * ppd) {
    int i, numPaperSizes = getPrinterCap(pinfo, DC_PAPERNAMES, NULL);
    if (numPaperSizes > 0) {
        char paperNames[numPaperSizes * 64];
        POINT paperSizes[numPaperSizes];
        getPrinterCap(pinfo, DC_PAPERNAMES, paperNames);
        getPrinterCap(pinfo, DC_PAPERSIZE, (char *)paperSizes);
        for (i = 0; i < numPaperSizes; ++i) {
            paperNames[i*64 + 63] = '\0';
            if (strstr(&paperNames[i*64], "Custom")) continue;
            ppdAddPaperSize(ppd, &paperNames[i*64],
                            paperSizes[i].x * 72 / 254, paperSizes[i].y * 72 / 254);
        }
        ppdSetDefaultPaperSize(ppd, "A4"); // TODO
    }
}


void getMediaSources(PRINTER_INFO_2A * pinfo, PPDGenerator * ppd) {
    DEVMODEA * defaults = pinfo->pDevMode;
    int i, numTrays = getPrinterCap(pinfo, DC_BINNAMES, NULL);
    if (numTrays > 0) {
        char trayNames[numTrays * 24];
        getPrinterCap(pinfo, DC_BINNAMES, trayNames);
        for (i = 0; i < numTrays; ++i) {
            trayNames[i*24 + 23] = '\0';
            // TODO: Auto may be included more than once
            ppdAddTray(ppd, &trayNames[i*24]);
        }
        int defaultTray = 0;
        if (defaults->dmFields | DM_DEFAULTSOURCE) {
            defaultTray = defaults->dmDefaultSource - DMBIN_USER;
            if (defaultTray < 0 || defaultTray >= numTrays)
                defaultTray = 0;
        }
        ppdSetDefaultTray(ppd, &trayNames[defaultTray*24]);
    }
}


void getMediaTypes(PRINTER_INFO_2A * pinfo, PPDGenerator * ppd) {
    DEVMODEA * defaults = pinfo->pDevMode;
    int i, numMediaTypes = getPrinterCap(pinfo, DC_MEDIATYPENAMES, NULL);
    if (numMediaTypes > 0) {
        char mediaTypeNames[numMediaTypes * 64];
        getPrinterCap(pinfo, DC_MEDIATYPENAMES, mediaTypeNames);
        for (i = 0; i < numMediaTypes; ++i) {
            mediaTypeNames[i*64 + 63] = '\0';
            ppdAddMediaType(ppd, &mediaTypeNames[i*64]);
        }
        int defaultType = 0;
        if (defaults->dmFields | DM_MEDIATYPE) {
            pinfo->pDevMode->dmMediaType - DMMEDIA_USER;
            if (defaultType < 0 || defaultType >= numMediaTypes)
                defaultType = 0;
        }
        ppdSetDefaultMediaType(ppd, &mediaTypeNames[defaultType*24]);
    }
}


char * getPPDFile(const char * printer) {
    PPDGenerator * ppd = newPPDGenerator(printer);
    char * result = NULL;
    int i;

    PRINTER_INFO_2A * pinfo = getPrinterInfo(printer);
    DEVMODEA * defaults = pinfo->pDevMode;
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
