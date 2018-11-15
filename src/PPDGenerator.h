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
