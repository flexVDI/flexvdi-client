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

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <poppler.h>
#include <cairo/cairo-win32.h>
#include "printclient-priv.h"
#include "PPDGenerator.h"
#include "flexvdi-port.h"


int flexvdi_get_printer_list(GSList ** printer_list) {
    int i, result;
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    *printer_list = NULL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    g_autofree BYTE * buffer = (BYTE *)g_malloc(needed);
    PRINTER_INFO_2 * pinfo = (PRINTER_INFO_2 *)buffer;
    if (needed && EnumPrinters(flags, NULL, 2, buffer, needed, &needed, &returned)) {
        for (i = returned - 1; i >= 0; --i) {
            *printer_list = g_slist_prepend(*printer_list,
                g_utf16_to_utf8(pinfo[i].pPrinterName, -1, NULL, NULL, NULL));
        }
        result = TRUE;
    } else {
        g_warning("EnumPrinters failed");
        result = FALSE;
    }
    return result;
}


typedef struct ClientPrinter {
    wchar_t * name;
    HANDLE handle;
    PRINTER_INFO_2 * pinfo;
} ClientPrinter;


static void client_printer_delete(ClientPrinter * printer) {
    g_free(printer->name);
    ClosePrinter(printer->handle);
    g_free(printer->pinfo);
    g_free(printer);
}


static ClientPrinter * client_printer_new(wchar_t * name) {
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
        client_printer_delete(printer);
    }
    return NULL;
}


static int client_printer_get_capabilities(ClientPrinter * printer, WORD cap, wchar_t * buffer) {
    return DeviceCapabilities(printer->name, printer->pinfo->pPortName,
                              cap, buffer, printer->pinfo->pDevMode);
}


static DEVMODE * client_printer_get_doc_props(ClientPrinter * printer) {
    LONG size = DocumentProperties(NULL, printer->handle, printer->name, NULL, NULL, 0);
    DEVMODE * options = (DEVMODE *)g_malloc(size);
    if (DocumentProperties(NULL, printer->handle, printer->name,
                           options, NULL, DM_OUT_BUFFER) < 0) {
        g_free(options);
        options = NULL;
    }
    return options;
}


static int client_printer_get_default_res(ClientPrinter * printer) {
    int result = 0;
    if (printer->pinfo->pDevMode->dmFields | DM_PRINTQUALITY)
        result = printer->pinfo->pDevMode->dmPrintQuality;
    if (result <= 0 && printer->pinfo->pDevMode->dmFields | DM_YRESOLUTION)
        result = printer->pinfo->pDevMode->dmYResolution;
    return result;
}


static void client_printer_get_resolutions(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, num_res = client_printer_get_capabilities(printer, DC_ENUMRESOLUTIONS, NULL);
    if (num_res > 0) {
        LONG resolutions[num_res * 2];
        client_printer_get_capabilities(printer, DC_ENUMRESOLUTIONS, (wchar_t *)resolutions);
        for (i = 0; i < num_res; ++i) {
            ppd_generator_add_resolution(ppd, resolutions[i*2]);
        }
    }
    int default_res = client_printer_get_default_res(printer);
    if (default_res > 0)
        ppd_generator_set_default_resolution(ppd, default_res);
}


static void client_printer_get_paper_margins(ClientPrinter * printer, int paper, double * left, double * down) {
    g_autofree DEVMODE * options = client_printer_get_doc_props(printer);
    if (options) {
        options->dmFields |= DM_PAPERSIZE | DM_SCALE | DM_ORIENTATION |
                             DM_PRINTQUALITY | DM_YRESOLUTION;
        options->dmScale = 100;
        options->dmOrientation = DMORIENT_PORTRAIT;
        options->dmPaperSize = paper;
        int default_res = client_printer_get_default_res(printer);
        if (default_res <= 0) default_res = 300;
        options->dmPrintQuality = options->dmYResolution = default_res;
        HDC dc = CreateDC(NULL, printer->name, NULL, options);
        if (dc) {
            *left = GetDeviceCaps(dc, PHYSICALOFFSETX) * 72.0 / default_res;
            *down = GetDeviceCaps(dc, PHYSICALOFFSETY) * 72.0 / default_res;
        }
        DeleteDC(dc);
    }
}


static void client_printer_get_paper(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, num_sizes = client_printer_get_capabilities(printer, DC_PAPERNAMES, NULL);
    if (num_sizes > 0) {
        wchar_t paper_names[num_sizes * 64];
        WORD paper_ids[num_sizes];
        POINT paper_sizes[num_sizes];
        client_printer_get_capabilities(printer, DC_PAPERNAMES, paper_names);
        client_printer_get_capabilities(printer, DC_PAPERS, (wchar_t *)paper_ids);
        client_printer_get_capabilities(printer, DC_PAPERSIZE, (wchar_t *)paper_sizes);
        for (i = 0; i < num_sizes; ++i) {
            char * paper_name_utf8 = g_utf16_to_utf8(&paper_names[i*64], 64, NULL, NULL, NULL);
            if (!strstr(paper_name_utf8, "Custom")) {
                double left = 0.0, bottom = 0.0;
                double width = paper_sizes[i].x * 72.0 / 254.0;
                double length = paper_sizes[i].y * 72.0 / 254.0;
                client_printer_get_paper_margins(printer, paper_ids[i], &left, &bottom);
                ppd_generator_add_paper_size(ppd, paper_name_utf8, width, length,
                                left, bottom, width - left, length - bottom);
            }
            else g_free(paper_name_utf8);
        }
        ppd_generator_set_default_paper_size(ppd, g_strdup("A4")); // TODO
    }
}


static void client_printer_get_media_sources(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, num_trays = client_printer_get_capabilities(printer, DC_BINNAMES, NULL);
    if (num_trays > 0) {
        wchar_t tray_names[num_trays * 24];
        client_printer_get_capabilities(printer, DC_BINNAMES, tray_names);
        for (i = 0; i < num_trays; ++i) {
            // TODO: Auto may be included more than once
            char * tray_name_utf8 = g_utf16_to_utf8(&tray_names[i*24], 24, NULL, NULL, NULL);
            ppd_generator_add_tray(ppd, tray_name_utf8);
        }
        int default_tray = 0;
        DEVMODE * defaults = printer->pinfo->pDevMode;
        if (defaults->dmFields | DM_DEFAULTSOURCE) {
            default_tray = defaults->dmDefaultSource - DMBIN_USER;
            if (default_tray < 0 || default_tray >= num_trays)
                default_tray = 0;
        }
        char * tray_name_utf8 = g_utf16_to_utf8(&tray_names[default_tray*24], 24,
                                                NULL, NULL, NULL);
        ppd_generator_set_default_tray(ppd, tray_name_utf8);
    }
}


static void client_printer_get_media_types(ClientPrinter * printer, PPDGenerator * ppd) {
    int i, num_media_types = client_printer_get_capabilities(printer, DC_MEDIATYPENAMES, NULL);
    if (num_media_types > 0) {
        wchar_t media_names[num_media_types * 64];
        client_printer_get_capabilities(printer, DC_MEDIATYPENAMES, media_names);
        for (i = 0; i < num_media_types; ++i) {
            char * media_name_utf8 = g_utf16_to_utf8(&media_names[i*64], 64,
                                                     NULL, NULL, NULL);
            ppd_generator_add_media_type(ppd, media_name_utf8);
        }
        int default_type = 0;
        DEVMODE * defaults = printer->pinfo->pDevMode;
        if (defaults->dmFields | DM_MEDIATYPE) {
            default_type = defaults->dmMediaType - DMMEDIA_USER;
            if (default_type < 0 || default_type >= num_media_types)
                default_type = 0;
        }
        char * media_name_utf8 = g_utf16_to_utf8(&media_names[default_type*64], 64,
                                                 NULL, NULL, NULL);
        ppd_generator_set_default_media_type(ppd, media_name_utf8);
    }
}


static LONG client_printer_best_paper_match(ClientPrinter * printer, double width, double length) {
    int i, numPaperSizes = client_printer_get_capabilities(printer, DC_PAPERNAMES, NULL);
    int bestMatch = 0;
    if (numPaperSizes > 0) {
        WORD paperIds[numPaperSizes];
        POINT paperSizes[numPaperSizes];
        client_printer_get_capabilities(printer, DC_PAPERS, (wchar_t *) paperIds);
        client_printer_get_capabilities(printer, DC_PAPERSIZE, (wchar_t *) paperSizes);
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
        g_debug("Best match is paper %d with error %f", bestMatch, minError);
    }
    return bestMatch;
}


static void client_printer_get_media_size_from_file(ClientPrinter * printer,
                                                    const char * pdf,
                                                    DEVMODE * options) {
    PopplerDocument * document = NULL;
    GError * error = NULL;
    g_autofree gchar * uri = g_filename_to_uri(pdf, NULL, &error);
    double width, length;

    if (uri) {
        document = poppler_document_new_from_file(uri, NULL, &error);
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
            options->dmPaperSize = client_printer_best_paper_match(printer, width, length);
            if (!options->dmPaperSize) {
                options->dmFields |= DM_PAPERLENGTH | DM_PAPERWIDTH;
                options->dmPaperWidth = width;
                options->dmPaperLength = length;
                g_info("Pagesize set to %fx%f", width, length);
            } else {
                g_info("Pagesize set to %d", options->dmPaperSize);
            }
        }

        g_object_unref(document);
    }
}


static wchar_t * as_utf16(char * utf8) {
    wchar_t * result = g_utf8_to_utf16(utf8, -1, NULL, NULL, NULL);
    g_free(utf8);
    return result;
}


char * get_ppd_file(const char * printer) {
    PPDGenerator * ppd = ppd_generator_new(printer);
    char * result = NULL;

    ClientPrinter * cprinter = client_printer_new(as_utf16(g_strdup(printer)));
    if (cprinter) {
        ppd_generator_set_color(ppd,
            client_printer_get_capabilities(cprinter, DC_COLORDEVICE, NULL));
        ppd_generator_set_duplex(ppd,
            client_printer_get_capabilities(cprinter, DC_DUPLEX, NULL));
        client_printer_get_resolutions(cprinter, ppd);
        client_printer_get_paper(cprinter, ppd);
        client_printer_get_media_sources(cprinter, ppd);
        client_printer_get_media_types(cprinter, ppd);
        client_printer_delete(cprinter);
        result = g_strdup(ppd_generator_run(ppd));
    }

    g_object_unref(ppd);
    return result;
}


static int job_option_get_int(char * options, const char * opName, int defaultValue) {
    g_autofree gchar * option = get_job_options(options, opName);
    return option ? atoi(option) : defaultValue;
}


static void client_printer_get_media_source_option(ClientPrinter * printer,
                                                   char * jobOptions,
                                                   DEVMODE * options) {
    int mediaSource = job_option_get_int(jobOptions, "media-source", 0);
    if (mediaSource < 0 || mediaSource >= client_printer_get_capabilities(printer, DC_BINNAMES, NULL)) {
        g_debug("Media source %d outside [0,%d)",
                   mediaSource, client_printer_get_capabilities(printer, DC_BINNAMES, NULL));
        mediaSource = 0;
    }
    options->dmFields |= DM_DEFAULTSOURCE;
    options->dmDefaultSource = DMBIN_USER + mediaSource;
}


static void client_printer_get_media_type_option(ClientPrinter * printer,
                                                 char * jobOptions,
                                                 DEVMODE * options) {
    int mediaType = job_option_get_int(jobOptions, "media-type", 0);
    if (mediaType < 0 || mediaType >= client_printer_get_capabilities(printer, DC_MEDIATYPENAMES, NULL)) {
        g_debug("Media type %d outside [0,%d)",
                   mediaType, client_printer_get_capabilities(printer, DC_MEDIATYPENAMES, NULL));
        mediaType = 0;
    }
    options->dmFields |= DM_MEDIATYPE;
    options->dmMediaType = DMMEDIA_USER + mediaType;
}


static void client_printer_get_duplex_option(char * jobOptions, DEVMODE * options) {
    g_autofree gchar * sides = get_job_options(jobOptions, "sides");
    if (sides) {
        options->dmFields |= DM_DUPLEX;
        if (!strcmp(sides, "two-sided-short-edge")) {
            options->dmDuplex = DMDUP_HORIZONTAL;
        } else if (!strcmp(sides, "two-sided-long-edge")) {
            options->dmDuplex = DMDUP_VERTICAL;
        } else {
            options->dmDuplex = DMDUP_SIMPLEX;
        }
    }
}


static void client_printer_get_collate_option(char * jobOptions, DEVMODE * options) {
    g_autofree gchar * nocollate = get_job_options(jobOptions, "noCollate");
    if (nocollate) {
        options->dmFields |= DM_COLLATE;
        options->dmCollate = DMCOLLATE_FALSE;
    } else {
        g_autofree gchar * collate = get_job_options(jobOptions, "Collate");
        if (collate) {
            options->dmFields |= DM_COLLATE;
            options->dmCollate = DMCOLLATE_TRUE;
        }
    }
}


static void client_printer_get_resolution_option(char * jobOptions, DEVMODE * options) {
    int resolution = job_option_get_int(jobOptions, "Resolution", 0);
    if (resolution) {
        options->dmFields |= DM_PRINTQUALITY | DM_YRESOLUTION;
        options->dmPrintQuality = options->dmYResolution = resolution;
    }
}


static DEVMODE * job_options_to_DevMode(ClientPrinter * printer, const char * pdf,
                                        char * jobOptions) {
    DEVMODE * options = client_printer_get_doc_props(printer);
    if (options) {
        client_printer_get_media_size_from_file(printer, pdf, options);
        client_printer_get_media_source_option(printer, jobOptions, options);
        client_printer_get_media_type_option(printer, jobOptions, options);
        client_printer_get_duplex_option(jobOptions, options);
        client_printer_get_collate_option(jobOptions, options);
        client_printer_get_resolution_option(jobOptions, options);
        options->dmFields |= DM_COPIES;
        options->dmCopies = job_option_get_int(jobOptions, "copies", 1);
        g_autofree gchar * color = get_job_options(jobOptions, "color");
        options->dmFields |= DM_COLOR;
        options->dmColor = color ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;
    }
    return options;
}


static void print_file(ClientPrinter * printer, const char * pdf,
                       const wchar_t * title, DEVMODE * options) {
    PopplerDocument * document = NULL;
    GError * error = NULL;

    g_info("Printing %s", pdf);
    g_autofree gchar * uri = g_filename_to_uri(pdf, NULL, &error);
    if (uri) {
        document = poppler_document_new_from_file(uri, NULL, &error);
    }
    if (!document) {
        g_error("%s", error->message);
        return;
    }

    HDC dc = CreateDC(NULL, printer->name, NULL, options);
    if (!dc) {
        g_error("Could not create DC");
        return;
    }

    cairo_status_t status;
    cairo_surface_t * surface = cairo_win32_printing_surface_create(dc);
    cairo_t * cr = cairo_create(surface);
    if ((status = cairo_surface_status(surface))) {
        g_error("Failed to start printing: %s", cairo_status_to_string(status));
    } else if ((status = cairo_status(cr))) {
        g_error("Failed to start printing: %s", cairo_status_to_string(status));
    } else {
        cairo_surface_set_fallback_resolution(surface, options->dmYResolution,
                                              options->dmYResolution);
        double scale, xOffset = 0.0, yOffset = 0.0;
        scale = options->dmYResolution / 72.0;
        client_printer_get_paper_margins(printer, options->dmPaperSize, &xOffset, &yOffset);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -xOffset, -yOffset);
        DOCINFO docinfo = { sizeof(DOCINFO), title, NULL, NULL, 0 };
        StartDoc(dc, &docinfo);
        int i, num_pages = poppler_document_get_n_pages(document);
        for (i = 0; i < num_pages; i++) {
            StartPage(dc);
            PopplerPage * page = poppler_document_get_page(document, i);
            if (!page) {
                g_error("poppler fail: page not found");
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
}


int print_job(PrintJob * job) {
    g_debug("Printing file %s with options %s", job->name, job->options);
    gchar * printer_name = get_job_options(job->options, "printer");

    if (printer_name) {
        ClientPrinter * printer = client_printer_new(as_utf16(printer_name));

        if (printer) {
            DEVMODE * dm = job_options_to_DevMode(printer, job->name, job->options);
            if (dm) {
                g_autofree wchar_t * title = as_utf16(get_job_options(job->options, "title"));
                print_file(printer, job->name, title ? title : L"", dm);
            }

            client_printer_delete(printer);
            return TRUE;
        }
    }

    return FALSE;
}
