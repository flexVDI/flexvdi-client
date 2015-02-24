/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <string.h>
#include <glib.h>
#ifdef WIN32
#include <wingdi.h>
#endif
#include "PPDGenerator.h"


typedef struct PaperDescription {
    char * name;
    int width, length;
} PaperDescription;


PaperDescription * newPaperDescription(const char * name, int width, int length) {
    PaperDescription * result = (PaperDescription *)g_malloc(sizeof(PaperDescription));
    result->name = g_strdup(name);
    result->width = width;
    result->length = length;
    return result;
}


void deletePaperDescription(gpointer p) {
    g_free(((PaperDescription *)p)->name);
    g_free(p);
}


struct PPDGenerator {
    char * printerName;
    FILE * file;
    char * fileName;
    int color;
    int duplex;
    GSList * paperSizes;
    char * defaultPaperSize;
    GSList * trays;
    char * defaultTray;
    GSList * mediaTypes;
    char * defaultType;
    GSList * resolutions;
    int defaultResolution;
    int margins[4];
};


static int isValid(PPDGenerator * ppd) {
    return ppd->printerName && ppd->file && ppd->paperSizes && ppd->defaultPaperSize &&
        ppd->resolutions;
};


PPDGenerator * newPPDGenerator(const char * printerName) {
    PPDGenerator * result = g_malloc0(sizeof(PPDGenerator));
    result->printerName = g_strdup(printerName);
    int fd = g_file_open_tmp("flexvdips-XXXXXX.ppd", &result->fileName, NULL);
    result->file = fdopen(fd, "w");
    if (fd == -1 || result->file == NULL) {
        deletePPDGenerator(result);
        return NULL;
    }
    return result;
}


void deletePPDGenerator(PPDGenerator * ppd) {
    fclose(ppd->file);
    g_free(ppd->fileName);
    g_free(ppd->printerName);
    g_free(ppd->defaultPaperSize);
    g_free(ppd->defaultTray);
    g_free(ppd->defaultType);
    g_slist_free_full(ppd->paperSizes, deletePaperDescription);
    g_slist_free_full(ppd->trays, g_free);
    g_slist_free_full(ppd->mediaTypes, g_free);
    g_slist_free(ppd->resolutions);
    g_free(ppd);
}


void ppdSetColor(PPDGenerator * ppd, int color) {
    ppd->color = color;
}


void ppdSetDuplex(PPDGenerator * ppd, int duplex) {
    ppd->duplex = duplex;
}


static gint comparePaper(gconstpointer paper, gconstpointer name) {
    return strcmp((const char *)name, ((PaperDescription *)paper)->name);
}


void ppdAddPaperSize(PPDGenerator * ppd, const char * name, int width, int length) {
    if (!g_slist_find_custom(ppd->paperSizes, name, comparePaper)) {
        ppd->paperSizes = g_slist_append(ppd->paperSizes,
                                         newPaperDescription(name, width, length));
    }
}


void ppdSetDefaultPaperSize(PPDGenerator * ppd, const char * name) {
    g_free(ppd->defaultPaperSize);
    ppd->defaultPaperSize = g_strdup(name);
}


void ppdAddResolution(PPDGenerator * ppd, int resolution) {
    if (!g_slist_find(ppd->paperSizes, GINT_TO_POINTER(resolution))) {
        ppd->resolutions = g_slist_append(ppd->resolutions, GINT_TO_POINTER(resolution));
        if (resolution > ppd->defaultResolution) ppd->defaultResolution = resolution;
    }
}


void ppdSetDefaultResolution(PPDGenerator * ppd, int resolution) {
    ppd->defaultResolution = resolution;
}


static gint compareString(gconstpointer a, gconstpointer b) {
    return strcmp((const char *)a, (const char *)b);
}


void ppdAddMediaType(PPDGenerator * ppd, const gchar * media) {
    if (!g_slist_find_custom(ppd->mediaTypes, media, compareString)) {
        ppd->mediaTypes = g_slist_append(ppd->mediaTypes, g_strdup(media));
    }
}


void ppdSetDefaultMediaType(PPDGenerator * ppd, const char * media) {
    g_free(ppd->defaultType);
    ppd->defaultType = g_strdup(media);
}


void ppdAddTray(PPDGenerator * ppd, const gchar * tray) {
    if (!g_slist_find_custom(ppd->trays, tray, compareString)) {
        ppd->trays = g_slist_append(ppd->trays, g_strdup(tray));
    }
}


void ppdSetDefaultTray(PPDGenerator * ppd, const char * tray) {
    g_free(ppd->defaultTray);
    ppd->defaultTray = g_strdup(tray);
}


void ppdSetHWMargins(PPDGenerator * ppd, int top, int down, int left, int right) {
    ppd->margins[0] = left; // In PPD order
    ppd->margins[1] = down;
    ppd->margins[2] = right;
    ppd->margins[3] = top;
}


static void generateHeader(PPDGenerator * ppd) {
    char * baseName = g_strrstr(ppd->fileName, "/") + 1;
    fprintf(ppd->file,
            "*PPD-Adobe: \"4.3\"\n"
            "*FileVersion: \"1.0\"\n"
            "*FormatVersion: \"4.3\"\n"
            "*LanguageEncoding: ISOLatin1\n"
            "*LanguageVersion: English\n"
            "*Manufacturer: \"Flexible Software Solutions S.L.\"\n"
            "*ModelName: \"%s\"\n"
            "*ShortNickName: \"%s\"\n"
            "*NickName: \"%s\"\n"
            "*PCFileName: \"%s\"\n"
            "*Product: \"%s\"\n"
            "*PSVersion: \"(3010) 815\"\n"
            "*cupsFilter: \"application/pdf  0  pdftopdf-nocopies\"\n"
            "*Copyright: \"2014-2015 Flexible Software Solutions S.L.\"\n"
            "*LanguageLevel: \"3\"\n"
            "*ColorDevice: True\n"
            "*DefaultColorSpace: RGB\n"
            "*FileSystem: True\n"
            "*Extensions: CMYK FileSystem Composite\n"
            "*TTRasterizer: Type42\n"
            "*FreeVM: \"10000000\"\n"
            "*PrintPSErrors: True\n"
            "*ContoneOnly: True\n\n"
            , ppd->printerName, ppd->printerName, ppd->printerName, baseName, ppd->printerName);
}


static generateResolutions(PPDGenerator * ppd) {
    GSList * i;
    if (ppd->defaultResolution == 0) ppd->defaultResolution = 300;
    if (!ppd->resolutions)
        ppd->resolutions = g_slist_append(NULL, GINT_TO_POINTER(ppd->defaultResolution));
    fprintf(ppd->file,
            "*%% == Valid resolutions\n"
            "*OpenUI *Resolution: PickOne\n"
            "*DefaultResolution: %ddpi\n"
            "*OrderDependency: 10 AnySetup *Resolution\n"
            , ppd->defaultResolution > 0 ? ppd->defaultResolution : 300);
    for (i = ppd->resolutions; i != NULL; i = g_slist_next(i)) {
        int r = GPOINTER_TO_INT(i->data);
        fprintf(ppd->file,
                "*Resolution %ddpi: \"<< /HWResolution [%d %d] >> setpagedevice\"\n"
                , r, r, r);
    }
    fprintf(ppd->file, "*CloseUI: *Resolution\n\n");
}


static void generatePaperSizes(PPDGenerator * ppd) {
    GSList * i;
    int maxMediaSize = 0;
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        if (maxMediaSize < desc->width) maxMediaSize = desc->width;
        if (maxMediaSize < desc->length) maxMediaSize = desc->length;
    }
    fprintf(ppd->file,
            "*%% == Paper stuff\n"
            "*%% Page sizes taken from ghostscript gs_statd.ps.\n"
            "*%% Ghostscript pdfwrite ignores Orientation, so set the\n"
            "*%% custom page width/length and then use an Install procedure\n"
            "*%% to rotate the image.\n"
            "*HWMargins: %d %d %d %d\n"
            , ppd->margins[0], ppd->margins[1], ppd->margins[2], ppd->margins[3]);
    fprintf(ppd->file,
            "*ParamCustomPageSize Width: 1 points 1 %d\n"
            "*ParamCustomPageSize Height: 2 points 1 %d\n"
            "*ParamCustomPageSize WidthOffset/Width Margin: 3 points 0 %d\n"
            "*ParamCustomPageSize HeightOffset/Height Margin: 4 points 0 %d\n"
            "*ParamCustomPageSize Orientation: 5 int 0 3\n"
            "*NonUIOrderDependency: 20 AnySetup *CustomPageSize\n"
            "*CustomPageSize True: \"\n"
            "5 -2 roll exch 5 2 roll\n"
            "3 -2 roll exch 3 2 roll\n"
            "[ {}\n"
            "{90 rotate 0 currentpagedevice /PageSize get 0 get neg translate}\n"
            "{180 rotate currentpagedevice /PageSize get\n"
            "    dup 0 get neg exch 1 get neg translate}\n"
            "    {270 rotate currentpagedevice /PageSize get 1 get neg 0 translate}\n"
            "] exch get\n"
            "4 dict dup begin 6 1 roll\n"
            "/Install exch def\n"
            "2 array astore /PageOffset exch def\n"
            "2 array astore /PageSize exch def\n"
            "/ImagingBBox null def\n"
            "end setpagedevice\"\n"
            "*End\n"
            "*MaxMediaWidth: \"%d\"\n"
            "*MaxMediaHeight: \"%d\"\n"
            "*LandscapeOrientation: Any\n\n"
            , maxMediaSize, maxMediaSize, maxMediaSize, maxMediaSize, maxMediaSize, maxMediaSize);

    fprintf(ppd->file,
            "*OpenUI *PageSize: PickOne\n"
            "*DefaultPageSize: %s\n"
            "*OrderDependency: 20 AnySetup *PageSize\n"
            , ppd->defaultPaperSize);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PageSize %s: \"<< /PageSize [%d %d] /ImagingBBox null >> setpagedevice\"\n"
                , desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file,
            "*CloseUI: *PageSize\n\n"

            "*OpenUI *PageRegion: PickOne\n"
            "*DefaultPageRegion: %s\n"
            "*OrderDependency: 20 AnySetup *PageRegion\n"
            , ppd->defaultPaperSize);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PageRegion %s: \"<< /PageSize [%d %d] /ImagingBBox null >> setpagedevice\"\n"
                , desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file,
            "*CloseUI: *PageRegion\n\n"

            "*DefaultImageableArea: %s\n"
            , ppd->defaultPaperSize);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*ImageableArea %s: \"0 0 %d %d\"\n"
                , desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file,
            "\n*DefaultPaperDimension: %s\n"
            , ppd->defaultPaperSize);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PaperDimension %s: \"%d %d\"\n"
                , desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file, "\n");
}


static void generateDuplex(PPDGenerator * ppd) {
    GSList * i;
    if (ppd->duplex) {
        fprintf(ppd->file,
                "*%% == Duplex\n"
                "*OpenUI *Duplex/Double-Sided Printing: PickOne\n"
                "*OrderDependency: 30 AnySetup *Duplex\n"
                "*DefaultDuplex: None\n"
                "*Duplex DuplexNoTumble/Long Edge (Standard): \"<</Duplex true/Tumble false>>setpagedevice\"\n"
                "*Duplex DuplexTumble/Short Edge (Flip): \"<</Duplex true/Tumble true>>setpagedevice\"\n"
                "*Duplex None/Off: \"<</Duplex false/Tumble false>>setpagedevice\"\n"
                "*CloseUI: *Duplex\n\n");
    }
}


static void generateTrays(PPDGenerator * ppd) {
    GSList * i;
    if (ppd->trays != NULL) {
        if (!ppd->defaultTray)
            ppd->defaultTray = g_strdup((const char *)ppd->trays->data);
        fprintf(ppd->file,
                "*%% == Printer paper trays\n"
                "*OpenUI *InputSlot/Input Slot: PickOne\n"
                "*OrderDependency: 30 AnySetup *InputSlot\n"
                "*DefaultInputSlot: %s\n"
               , ppd->defaultTray);
        for (i = ppd->trays; i != NULL; i = g_slist_next(i)) {
            const char * tray = (const char *)i->data;
            char * Tray = g_ascii_strdown(tray, -1);
            Tray[0] = g_ascii_toupper(Tray[0]);
            fprintf(ppd->file,
                    "*InputSlot %s/%s: \"<< /InputAttributes <</Priority [2 0 1]>> >> setpagedevice\"\n"
                    , tray, Tray);
            g_free(Tray);
        }
        fprintf(ppd->file, "*CloseUI *InputSlot\n\n");
    }
}


static void generateMediaTypes(PPDGenerator * ppd) {
    GSList * i;
    if (ppd->mediaTypes != NULL) {
        if (!ppd->defaultType)
            ppd->defaultType = g_strdup((const char *)ppd->mediaTypes->data);
        fprintf(ppd->file,
                "*%% == Media types\n"
                "*OpenUI *MediaType/Media Type: PickOne\n"
                "*OrderDependency: 30 AnySetup *MediaType\n"
                "*DefaultMediaType: %s\n"
               , ppd->defaultType);
        for (i = ppd->mediaTypes; i != NULL; i = g_slist_next(i)) {
            const char * media = (const char *)i->data;
            char * Media = g_ascii_strdown(media, -1);
            Media[0] = g_ascii_toupper(Media[0]);
            fprintf(ppd->file,
                    "*MediaType %s/%s Paper: \"<</MediaType (%s)>> setpagedevice\"\n"
                    , media, Media, media);
            g_free(Media);
        }
        fprintf(ppd->file, "*CloseUI *MediaType\n\n");
    }
}


static void generateFonts(PPDGenerator * ppd) {
    fprintf(ppd->file,
            "*%% == Fonts\n"
            "*DefaultFont: Courier\n"
            "*Font Bookman-Demi: Standard \"(1.05)\" Standard ROM\n"
            "*Font Bookman-DemiItalic: Standard \"(1.05)\" Standard ROM\n"
            "*Font Bookman-Light: Standard \"(1.05)\" Standard ROM\n"
            "*Font Bookman-LightItalic: Standard \"(1.05)\" Standard ROM\n"
            "*Font Courier: Standard \"(1.05)\" Standard ROM\n"
            "*Font Courier-Oblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font Courier-Bold: Standard \"(1.05)\" Standard ROM\n"
            "*Font Courier-BoldOblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font AvantGarde-Book: Standard \"(1.05)\" Standard ROM\n"
            "*Font AvantGarde-BookOblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font AvantGarde-Demi: Standard \"(1.05)\" Standard ROM\n"
            "*Font AvantGarde-DemiOblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-Oblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-Bold: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-BoldOblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-Narrow: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-Narrow-Oblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-Narrow-Bold: Standard \"(1.05)\" Standard ROM\n"
            "*Font Helvetica-Narrow-BoldOblique: Standard \"(1.05)\" Standard ROM\n"
            "*Font Palatino-Roman: Standard \"(1.05)\" Standard ROM\n"
            "*Font Palatino-Italic: Standard \"(1.05)\" Standard ROM\n"
            "*Font Palatino-Bold: Standard \"(1.05)\" Standard ROM\n"
            "*Font Palatino-BoldItalic: Standard \"(1.05)\" Standard ROM\n"
            "*Font NewCenturySchlbk-Roman: Standard \"(1.05)\" Standard ROM\n"
            "*Font NewCenturySchlbk-Italic: Standard \"(1.05)\" Standard ROM\n"
            "*Font NewCenturySchlbk-Bold: Standard \"(1.05)\" Standard ROM\n"
            "*Font NewCenturySchlbk-BoldItalic: Standard \"(1.05)\" Standard ROM\n"
            "*Font Times-Roman: Standard \"(1.05)\" Standard ROM\n"
            "*Font Times-Italic: Standard \"(1.05)\" Standard ROM\n"
            "*Font Times-Bold: Standard \"(1.05)\" Standard ROM\n"
            "*Font Times-BoldItalic: Standard \"(1.05)\" Standard ROM\n"
            "*Font Symbol: Special \"(001.005)\" Special ROM\n"
            "*Font ZapfChancery-MediumItalic: Standard \"(1.05)\" Standard ROM\n"
            "*Font ZapfDingbats: Special \"(001.005)\" Special ROM\n"
            "*Font Charter-Roman: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font CharterBT-Roman: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font Charter-Italic: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font CharterBT-Italic: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font Charter-Bold: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font CharterBT-Bold: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font Charter-BoldItalic: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font CharterBT-BoldItalic: Standard \"(2.0-1.0)\" Standard ROM\n"
            "*Font Utopia-Regular: Standard \"(001.001)\" Standard ROM\n"
            "*Font Utopia-Italic: Standard \"(001.001)\" Standard ROM\n"
            "*Font Utopia-Bold: Standard \"(001.001)\" Standard ROM\n"
            "*Font Utopia-BoldItalic: Standard \"(001.001)\" Standard ROM\n"
           );
}


gchar * generatePPD(PPDGenerator * ppd) {
    if (!isValid(ppd)) return NULL;
    generateHeader(ppd);
    generateResolutions(ppd);
    generatePaperSizes(ppd);
    generateDuplex(ppd);
    generateTrays(ppd);
    generateMediaTypes(ppd);
    // TODO: UI constraints
    generateFonts(ppd);

    return ppd->fileName;
}
