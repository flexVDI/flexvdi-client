/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include "flexvdi-cmdline.h"
#include "flexvdi-spice.h"

static gchar * username, * password, * domain;
static const gchar ** serialParams;


const gchar * getUsernameOption() {
    return username;
}


const gchar * getPasswordOption() {
    return password;
}


const gchar * getDomainOption() {
    return domain;
}


const gchar ** getSerialPortParams() {
    return serialParams;
}


static size_t freadsecure(gchar ** buffer, FILE * file) {
    size_t count = 0, bufferSize = 0, i;
    do {
        gchar * c = *buffer;
        *buffer = g_malloc(bufferSize += 1024);
        memcpy(*buffer, c, count);
        for (i = 0; i < count; ++i)
            c[i] = '\0';
        c = *buffer + count;
        for (; count < bufferSize; ++c, ++count) {
            int r = fgetc(file);
            if (r == '\n' || r == EOF) break;
            *c = r & 255;
        }
    } while (count == bufferSize);
    (*buffer)[count] = '\0';
    return count;
}


static gboolean readPwdFromStdin(const gchar * optionName, const gchar * value,
                                 gpointer data, GError ** error) {
    size_t bytesRead = freadsecure(&password, stdin);
    if (bytesRead > 0) {
        return TRUE;
    } else {
        flexvdiLog(L_WARN, "Failed to read password from stdin");
        return FALSE;
    }
}


static const GOptionEntry cmdline_entries[] = {
    // { long_name, short_name, flags, arg, arg_data, description, arg_description },
    { "flexvdi-user", 0, 0, G_OPTION_ARG_STRING, &username,
      N_("User name for Single Sign-on."), N_("<username>") },
    { "flexvdi-passwd", 0, 0, G_OPTION_ARG_STRING, &password,
      N_("Password for Single Sign-on. WARNING: This is insecure."), N_("<password>") },
    { "flexvdi-passwd-from-stdin", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
      readPwdFromStdin, N_("Read password from stdin."), NULL },
    { "flexvdi-domain", 0, 0, G_OPTION_ARG_STRING, &domain,
      N_("Domain name for Single Sign-on."), N_("<domainname>") },
    { "flexvdi-serial-port", 0, 0, G_OPTION_ARG_STRING_ARRAY, &serialParams,
      N_("Add serial port with parameters"), N_("<serial params>") },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


GOptionGroup * flexvdi_get_option_group(void) {
    username = password = domain = NULL;
    GOptionGroup * grp = g_option_group_new("flexvdi", _("flexVDI Options:"),
                                            _("Show flexVDI Options"), NULL, NULL);
    g_option_group_add_entries(grp, cmdline_entries);

    return grp;
}
