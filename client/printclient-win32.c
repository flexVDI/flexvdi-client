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
    if (needed > 0) {
        gchar * buffer = g_malloc(needed);
        PRINTER_INFO_2A * pinfo = (PRINTER_INFO_2A *)buffer;
        if (EnumPrintersA(flags, NULL, 2, (LPBYTE)pinfo, needed, &needed, &returned)) {
            unsigned int i;
            for (i = 0; i < returned; i++) {
                *printerList = g_slist_append(*printerList, g_strdup(pinfo[i].pPrinterName));
            }
        } else result = 1;
        g_free(buffer);
    } else result = 1;
    return result;
}


char * getPPDFile(const char * printer) {
    PPDGenerator * ppd = newPPDGenerator(printer);
    char * result = NULL;
    // TODO: failures

    PRINTER_INFO_2A * pinfo = getPrinterInfo(printer);
    if (pinfo) {
        int i;
        ppdSetColor(ppd, getPrinterCap(pinfo, DC_COLORDEVICE, NULL));
        ppdSetDuplex(ppd, getPrinterCap(pinfo, DC_DUPLEX, NULL));
        // Resolution
        int numResolutions = getPrinterCap(pinfo, DC_ENUMRESOLUTIONS, NULL);
        LONG resolutions[numResolutions];
        getPrinterCap(pinfo, DC_ENUMRESOLUTIONS, (char *)resolutions);
        for (i = 0; i < numResolutions; ++i) {
            ppdAddResolution(ppd, resolutions[i]);
        }
        int defaultResolution = pinfo->pDevMode->dmPrintQuality;
        if (defaultResolution < 0) defaultResolution = pinfo->pDevMode->dmYResolution;
        ppdSetDefaultResolution(ppd, defaultResolution);
        // Paper
        int numPaperSizes = getPrinterCap(pinfo, DC_PAPERS, NULL);
        LONG papers[numPaperSizes];
        getPrinterCap(pinfo, DC_PAPERS, (char *)papers);
        for (i = 0; i < numPaperSizes; ++i) {
            ppdAddPaperSize(ppd, ppdPaperSizeFromWindows(papers[i]));
        }
        ppdSetDefaultPaperSize(ppd, ppdPaperSizeFromWindows(pinfo->pDevMode->dmPaperSize));
        g_free(pinfo);
        result = g_strdup(generatePPD(ppd));
    }

    deletePPDGenerator(ppd);
    return result;
}
