/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef SERIALREDIR_H
#define SERIALREDIR_H

#include "spice-client.h"
#include "configuration.h"

void serial_port_init(ClientConf * conf);
void serial_port_open(SpiceChannel * channel);

#endif // SERIALREDIR_H
