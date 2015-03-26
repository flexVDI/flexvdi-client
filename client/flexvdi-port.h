/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDI_PORT_H_
#define _FLEXVDI_PORT_H_

#include <glib.h>

void flexvdi_port_register_session(gpointer session);

// SSO API
void flexvdi_send_credentials(const gchar *username, const gchar *password,
                              const gchar *domain);

// Print Client API
void flexvdi_get_printer_list(GSList ** printer_list);
void flexvdi_share_printer(const char * printer);
void flexvdi_unshare_printer(const char * printer);

#endif /* _FLEXVDI_PORT_H_ */
