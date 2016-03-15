/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include "flexvdi-cmdline.h"
#include "flexvdi-spice.h"

static const gchar ** serialParams;


const gchar ** getSerialPortParams() {
    return serialParams;
}


static const GOptionEntry cmdline_entries[] = {
    // { long_name, short_name, flags, arg, arg_data, description, arg_description },
    { "flexvdi-serial-port", 0, 0, G_OPTION_ARG_STRING_ARRAY, &serialParams,
      N_("Add serial port with parameters"), N_("<serial params>") },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


GOptionGroup * flexvdi_get_option_group(void) {
    GOptionGroup * grp = g_option_group_new("flexvdi", _("flexVDI Options:"),
                                            _("Show flexVDI Options"), NULL, NULL);
    g_option_group_add_entries(grp, cmdline_entries);

    return grp;
}
