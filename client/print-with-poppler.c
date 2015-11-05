/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include <poppler.h>
#include <cairo/cairo-win32.h>


static void flexvdiLog(GLogLevelFlags level, const char * format, ...) {
    va_list args;
    va_start(args, format);
    g_logv(G_LOG_DOMAIN, level, format, args);
    va_end(args);
}


typedef struct ClientPrinter {
    wchar_t * name;
    HANDLE hnd;
    PRINTER_INFO_2 * pinfo;
} ClientPrinter;


static void deleteClientPrinter(ClientPrinter * printer) {
    g_free(printer->name);
    ClosePrinter(printer->hnd);
    g_free(printer->pinfo);
    g_free(printer);
}


static ClientPrinter * newClientPrinter(wchar_t * name) {
    DWORD needed = 0;
    ClientPrinter * printer = g_malloc0(sizeof(ClientPrinter));
    if (printer) {
        printer->name = name;
        if (OpenPrinter(printer->name, &printer->hnd, NULL)) {
            GetPrinter(printer, 2, NULL, 0, &needed);
            if (needed > 0) {
                printer->pinfo = g_malloc(needed);
                if (GetPrinter(printer, 2, (LPBYTE) printer->pinfo, needed, &needed)) {
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
    LONG size = DocumentProperties(NULL, printer->hnd, printer->name, NULL, NULL, 0);
    DEVMODE * dm = g_malloc(size);
    if (DocumentProperties(NULL, printer->hnd, printer->name, dm, NULL, DM_OUT_BUFFER) < 0) {
        g_free(dm);
        dm = NULL;
    }
    return dm;
}


static int getDefaultResolution(ClientPrinter * printer) {
    int result = 0;
    if (printer->pinfo->pDevMode->dmFields | DM_PRINTQUALITY) {
        result = printer->pinfo->pDevMode->dmPrintQuality;
    }
    if (result <= 0 && printer->pinfo->pDevMode->dmFields | DM_YRESOLUTION) {
        result = printer->pinfo->pDevMode->dmYResolution;
    }
    return result;
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
        if (defaultResolution <= 0) {
            defaultResolution = 300;
        }
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


static LONG findBestPaperMatch(ClientPrinter * printer, double width, double length) {
    int i, numPaperSizes = getPrinterCap(printer, DC_PAPERNAMES, NULL);
    int bestMatch = 0;
    if (numPaperSizes > 0) {
        WORD paperIds[numPaperSizes];
        POINT paperSizes[numPaperSizes];
        getPrinterCap(printer, DC_PAPERS, (wchar_t *) paperIds);
        getPrinterCap(printer, DC_PAPERSIZE, (wchar_t *) paperSizes);
        double minError = -1;
        for (i = 0; i < numPaperSizes; ++i) {
            POINT p = paperSizes[i];
            double error = abs(width - p.x) * (length < p.y ? length : p.y) +
                           abs(length - p.y) * (width < p.x ? width : p.x);
            if (minError < 0.0 || minError > error) {
                minError = error;
                bestMatch = paperIds[i];
            }
        }
        if (minError > 1000000.0) {
            bestMatch = 0;
        }
        flexvdiLog(G_LOG_LEVEL_DEBUG, "Best match is paper %d with error %f\n", bestMatch, minError);
    }
    return bestMatch;
}


static void getMediaSizeOptionFromFile(ClientPrinter * printer, const char * pdf,
                                       DEVMODE * options) {
    PopplerDocument * document = NULL;
    GError * error = NULL;
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
                flexvdiLog(G_LOG_LEVEL_INFO, "Pagesize set to %fx%f\n", width, length);
            } else {
                flexvdiLog(G_LOG_LEVEL_INFO, "Pagesize set to %d\n", options->dmPaperSize);
            }
        }
        g_object_unref(document);
    }
}


static char * getJobOption(char * options, const char * opName) {
    gunichar equalSign = g_utf8_get_char("="),
             space = g_utf8_get_char(" "),
             quotes = g_utf8_get_char("\"");
    int opLen = strlen(opName);
    gunichar nextChar;
    char * opPos = options, * opEnd = options + strlen(options);
    do {
        opPos = strstr(opPos, opName);
        if (!opPos) {
            return NULL;
        }
        opPos += opLen;
        nextChar = g_utf8_get_char(opPos);
    } while ((opPos > options + opLen && g_utf8_get_char(opPos - opLen - 1) != space) ||
             (opPos < opEnd && nextChar != equalSign && nextChar != space));
    int valueLen = 0;
    if (nextChar == equalSign) {
        opPos = g_utf8_next_char(opPos);
        gunichar delimiter = space;
        if (g_utf8_get_char(opPos) == quotes) {
            opPos = g_utf8_next_char(opPos);
            delimiter = quotes;
        }
        char * end = g_utf8_strchr(opPos, -1, delimiter);
        if (!end) {
            end = opEnd;
        }
        valueLen = end - opPos;
    }
    char * result = g_malloc(valueLen + 1);
    memcpy(result, opPos, valueLen);
    result[valueLen] = '\0';
    return result;
}


static int getIntJobOption(char * options, const char * opName, int defaultValue) {
    char * option = getJobOption(options, opName);
    int value = option ? atoi(option) : defaultValue;
    g_free(option);
    return value;
}


static void getMediaSourceOption(ClientPrinter * printer, char * jobOptions,
                                 DEVMODE * options) {
    int mediaSource = getIntJobOption(jobOptions, "media-source", 0);
    if (mediaSource < 0 || mediaSource >= getPrinterCap(printer, DC_BINNAMES, NULL)) {
        mediaSource = 0;
    }
    options->dmFields |= DM_DEFAULTSOURCE;
    options->dmDefaultSource = DMBIN_USER + mediaSource;
}


static void getMediaTypeOption(ClientPrinter * printer, char * jobOptions,
                               DEVMODE * options) {
    int mediaType = getIntJobOption(jobOptions, "media-type", 0);
    if (mediaType < 0 || mediaType >= getPrinterCap(printer, DC_MEDIATYPENAMES, NULL)) {
        mediaType = 0;
    }
    options->dmFields |= DM_MEDIATYPE;
    options->dmMediaType = DMMEDIA_USER + mediaType;
}


static void getDuplexOption(char * jobOptions, DEVMODE * options) {
    char * sides = getJobOption(jobOptions, "sides");
    if (sides) {
        options->dmFields |= DM_DUPLEX;
        if (!strcmp(sides, "two-sided-short-edge")) {
            options->dmDuplex = DMDUP_HORIZONTAL;
        } else if (!strcmp(sides, "two-sided-long-edge")) {
            options->dmDuplex = DMDUP_VERTICAL;
        } else {
            options->dmDuplex = DMDUP_SIMPLEX;
        }
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
    int resolution = getIntJobOption(jobOptions, "Resolution", 0);
    if (resolution) {
        options->dmFields |= DM_PRINTQUALITY | DM_YRESOLUTION;
        options->dmPrintQuality = options->dmYResolution = resolution;
    }
}


static DEVMODE * jobOptionsToDevMode(ClientPrinter * printer, const char * pdf,
                                     char * jobOptions) {
    char * color;
    DEVMODE * options = getDocumentProperties(printer);
    if (options) {
        getMediaSizeOptionFromFile(printer, pdf, options);
        getMediaSourceOption(printer, jobOptions, options);
        getMediaTypeOption(printer, jobOptions, options);
        getDuplexOption(jobOptions, options);
        getCollateOption(jobOptions, options);
        getResolutionOption(jobOptions, options);
        options->dmFields |= DM_COPIES;
        options->dmCopies = getIntJobOption(jobOptions, "copies", 1);
        color = getJobOption(jobOptions, "color");
        options->dmFields |= DM_COLOR;
        options->dmColor = color ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;
        g_free(color);
    }
    return options;
}


static int printFile(ClientPrinter * printer, const char * pdf,
                     const wchar_t * title, DEVMODE * options) {
    PopplerDocument * document = NULL;
    GError * error = NULL;

    flexvdiLog(G_LOG_LEVEL_INFO, "Printing %s\n", pdf);
    gchar * uri = g_filename_to_uri(pdf, NULL, &error);
    if (uri) {
        document = poppler_document_new_from_file(uri, NULL, &error);
        g_free(uri);
    }
    if (!document) {
        flexvdiLog(G_LOG_LEVEL_ERROR, "%s\n", error->message);
        return FALSE;
    }

    HDC dc = CreateDC(NULL, printer->name, NULL, options);
    if (!dc) {
        flexvdiLog(G_LOG_LEVEL_ERROR, "Could not create DC\n");
        return FALSE;
    }

    cairo_status_t status;
    cairo_surface_t * surface = cairo_win32_printing_surface_create(dc);
    cairo_t * cr = cairo_create(surface);
    int result = TRUE;
    if ((status = cairo_surface_status(surface))) {
        flexvdiLog(G_LOG_LEVEL_ERROR, "Failed to start printing :%s\n", cairo_status_to_string(status));
        result = FALSE;
    } else if ((status = cairo_status(cr))) {
        flexvdiLog(G_LOG_LEVEL_ERROR, "Failed to start printing :%s\n", cairo_status_to_string(status));
        result = FALSE;
    } else {
        cairo_surface_set_fallback_resolution(surface, options->dmYResolution,
                                              options->dmYResolution);
        double scale, xOffset = 0.0, yOffset = 0.0;
        scale = options->dmYResolution / 72.0;
        getPaperMargins(printer, options->dmPaperSize, &xOffset, &yOffset);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -xOffset, -yOffset);
        DOCINFO docinfo = { sizeof(DOCINFO), title, NULL, NULL, 0 };
        StartDoc(dc, &docinfo);
        int i, num_pages = poppler_document_get_n_pages(document);
        for (i = 0; i < num_pages; i++) {
            StartPage(dc);
            PopplerPage * page = poppler_document_get_page(document, i);
            if (!page) {
                flexvdiLog(G_LOG_LEVEL_ERROR, "poppler fail: page not found\n");
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


static wchar_t * asUtf16(char * utf8) {
    wchar_t * result = g_utf8_to_utf16(utf8, -1, NULL, NULL, NULL);
    g_free(utf8);
    return result;
}


int main(int argc, char * argv[]) {
    if (argc != 3)
        return 1;
    char * name = argv[1], * options = argv[2];

    ClientPrinter * printer = newClientPrinter(asUtf16(getJobOption(options, "printer")));
    if (!printer)
        return 1;

    int result = FALSE;
    DEVMODE * dm = jobOptionsToDevMode(printer, name, options);
    if (dm) {
        wchar_t * title = asUtf16(getJobOption(options, "title"));
        result = printFile(printer, name, title ? title : L"", dm);
        g_free(title);
        g_free(dm);
    }
    deleteClientPrinter(printer);

    return result ? 0 : 1;
}
