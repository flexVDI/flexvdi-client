/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#ifdef WIN32
#include <windows.h>
#endif
#include "PPDGenerator.h"


typedef struct PaperDescription {
    char * name;
    double width, length;
    double left, bottom, right, top;
} PaperDescription;


PaperDescription * newPaperDescription(char * name, double width, double length,
                                       double left, double bottom, double right, double top) {
    PaperDescription * result = (PaperDescription *)g_malloc(sizeof(PaperDescription));
    result->name = name;
    result->width = width;
    result->length = length;
    result->left = left;
    result->bottom = bottom;
    result->right = right;
    result->top = top;
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
    double left, bottom, right, top;
};


static int isValid(PPDGenerator * ppd) {
    return ppd->printerName && ppd->file && ppd->paperSizes && ppd->defaultPaperSize;
}


PPDGenerator * newPPDGenerator(const char * printerName) {
    PPDGenerator * result = g_malloc0(sizeof(PPDGenerator));
    result->printerName = g_strdup(printerName);
    int fd = g_file_open_tmp("fvXXXXXX.ppd", &result->fileName, NULL);
    result->file = fdopen(fd, "w");
    if (fd == -1 || result->file == NULL) {
        deletePPDGenerator(result);
        return NULL;
    }
    result->left = result->bottom = result->right = result->top = 0.0;
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


static gint comparePaper(gconstpointer a, gconstpointer b) {
    return strcmp(((PaperDescription *)a)->name, ((PaperDescription *)b)->name);
}


void ppdAddPaperSize(PPDGenerator * ppd, char * name, double width, double length,
                     double left, double bottom, double right, double top) {
    PaperDescription tmp;
    tmp.name = name;
    if (!g_slist_find_custom(ppd->paperSizes, &tmp, comparePaper)) {
        ppd->paperSizes = g_slist_prepend(ppd->paperSizes,
                                          newPaperDescription(name, width, length,
                                                              left, bottom, right, top));
        if (ppd->left < left) ppd->left = left;
        if (ppd->bottom < bottom) ppd->bottom = bottom;
        if (ppd->right < width - right) ppd->right = width - right;
        if (ppd->top < length - top) ppd->top = length - top;
    }
}


void ppdSetDefaultPaperSize(PPDGenerator * ppd, char * name) {
    g_free(ppd->defaultPaperSize);
    ppd->defaultPaperSize = name;
}


static gint compareResolutions(gconstpointer a, gconstpointer b) {
    return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}


void ppdAddResolution(PPDGenerator * ppd, int resolution) {
    if (!g_slist_find(ppd->paperSizes, GINT_TO_POINTER(resolution))) {
        ppd->resolutions = g_slist_insert_sorted(ppd->resolutions,
                                                 GINT_TO_POINTER(resolution),
                                                 compareResolutions);
        if (resolution > ppd->defaultResolution) ppd->defaultResolution = resolution;
    }
}


void ppdSetDefaultResolution(PPDGenerator * ppd, int resolution) {
    ppd->defaultResolution = resolution;
}


static gint compareString(gconstpointer a, gconstpointer b) {
    return strcmp((const char *)a, (const char *)b);
}


void ppdAddMediaType(PPDGenerator * ppd, char * media) {
    if (!g_slist_find_custom(ppd->mediaTypes, media, compareString)) {
        ppd->mediaTypes = g_slist_prepend(ppd->mediaTypes, media);
    }
}


void ppdSetDefaultMediaType(PPDGenerator * ppd, char * media) {
    g_free(ppd->defaultType);
    ppd->defaultType = media;
}


void ppdAddTray(PPDGenerator * ppd, char * tray) {
    if (!g_slist_find_custom(ppd->trays, tray, compareString)) {
        ppd->trays = g_slist_prepend(ppd->trays, tray);
    }
}


void ppdSetDefaultTray(PPDGenerator * ppd, char * tray) {
    g_free(ppd->defaultTray);
    ppd->defaultTray = tray;
}


static char * sanitize(const char * str) {
    static char buffer[100];
    const char * j = str;
    int i = 0;
    while (i < 99 && *j != '\0') {
        gunichar c = g_utf8_get_char(j);
        if (c >= 33 && c <= 126 && c != '/' && c != ':' && c != '.' && c != ',' && c != '(' && c != ')') {
            g_unichar_to_utf8(g_unichar_tolower(c), &buffer[i++]);
        }
        j = g_utf8_next_char(j);
    }
    buffer[i] = '\0';
    return buffer;
}


static void generateHeader(PPDGenerator * ppd) {
    char * baseName = g_strrstr(ppd->fileName,
#ifdef WIN32
                                "\\"
#else
                                "/"
#endif
                               ) + 1;
    char * modelName = g_strdup(ppd->printerName);
    const char * colorDevice = ppd->color ? "True" : "False";
    const char * defaultCS = ppd->color ? "RGB" : "Gray";
    g_strdelimit(modelName, "_", ' ');
    fprintf(ppd->file,
            "*PPD-Adobe: \"4.3\"\n"
            "*FileVersion: \"1.0\"\n"
            "*FormatVersion: \"4.3\"\n"
            "*LanguageEncoding: ISOLatin1\n"
            "*LanguageVersion: English\n"
            "*Manufacturer: \"Flexible Software Solutions S.L.\"\n"
            "*ModelName: \"%s\"\n"
            "*ShortNickName: \"%.31s\"\n"
            "*NickName: \"%s\"\n"
            "*PCFileName: \"%s\"\n"
            "*Product: \"(%s)\"\n"
            "*PSVersion: \"(3010) 815\"\n"
            "*Copyright: \"2014-2015 Flexible Software Solutions S.L.\"\n"
            "*LanguageLevel: \"3\"\n"
            "*ColorDevice: %s\n"
            "*DefaultColorSpace: %s\n"
            "*FileSystem: True\n"
            "*Extensions: CMYK FileSystem Composite\n"
            "*TTRasterizer: Type42\n"
            "*FreeVM: \"10000000\"\n"
            "*PrintPSErrors: True\n"
            "*ContoneOnly: True\n"
            "*cupsFilter: \"application/pdf  0  pdftopdf-nocopies\"\n"
            "*cupsLanguages: \"en\"\n"
            "\n"
            , modelName, modelName, modelName, baseName, modelName, colorDevice, defaultCS);
    g_free(modelName);
}


static void generatePaperSizes(PPDGenerator * ppd) {
    GSList * i;
    int maxMediaSize = 0;
    ppd->paperSizes = g_slist_sort(ppd->paperSizes, comparePaper);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        if (maxMediaSize < desc->width) maxMediaSize = desc->width;
        if (maxMediaSize < desc->length) maxMediaSize = desc->length;
    }
    fprintf(ppd->file,
            "*%% == Paper stuff\n"
            "*HWMargins: %d %d %d %d\n"
            , (int)ceil(ppd->left), (int)ceil(ppd->bottom)
            , (int)ceil(ppd->right), (int)ceil(ppd->top));
    fprintf(ppd->file,
            "*%% Ghostscript pdfwrite ignores Orientation, so set the\n"
            "*%% custom page width/length and then use an Install procedure\n"
            "*%% to rotate the image.\n"
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
            , sanitize(ppd->defaultPaperSize));
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PageSize %.34s/%s: \"<< /PageSize [%.2f %.2f] /ImagingBBox null >> setpagedevice\"\n"
                , sanitize(desc->name), desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file,
            "*CloseUI: *PageSize\n\n"

            "*OpenUI *PageRegion: PickOne\n"
            "*DefaultPageRegion: %s\n"
            "*OrderDependency: 20 AnySetup *PageRegion\n"
            , sanitize(ppd->defaultPaperSize));
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PageRegion %.34s/%s: \"<< /PageSize [%.2f %.2f] /ImagingBBox null >> setpagedevice\"\n"
                , sanitize(desc->name), desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file,
            "*CloseUI: *PageRegion\n\n"

            "*DefaultImageableArea: %s\n"
            , ppd->defaultPaperSize);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*ImageableArea %.34s/%s: \"%.2f %.2f %.2f %.2f\"\n"
                , sanitize(desc->name), desc->name
                , desc->left, desc->bottom, desc->right, desc->top);
    }
    fprintf(ppd->file,
            "\n*DefaultPaperDimension: %s\n"
            , ppd->defaultPaperSize);
    for (i = ppd->paperSizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PaperDimension %.34s/%s: \"%.2f %.2f\"\n"
                , sanitize(desc->name), desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file, "\n");
}


static void generateResolutions(PPDGenerator * ppd) {
    GSList * i;
    if (ppd->defaultResolution == 0) {
        ppd->defaultResolution = 300;
    }
    if (!ppd->resolutions) {
        ppd->resolutions = g_slist_append(NULL, GINT_TO_POINTER(ppd->defaultResolution));
    }
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


static void generateDuplex(PPDGenerator * ppd) {
    if (ppd->duplex) {
        fprintf(ppd->file,
                "*%% == Duplex\n"
                "*OpenUI *Duplex/Double-Sided Printing: PickOne\n"
                "*OrderDependency: 30 AnySetup *Duplex\n"
                "*DefaultDuplex: None\n"
                "*Duplex DuplexNoTumble/Long Edge (Standard): \"\"\n"
                "*Duplex DuplexTumble/Short Edge (Flip): \"\"\n"
                "*Duplex None/Off: \"\"\n"
                "*CloseUI: *Duplex\n\n");
    }
}


static void generateColor(PPDGenerator * ppd) {
    if (ppd->color) {
        fprintf(ppd->file,
                "*%% == Color\n"
                "*OpenUI *ColorModel/Color Mode: PickOne\n"
                "*OrderDependency: 30 AnySetup *ColorModel\n"
                "*DefaultColorModel: RGB\n"
                "*ColorModel Gray/Grayscale: \"\"\n"
                "*ColorModel RGB/Color: \"\"\n"
                "*CloseUI: *ColorModel\n\n");
    } else {
        fprintf(ppd->file,
                "*%% == Color\n"
                "*OpenUI *ColorModel/Color Mode: PickOne\n"
                "*OrderDependency: 30 AnySetup *ColorModel\n"
                "*DefaultColorModel: Gray\n"
                "*ColorModel Gray/Grayscale: \"\"\n"
                "*CloseUI: *ColorModel\n\n");
    }
}


static void generateTrays(PPDGenerator * ppd) {
    GSList * i;
    int j;
    if (ppd->trays != NULL) {
        ppd->trays = g_slist_reverse(ppd->trays);
        char * defaultTray = ppd->defaultTray;
        if (!defaultTray) defaultTray = (char *)ppd->trays->data;

        fprintf(ppd->file,
                "*%% == Printer paper trays\n"
                "*OpenUI *InputSlot/Input Slot: PickOne\n"
                "*OrderDependency: 30 AnySetup *InputSlot\n"
                "*DefaultInputSlot: %s\n"
               , sanitize(defaultTray));
        for (i = ppd->trays, j = 0; i != NULL; i = g_slist_next(i), ++j) {
            fprintf(ppd->file,
                    "*InputSlot %s/%s: \"\"\n"
                    , sanitize((const char *)i->data), (const char *)i->data);
        }
        fprintf(ppd->file, "*CloseUI: *InputSlot\n\n");
    }
}


static void generateMediaTypes(PPDGenerator * ppd) {
    GSList * i;
    int j;
    if (ppd->mediaTypes != NULL) {
        ppd->mediaTypes = g_slist_reverse(ppd->mediaTypes);
        char * defaultType = ppd->defaultType;
        if (!defaultType) defaultType = (char *)ppd->mediaTypes->data;

        fprintf(ppd->file,
                "*%% == Media types\n"
                "*OpenUI *MediaType/Media Type: PickOne\n"
                "*OrderDependency: 30 AnySetup *MediaType\n"
                "*DefaultMediaType: %s\n"
               , sanitize(defaultType));
        for (i = ppd->mediaTypes, j = 0; i != NULL; i = g_slist_next(i), ++j) {
            fprintf(ppd->file,
                    "*MediaType %s/%s: \"\"\n"
                    , sanitize((const char *)i->data), (const char *)i->data);
        }
        fprintf(ppd->file, "*CloseUI: *MediaType\n\n");
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
    char * oldLocale = setlocale(LC_NUMERIC, "C");
    setlocale(LC_NUMERIC, "C");
    generateHeader(ppd);
    generatePaperSizes(ppd);
    generateResolutions(ppd);
    generateDuplex(ppd);
    generateColor(ppd);
    generateTrays(ppd);
    generateMediaTypes(ppd);
    // TODO: UI constraints
    generateFonts(ppd);
    setlocale(LC_NUMERIC, oldLocale);

    return ppd->fileName;
}
