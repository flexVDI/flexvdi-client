/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include "printclient.h"
#include "PPDGenerator.h"
#include "flexvdi-port.h"


int flexvdiSpiceGetPrinterList(GSList ** printerList) {
    int i, result;
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
        result = TRUE;
    } else {
        g_warning("EnumPrinters failed");
        result = FALSE;
    }
    g_free(buffer);
    return result;
}


typedef struct ClientPrinter {
    wchar_t * name;
    HANDLE handle;
    PRINTER_INFO_2 * pinfo;
} ClientPrinter;


static void deleteClientPrinter(ClientPrinter * printer) {
    g_free(printer->name);
    ClosePrinter(printer->handle);
    g_free(printer->pinfo);
    g_free(printer);
}


static ClientPrinter * newClientPrinter(wchar_t * name) {
    DWORD needed = 0;
    ClientPrinter * printer = g_malloc0(sizeof(ClientPrinter));
    if (printer) {
        printer->name = name;
        if (OpenPrinter(printer->name, &printer->handle, NULL)) {
            GetPrinter(printer->handle, 2, NULL, 0, &needed);
            if (needed > 0) {
                printer->pinfo = g_malloc(needed);
                if (GetPrinter(printer->handle, 2, (LPBYTE) printer->pinfo, needed, &needed)) {
                    return printer;
                }
            }
        }
        deleteClientPrinter(printer);
    }
    return NULL;
}


static int getPrinterCap(ClientPrinter * printer, WORD cap, wchar_t * buffer) {
    return DeviceCapabilities(printer->name, printer->pinfo->pPortName,
                              cap, buffer, printer->pinfo->pDevMode);
}


static DEVMODE * getDocumentProperties(ClientPrinter * printer) {
    LONG size = DocumentProperties(NULL, printer->handle, printer->name, NULL, NULL, 0);
    DEVMODE * options = (DEVMODE *)g_malloc(size);
    if (DocumentProperties(NULL, printer->handle, printer->name,
                           options, NULL, DM_OUT_BUFFER) < 0) {
        g_free(options);
        options = NULL;
    }
    return options;
}


static int getDefaultResolution(ClientPrinter * printer) {
    int result = 0;
    if (printer->pinfo->pDevMode->dmFields | DM_PRINTQUALITY)
        result = printer->pinfo->pDevMode->dmPrintQuality;
    if (result <= 0 && printer->pinfo->pDevMode->dmFields | DM_YRESOLUTION)
        result = printer->pinfo->pDevMode->dmYResolution;
    return result;
}


static void getResolutions(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, numResolutions = getPrinterCap(printer, DC_ENUMRESOLUTIONS, NULL);
    if (numResolutions > 0) {
        LONG resolutions[numResolutions * 2];
        getPrinterCap(printer, DC_ENUMRESOLUTIONS, (wchar_t *)resolutions);
        for (i = 0; i < numResolutions; ++i) {
            ppdAddResolution(ppd, resolutions[i*2]);
        }
    }
    int defaultResolution = getDefaultResolution(printer);
    if (defaultResolution > 0)
        ppdSetDefaultResolution(ppd, defaultResolution);
}


static void getPaperMargins(ClientPrinter * printer, int paper, double * left, double * down) {
    DEVMODE * options = getDocumentProperties(printer);
    if (options) {
        options->dmFields |= DM_PAPERSIZE | DM_SCALE | DM_ORIENTATION |
                             DM_PRINTQUALITY | DM_YRESOLUTION;
        options->dmScale = 100;
        options->dmOrientation = DMORIENT_PORTRAIT;
        options->dmPaperSize = paper;
        int defaultResolution = getDefaultResolution(printer);
        if (defaultResolution <= 0) defaultResolution = 300;
        options->dmPrintQuality = options->dmYResolution = defaultResolution;
        HDC dc = CreateDC(NULL, printer->name, NULL, options);
        if (dc) {
            *left = GetDeviceCaps(dc, PHYSICALOFFSETX) * 72.0 / defaultResolution;
            *down = GetDeviceCaps(dc, PHYSICALOFFSETY) * 72.0 / defaultResolution;
        }
        DeleteDC(dc);
        g_free(options);
    }
}


static void getPaper(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, numPaperSizes = getPrinterCap(printer, DC_PAPERNAMES, NULL);
    if (numPaperSizes > 0) {
        wchar_t paperNames[numPaperSizes * 64];
        WORD paperIds[numPaperSizes];
        POINT paperSizes[numPaperSizes];
        getPrinterCap(printer, DC_PAPERNAMES, paperNames);
        getPrinterCap(printer, DC_PAPERS, (wchar_t *)paperIds);
        getPrinterCap(printer, DC_PAPERSIZE, (wchar_t *)paperSizes);
        for (i = 0; i < numPaperSizes; ++i) {
            char * paperNameUtf8 = g_utf16_to_utf8(&paperNames[i*64], 64, NULL, NULL, NULL);
            if (!strstr(paperNameUtf8, "Custom")) {
                double left = 0.0, bottom = 0.0;
                double width = paperSizes[i].x * 72.0 / 254.0;
                double length = paperSizes[i].y * 72.0 / 254.0;
                getPaperMargins(printer, paperIds[i], &left, &bottom);
                ppdAddPaperSize(ppd, paperNameUtf8, width, length,
                                left, bottom, width - left, length - bottom);
            }
            else g_free(paperNameUtf8);
        }
        ppdSetDefaultPaperSize(ppd, g_strdup("A4")); // TODO
    }
}


static void getMediaSources(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, numTrays = getPrinterCap(printer, DC_BINNAMES, NULL);
    if (numTrays > 0) {
        wchar_t trayNames[numTrays * 24];
        getPrinterCap(printer, DC_BINNAMES, trayNames);
        for (i = 0; i < numTrays; ++i) {
            // TODO: Auto may be included more than once
            char * trayNameUtf8 = g_utf16_to_utf8(&trayNames[i*24], 24, NULL, NULL, NULL);
            ppdAddTray(ppd, trayNameUtf8);
        }
        int defaultTray = 0;
        DEVMODE * defaults = printer->pinfo->pDevMode;
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


static void getMediaTypes(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, numMediaTypes = getPrinterCap(printer, DC_MEDIATYPENAMES, NULL);
    if (numMediaTypes > 0) {
        wchar_t mediaTypeNames[numMediaTypes * 64];
        getPrinterCap(printer, DC_MEDIATYPENAMES, mediaTypeNames);
        for (i = 0; i < numMediaTypes; ++i) {
            char * mediaTypeNameUtf8 = g_utf16_to_utf8(&mediaTypeNames[i*64], 64,
                                                       NULL, NULL, NULL);
            ppdAddMediaType(ppd, mediaTypeNameUtf8);
        }
        int defaultType = 0;
        DEVMODE * defaults = printer->pinfo->pDevMode;
        if (defaults->dmFields | DM_MEDIATYPE) {
            defaultType = defaults->dmMediaType - DMMEDIA_USER;
            if (defaultType < 0 || defaultType >= numMediaTypes)
                defaultType = 0;
        }
        char * mediaTypeNameUtf8 = g_utf16_to_utf8(&mediaTypeNames[defaultType*64], 64,
                                                   NULL, NULL, NULL);
        ppdSetDefaultMediaType(ppd, mediaTypeNameUtf8);
    }
}


static wchar_t * asUtf16(char * utf8) {
    wchar_t * result = g_utf8_to_utf16(utf8, -1, NULL, NULL, NULL);
    g_free(utf8);
    return result;
}


char * getPPDFile(const char * printer) {
    PPDGenerator * ppd = newPPDGenerator(printer);
    char * result = NULL;

    ClientPrinter * cprinter = newClientPrinter(asUtf16(g_strdup(printer)));
    if (cprinter) {
        ppdSetColor(ppd, getPrinterCap(cprinter, DC_COLORDEVICE, NULL));
        ppdSetDuplex(ppd, getPrinterCap(cprinter, DC_DUPLEX, NULL));
        getResolutions(cprinter, ppd);
        getPaper(cprinter, ppd);
        getMediaSources(cprinter, ppd);
        getMediaTypes(cprinter, ppd);
        deleteClientPrinter(cprinter);
        result = g_strdup(generatePPD(ppd));
    }

    deletePPDGenerator(ppd);
    return result;
}


void printJob(PrintJob * job) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    size_t envSize = strlen(job->options) + 13; // "JOBOPTIONS=...\0\0"
    char env[envSize];
    g_snprintf(env, envSize, "JOBOPTIONS=%s", job->options);
    env[envSize - 1] = '\0';
    wchar_t * cmdLine = asUtf16(g_strdup_printf("print-pdf \"%s\"", job->name));
    DWORD exitCode = 1;
    BOOL result = FALSE;
    if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, env, NULL, &si, &pi)) {
        // Wait until child process exits.
        WaitForSingleObject(pi.hProcess, INFINITE);
        result = GetExitCodeProcess(pi.hProcess, &exitCode) && !exitCode;
        g_debug("CreateProcess succeeded with code %d", exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else g_warning("CreateProcess failed");
    g_free(cmdLine);

    if (!result) {
        wchar_t * fileW = asUtf16(job->name);
        ShellExecute(NULL, L"open", fileW, NULL, NULL, SW_SHOWNORMAL);
        g_free(fileW);
    }
}

