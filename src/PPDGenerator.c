/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include "PPDGenerator.h"
#include "flexvdi-port.h"


typedef struct PaperDescription {
    char * name;
    double width, length;
    double left, bottom, right, top;
} PaperDescription;


PaperDescription * paper_description_new(char * name, double width, double length,
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


void paper_description_delete(gpointer p) {
    g_free(((PaperDescription *)p)->name);
    g_free(p);
}


struct _PPDGenerator {
    GObject parent;
    char * printer_name;
    FILE * file;
    char * filename;
    int color;
    int duplex;
    GSList * paper_sizes;
    char * default_paper_size;
    GSList * trays;
    char * default_tray;
    GSList * media_types;
    char * default_type;
    GSList * resolutions;
    int default_resolution;
    double left, bottom, right, top;
};

G_DEFINE_TYPE(PPDGenerator, ppd_generator, G_TYPE_OBJECT);

static void ppd_generator_finalize(GObject * obj);

static void ppd_generator_class_init(PPDGeneratorClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->finalize = ppd_generator_finalize;
}

static void ppd_generator_init(PPDGenerator * ppd) {
    int fd = g_file_open_tmp("fvXXXXXX.ppd", &ppd->filename, NULL);
    ppd->file = fdopen(fd, "w");
    if (fd == -1 || ppd->file == NULL) {
        g_warning("Failed to create temp PPD file");
    }
    ppd->left = ppd->bottom = ppd->right = ppd->top = 0.0;
}


static int is_valid(PPDGenerator * ppd) {
    return ppd->printer_name && ppd->file && ppd->paper_sizes && ppd->default_paper_size;
}


PPDGenerator * ppd_generator_new(const char * printer_name) {
    PPDGenerator * ppd = g_object_new(PPD_GENERATOR_TYPE, NULL);
    ppd->printer_name = g_strdup(printer_name);
    return ppd;
}


static void ppd_generator_finalize(GObject * obj) {
    PPDGenerator * ppd = PPD_GENERATOR(obj);
    fclose(ppd->file);
    g_free(ppd->filename);
    g_free(ppd->printer_name);
    g_free(ppd->default_paper_size);
    g_free(ppd->default_tray);
    g_free(ppd->default_type);
    g_slist_free_full(ppd->paper_sizes, paper_description_delete);
    g_slist_free_full(ppd->trays, g_free);
    g_slist_free_full(ppd->media_types, g_free);
    g_slist_free(ppd->resolutions);
    G_OBJECT_CLASS(ppd_generator_parent_class)->finalize(obj);
}


void ppd_generator_set_color(PPDGenerator * ppd, int color) {
    ppd->color = color;
}


void ppd_generator_set_duplex(PPDGenerator * ppd, int duplex) {
    ppd->duplex = duplex;
}


static gint cmp_paper(gconstpointer a, gconstpointer b) {
    return strcmp(((PaperDescription *)a)->name, ((PaperDescription *)b)->name);
}


void ppd_generator_add_paper_size(PPDGenerator * ppd, char * name, double width, double length,
                                  double left, double bottom, double right, double top) {
    PaperDescription tmp;
    tmp.name = name;
    if (!g_slist_find_custom(ppd->paper_sizes, &tmp, cmp_paper)) {
        ppd->paper_sizes = g_slist_prepend(ppd->paper_sizes,
                                           paper_description_new(name, width, length,
                                                                 left, bottom, right, top));
        if (ppd->left < left) ppd->left = left;
        if (ppd->bottom < bottom) ppd->bottom = bottom;
        if (ppd->right < width - right) ppd->right = width - right;
        if (ppd->top < length - top) ppd->top = length - top;
    }
}


void ppd_generator_set_default_paper_size(PPDGenerator * ppd, char * name) {
    g_free(ppd->default_paper_size);
    ppd->default_paper_size = name;
}


static gint cmp_resolution(gconstpointer a, gconstpointer b) {
    return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}


void ppd_generator_add_resolution(PPDGenerator * ppd, int resolution) {
    if (!g_slist_find(ppd->paper_sizes, GINT_TO_POINTER(resolution))) {
        ppd->resolutions = g_slist_insert_sorted(ppd->resolutions,
                                                 GINT_TO_POINTER(resolution),
                                                 cmp_resolution);
        if (resolution > ppd->default_resolution) ppd->default_resolution = resolution;
    }
}


void ppd_generator_set_default_resolution(PPDGenerator * ppd, int resolution) {
    ppd->default_resolution = resolution;
}


static gint cmp_str(gconstpointer a, gconstpointer b) {
    return strcmp((const char *)a, (const char *)b);
}


void ppd_generator_add_media_type(PPDGenerator * ppd, char * media) {
    if (!g_slist_find_custom(ppd->media_types, media, cmp_str)) {
        ppd->media_types = g_slist_prepend(ppd->media_types, media);
    }
}


void ppd_generator_set_default_media_type(PPDGenerator * ppd, char * media) {
    g_free(ppd->default_type);
    ppd->default_type = media;
}


void ppd_generator_add_tray(PPDGenerator * ppd, char * tray) {
    if (!g_slist_find_custom(ppd->trays, tray, cmp_str)) {
        ppd->trays = g_slist_prepend(ppd->trays, tray);
    }
}


void ppd_generator_set_default_tray(PPDGenerator * ppd, char * tray) {
    g_free(ppd->default_tray);
    ppd->default_tray = tray;
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


static void generate_header(PPDGenerator * ppd) {
    g_autofree gchar * base_name = g_path_get_basename(ppd->filename);
    g_autofree gchar * model_name = g_strdup(ppd->printer_name);
    const char * color_dev = ppd->color ? "True" : "False";
    const char * defaultCS = ppd->color ? "RGB" : "Gray";
    g_strdelimit(model_name, "_", ' ');
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
            , model_name, model_name, model_name, base_name, model_name, color_dev, defaultCS);
}


static void generate_paper_sizes(PPDGenerator * ppd) {
    GSList * i;
    int max_size = 0;
    ppd->paper_sizes = g_slist_sort(ppd->paper_sizes, cmp_paper);
    for (i = ppd->paper_sizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        if (max_size < desc->width) max_size = desc->width;
        if (max_size < desc->length) max_size = desc->length;
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
            , max_size, max_size, max_size, max_size, max_size, max_size);

    fprintf(ppd->file,
            "*OpenUI *PageSize: PickOne\n"
            "*DefaultPageSize: %s\n"
            "*OrderDependency: 20 AnySetup *PageSize\n"
            , sanitize(ppd->default_paper_size));
    for (i = ppd->paper_sizes; i != NULL; i = g_slist_next(i)) {
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
            , sanitize(ppd->default_paper_size));
    for (i = ppd->paper_sizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PageRegion %.34s/%s: \"<< /PageSize [%.2f %.2f] /ImagingBBox null >> setpagedevice\"\n"
                , sanitize(desc->name), desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file,
            "*CloseUI: *PageRegion\n\n"

            "*DefaultImageableArea: %s\n"
            , ppd->default_paper_size);
    for (i = ppd->paper_sizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*ImageableArea %.34s/%s: \"%.2f %.2f %.2f %.2f\"\n"
                , sanitize(desc->name), desc->name
                , desc->left, desc->bottom, desc->right, desc->top);
    }
    fprintf(ppd->file,
            "\n*DefaultPaperDimension: %s\n"
            , ppd->default_paper_size);
    for (i = ppd->paper_sizes; i != NULL; i = g_slist_next(i)) {
        PaperDescription * desc = (PaperDescription *)i->data;
        fprintf(ppd->file,
                "*PaperDimension %.34s/%s: \"%.2f %.2f\"\n"
                , sanitize(desc->name), desc->name, desc->width, desc->length);
    }
    fprintf(ppd->file, "\n");
}


static void generate_resolutions(PPDGenerator * ppd) {
    GSList * i;
    if (ppd->default_resolution == 0) {
        ppd->default_resolution = 300;
    }
    if (!ppd->resolutions) {
        ppd->resolutions = g_slist_append(NULL, GINT_TO_POINTER(ppd->default_resolution));
    }
    fprintf(ppd->file,
            "*%% == Valid resolutions\n"
            "*OpenUI *Resolution: PickOne\n"
            "*DefaultResolution: %ddpi\n"
            "*OrderDependency: 10 AnySetup *Resolution\n"
            , ppd->default_resolution > 0 ? ppd->default_resolution : 300);
    for (i = ppd->resolutions; i != NULL; i = g_slist_next(i)) {
        int r = GPOINTER_TO_INT(i->data);
        fprintf(ppd->file,
                "*Resolution %ddpi: \"<< /HWResolution [%d %d] >> setpagedevice\"\n"
                , r, r, r);
    }
    fprintf(ppd->file, "*CloseUI: *Resolution\n\n");
}


static void generate_duplex(PPDGenerator * ppd) {
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


static void generate_color(PPDGenerator * ppd) {
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


static void generate_trays(PPDGenerator * ppd) {
    GSList * i;
    int j;
    if (ppd->trays != NULL) {
        ppd->trays = g_slist_reverse(ppd->trays);
        char * default_tray = ppd->default_tray;
        if (!default_tray) default_tray = (char *)ppd->trays->data;

        fprintf(ppd->file,
                "*%% == Printer paper trays\n"
                "*OpenUI *InputSlot/Input Slot: PickOne\n"
                "*OrderDependency: 30 AnySetup *InputSlot\n"
                "*DefaultInputSlot: %s\n"
               , sanitize(default_tray));
        for (i = ppd->trays, j = 0; i != NULL; i = g_slist_next(i), ++j) {
            fprintf(ppd->file,
                    "*InputSlot %s/%s: \"\"\n"
                    , sanitize((const char *)i->data), (const char *)i->data);
        }
        fprintf(ppd->file, "*CloseUI: *InputSlot\n\n");
    }
}


static void generate_media_types(PPDGenerator * ppd) {
    GSList * i;
    int j;
    if (ppd->media_types != NULL) {
        ppd->media_types = g_slist_reverse(ppd->media_types);
        char * default_type = ppd->default_type;
        if (!default_type) default_type = (char *)ppd->media_types->data;

        fprintf(ppd->file,
                "*%% == Media types\n"
                "*OpenUI *MediaType/Media Type: PickOne\n"
                "*OrderDependency: 30 AnySetup *MediaType\n"
                "*DefaultMediaType: %s\n"
               , sanitize(default_type));
        for (i = ppd->media_types, j = 0; i != NULL; i = g_slist_next(i), ++j) {
            fprintf(ppd->file,
                    "*MediaType %s/%s: \"\"\n"
                    , sanitize((const char *)i->data), (const char *)i->data);
        }
        fprintf(ppd->file, "*CloseUI: *MediaType\n\n");
    }
}


static void generate_fonts(PPDGenerator * ppd) {
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


gchar * ppd_generator_run(PPDGenerator * ppd) {
    if (!is_valid(ppd)) {
        g_warning("Invalid PPD data for printer %s", ppd->printer_name);
        return NULL;
    }
    char * old_locale = setlocale(LC_NUMERIC, "C");
    setlocale(LC_NUMERIC, "C");
    generate_header(ppd);
    generate_paper_sizes(ppd);
    generate_resolutions(ppd);
    generate_duplex(ppd);
    generate_color(ppd);
    generate_trays(ppd);
    generate_media_types(ppd);
    // TODO: UI constraints
    generate_fonts(ppd);
    setlocale(LC_NUMERIC, old_locale);

    return ppd->filename;
}
