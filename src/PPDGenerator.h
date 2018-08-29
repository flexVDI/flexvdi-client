/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _PPD_GENERATOR_H
#define _PPD_GENERATOR_H

#include <glib-object.h>

#define PPD_GENERATOR_TYPE (ppd_generator_get_type())
G_DECLARE_FINAL_TYPE(PPDGenerator, ppd_generator, PPD, GENERATOR, GObject)

PPDGenerator * ppd_generator_new(const char * printer_name);
void ppd_generator_set_color(PPDGenerator * ppd, int color);
void ppd_generator_set_duplex(PPDGenerator * ppd, int duplex);
void ppd_generator_add_paper_size(PPDGenerator * ppd, char * name,
                                  double width, double length,
                                  double left, double down,
                                  double right, double top);
void ppd_generator_set_default_paper_size(PPDGenerator * ppd, char * name);
void ppd_generator_add_resolution(PPDGenerator * ppd, int resolution);
void ppd_generator_set_default_resolution(PPDGenerator * ppd, int resolution);
void ppd_generator_add_media_type(PPDGenerator * ppd, char * media);
void ppd_generator_set_default_media_type(PPDGenerator * ppd, char * media);
void ppd_generator_add_tray(PPDGenerator * ppd, char * tray);
void ppd_generator_set_default_tray(PPDGenerator * ppd, char * tray);
char * ppd_generator_run(PPDGenerator * ppd);

#endif /* _PPD_GENERATOR_H */
