#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <windows.h>
#include <wchar.h>
#include <winspool.h>
#include <poppler.h>
#include <cairo.h>
#include <cairo/cairo-win32.h>
#include "printclient.h"
#include "PPDGenerator.h"
#include "flexvdi-spice.h"


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
        flexvdiLog(L_WARN, "EnumPrinters failed");
        result = FALSE;
    }
    g_free(buffer);
    return result;
}


static PRINTER_INFO_2 * getPrinterInfo(HANDLE printer) {
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


typedef struct ClientPrinter {
    wchar_t * name;
    HANDLE handle;
    PRINTER_INFO_2 * pinfo;
} ClientPrinter;


static void deleteClientPrinter(ClientPrinter * printer) {
    g_free(printer->name);
    ClosePrinter(printer->handle);
    g_free(printer->pinfo);
}


static ClientPrinter * newClientPrinter(const char * nameUtf8) {
    ClientPrinter * printer = g_malloc0(sizeof(ClientPrinter));
    if (printer) {
        printer->name = g_utf8_to_utf16(nameUtf8, -1, NULL, NULL, NULL);
        if (OpenPrinter(printer->name, &printer->handle, NULL)) {
            printer->pinfo = getPrinterInfo(printer->handle);
            if (!printer->pinfo) {
                deleteClientPrinter(printer);
                printer = NULL;
            }
        } else {
            deleteClientPrinter(printer);
            printer = NULL;
        }
    }
    return printer;
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


char * getPPDFile(const char * printer) {
    PPDGenerator * ppd = newPPDGenerator(printer);
    char * result = NULL;

    ClientPrinter * cprinter = newClientPrinter(printer);
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


static LONG findBestPaperMatch(ClientPrinter * printer, double width, double length) {
    int i, numPaperSizes = getPrinterCap(printer, DC_PAPERNAMES, NULL);
    int bestMatch = 0;
    if (numPaperSizes > 0) {
        WORD paperIds[numPaperSizes];
        POINT paperSizes[numPaperSizes];
        getPrinterCap(printer, DC_PAPERS, (wchar_t *)paperIds);
        getPrinterCap(printer, DC_PAPERSIZE, (wchar_t *)paperSizes);
        double minError = -1;
        for (i = 0; i < numPaperSizes; ++i) {
            POINT p = paperSizes[i];
            double error = abs(width - p.x)*(length < p.y ? length : p.y) +
                           abs(length - p.y)*(width < p.x ? width : p.x);
            if (minError < 0.0 || minError > error) {
                minError = error;
                bestMatch = paperIds[i];
            }
        }
        if (minError > 1000000.0) bestMatch = 0;
        flexvdiLog(L_DEBUG, "Best match is paper %d with error %f\n", bestMatch, minError);
    }
    return bestMatch;
}


static void getMediaSizeOptionFromFile(ClientPrinter * printer, const char * pdf,
                                       DEVMODE * options) {
    PopplerDocument * document = NULL;
    GError *error = NULL;
    gchar * uri = g_filename_to_uri(pdf, NULL, &error);
    double width, length;
    if (uri) {
        document = poppler_document_new_from_file(uri, NULL, &error);
        g_free(uri);
    }
    if (document) {
        PopplerPage * page = poppler_document_get_page(document, 0);
        if (page) {
            poppler_page_get_size(page, &width, &length);
            g_object_unref(page);
            width *= 254.0 / 72.0;
            length *= 254.0 / 72.0;
            options->dmFields |= DM_PAPERSIZE | DM_SCALE | DM_ORIENTATION;
            options->dmScale = 100;
            options->dmOrientation = DMORIENT_PORTRAIT;
            options->dmPaperSize = findBestPaperMatch(printer, width, length);
            if (!options->dmPaperSize) {
                options->dmFields |= DM_PAPERLENGTH | DM_PAPERWIDTH;
                options->dmPaperWidth = width;
                options->dmPaperLength = length;
                flexvdiLog(L_INFO, "Pagesize set to %fx%f\n", width, length);
            } else {
                flexvdiLog(L_INFO, "Pagesize set to %d\n", options->dmPaperSize);
            }
        }
        g_object_unref(document);
    }
}


static void getMediaSourceOption(ClientPrinter * printer, char * jobOptions,
                                 DEVMODE * options) {
    char * media_source = getJobOption(jobOptions, "media-source");
    int value = 0;
    if (media_source) {
        value = atoi(media_source);
        g_free(media_source);
    }
    int numTrays = getPrinterCap(printer, DC_BINNAMES, NULL);
    if (value < 0 || value >= numTrays) value = 0;
    options->dmFields |= DM_DEFAULTSOURCE;
    options->dmDefaultSource = DMBIN_USER + value;
}


static void getMediaTypeOption(ClientPrinter * printer, char * jobOptions,
                               DEVMODE * options) {
    char * media_type = getJobOption(jobOptions, "media-type");
    int value = 0;
    if (media_type) {
        value = atoi(media_type);
        g_free(media_type);
    }
    int numMediaTypes = getPrinterCap(printer, DC_MEDIATYPENAMES, NULL);
    if (value < 0 || value >= numMediaTypes) value = 0;
    options->dmFields |= DM_MEDIATYPE;
    options->dmMediaType = DMMEDIA_USER + value;
}


static void getDuplexOption(char * jobOptions, DEVMODE * options) {
    char * sides = getJobOption(jobOptions, "sides");
    if (sides) {
        options->dmFields |= DM_DUPLEX;
        if (!strcmp(sides, "two-sided-short-edge"))
            options->dmDuplex = DMDUP_HORIZONTAL;
        else if (!strcmp(sides, "two-sided-long-edge"))
            options->dmDuplex = DMDUP_VERTICAL;
        else options->dmDuplex = DMDUP_SIMPLEX;
        g_free(sides);
    }
}


static void getCollateOption(char * jobOptions, DEVMODE * options) {
    char * nocollate = getJobOption(jobOptions, "noCollate");
    if (nocollate) {
        options->dmFields |= DM_COLLATE;
        options->dmCollate = DMCOLLATE_FALSE;
        g_free(nocollate);
    } else {
        char * collate = getJobOption(jobOptions, "Collate");
        if (collate) {
            options->dmFields |= DM_COLLATE;
            options->dmCollate = DMCOLLATE_TRUE;
            g_free(collate);
        }
    }
}


static void getResolutionOption(char * jobOptions, DEVMODE * options) {
    char * resolution = getJobOption(jobOptions, "Resolution");
    if (resolution) {
        int value = atoi(resolution);
        options->dmFields |= DM_PRINTQUALITY | DM_YRESOLUTION;
        options->dmPrintQuality = options->dmYResolution = value;
        g_free(resolution);
    }
}


static DEVMODE * jobOptionsToWindows(ClientPrinter * printer, const char * pdf,
                                     char * jobOptions) {
    char * copies = getJobOption(jobOptions, "copies"),
         * color = getJobOption(jobOptions, "color");
    DEVMODE * options = getDocumentProperties(printer);
    if (options) {
        getMediaSizeOptionFromFile(printer, pdf, options);
        getMediaSourceOption(printer, jobOptions, options);
        getMediaTypeOption(printer, jobOptions, options);
        getDuplexOption(jobOptions, options);
        getCollateOption(jobOptions, options);
        getResolutionOption(jobOptions, options);
        options->dmFields |= DM_COPIES;
        options->dmCopies = copies ? atoi(copies) : 1;
        options->dmFields |= DM_COLOR;
        options->dmColor = color ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;
    }
    g_free(color);
    g_free(copies);
    return options;
}


static int printFile(ClientPrinter * printer, const char * pdf,
                     const wchar_t * title, DEVMODE * options) {
    // TODO: get media size from pdf file
    PopplerDocument * document = NULL;
    GError *error = NULL;

    flexvdiLog(L_INFO, "Printing %s\n", pdf);
    gchar * uri = g_filename_to_uri(pdf, NULL, &error);
    if (uri) {
        document = poppler_document_new_from_file(uri, NULL, &error);
        g_free(uri);
    }
    if (!document) {
        flexvdiLog(L_ERROR, "%s\n", error->message);
        return FALSE;
    }

    HDC dc = CreateDC(NULL, printer->name, NULL, options);
    if (!dc) {
        flexvdiLog(L_ERROR, "Could not create DC\n");
        return FALSE;
    }


    cairo_status_t status;
    cairo_surface_t * surface = cairo_win32_printing_surface_create(dc);
    cairo_t * cr = cairo_create(surface);
    int result = TRUE;
    if ((status = cairo_surface_status(surface))) {
        flexvdiLog(L_ERROR, "Failed to start printing :%s\n", cairo_status_to_string(status));
        result = FALSE;
    } else if ((status = cairo_status(cr))) {
        flexvdiLog(L_ERROR, "Failed to start printing :%s\n", cairo_status_to_string(status));
        result = FALSE;
    } else {
        cairo_surface_set_fallback_resolution(surface, options->dmYResolution,
                                              options->dmYResolution);
        double scale, xOffset = 0.0, yOffset = 0.0;
        scale = options->dmYResolution / 72.0;
        getPaperMargins(printer, options->dmPaperSize, &xOffset, &yOffset);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -xOffset, -yOffset);
        DOCINFO docinfo = { sizeof (DOCINFO), title, NULL, NULL, 0 };
        StartDoc(dc, &docinfo);
        int i, num_pages = poppler_document_get_n_pages(document);
        for (i = 0; i < num_pages; i++) {
            StartPage(dc);
            PopplerPage * page = poppler_document_get_page (document, i);
            if (!page) {
                flexvdiLog(L_ERROR, "poppler fail: page not found\n");
                result = FALSE;
                break;
            }
            poppler_page_render_for_printing(page, cr);
            cairo_surface_show_page(surface);
            g_object_unref(page);
            EndPage(dc);
        }
        EndDoc(dc);
    }

    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
    g_object_unref(document);
    DeleteDC(dc);
    return result;
}


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "start %s", file);
    system(command);
}


void printJob(PrintJob * job) {
    char * printerUtf8 = getJobOption(job->options, "printer");
    ClientPrinter * printer = newClientPrinter(printerUtf8);
    char * titleUtf8 = getJobOption(job->options, "title");
    wchar_t * title = g_utf8_to_utf16(titleUtf8, -1, NULL, NULL, NULL);
    int result = FALSE;

    if (printer) {
        DEVMODE * options = jobOptionsToWindows(printer, job->name, job->options);
        if (options) {
            result = printFile(printer, job->name, title ? title : L"", options);
            g_free(options);
        }
        deleteClientPrinter(printer);
    }

    if (!result) openWithApp(job->name);
    g_free(title);
    g_free(printerUtf8);
    g_free(titleUtf8);
}
