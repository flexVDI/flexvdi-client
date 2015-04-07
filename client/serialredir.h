/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef SERIALREDIR_H
#define SERIALREDIR_H

#include "spice-client.h"

void serialPortOpened(SpiceChannel * channel);
void serialPortData(SpicePortChannel * pchannel, gpointer data, int size);
void serialChannelDestroy(SpicePortChannel * channel);

#endif // SERIALREDIR_H
