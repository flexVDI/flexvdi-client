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


int flexvdi_get_printer_list(GSList ** printer_list) {
    int i, result;
    DWORD needed = 0, returned = 0;
    DWORD flags = PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL;
    *printer_list = NULL;
    EnumPrinters(flags, NULL, 2, NULL, 0, &needed, &returned);
    BYTE * buffer = (BYTE *)g_malloc(needed);
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
    g_free(buffer);
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
    DEVMODE * options = client_printer_get_doc_props(printer);
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
        g_free(options);
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
        ppd_generator_set_color(ppd, client_printer_get_capabilities(cprinter, DC_COLORDEVICE, NULL));
        ppd_generator_set_duplex(ppd, client_printer_get_capabilities(cprinter, DC_DUPLEX, NULL));
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


void print_job(PrintJob * job) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    size_t env_size = strlen(job->options) + 13; // "JOBOPTIONS=...\0\0"
    char env[env_size];
    g_snprintf(env, env_size, "JOBOPTIONS=%s", job->options);
    env[env_size - 1] = '\0';
    g_autofree wchar_t * cmd_line = as_utf16(g_strdup_printf("print-pdf \"%s\"", job->name));
    DWORD exit_code = 1;
    BOOL result = FALSE;
    if (CreateProcess(NULL, cmd_line, NULL, NULL, FALSE, 0, env, NULL, &si, &pi)) {
        // Wait until child process exits.
        WaitForSingleObject(pi.hProcess, INFINITE);
        result = GetExitCodeProcess(pi.hProcess, &exit_code) && !exit_code;
        g_debug("CreateProcess succeeded with code %d", exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else g_warning("CreateProcess failed");

    if (!result) {
        g_autofree wchar_t * fileW = as_utf16(job->name);
        ShellExecute(NULL, L"open", fileW, NULL, NULL, SW_SHOWNORMAL);
    }
}

