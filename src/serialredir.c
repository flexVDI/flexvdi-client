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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include "spice-client.h"
#include "flexvdi-port.h"
#include "spice-util.h"
#include "serialredir.h"


typedef struct SerialPort {
    char * device_name;
    SpicePortChannel * channel;
    GCancellable * cancellable;
    int fd;
    struct termios tio;
    GInputStream * istream;
    GOutputStream * ostream;
} SerialPort;


static gchar ** serial_params;
static SerialPort * serial_ports;
static int num_ports;


void serial_port_init(ClientConf * conf) {
    serial_params = client_conf_get_serial_params(conf);
    num_ports = 0;
    if (serial_params) {
        while (serial_params[num_ports]) ++num_ports;
        serial_ports = g_malloc0(sizeof(SerialPort) * num_ports);
    }
}


static SerialPort * get_serial_port(SpicePortChannel * channel) {
    int port_number;
    for (port_number = 0; port_number < num_ports; ++port_number)
        if (serial_ports[port_number].channel == channel)
            return &serial_ports[port_number];
    return NULL;
}


typedef struct SerialPortBuffer {
    SerialPort * serial;
    char data;
} SerialPortBuffer;


static SerialPortBuffer * get_buffer(SerialPort * serial) {
    SerialPortBuffer * buffer = g_malloc(sizeof(SerialPortBuffer));
    buffer->serial = serial;
    return buffer;
}


static void close_serial(SerialPort * serial) {
    SPICE_DEBUG("Closing serial device");
    g_cancellable_cancel(serial->cancellable);
    close(serial->fd);
    serial->fd = 0;
    g_object_unref(serial->istream);
    g_object_unref(serial->ostream);
}


static void set_serial_params(SerialPort * serial, const char * device,
                              const char * speed_str, const char * mode) {
    g_debug("Using serial device %s, with %sbps mode %.3s\n",
            device, speed_str, mode);

    g_free(serial->device_name);
    serial->device_name = g_strdup(device);

    memset(&serial->tio, 0, sizeof(struct termios));
    serial->tio.c_iflag = 0;
    serial->tio.c_oflag = 0;
    serial->tio.c_cflag = CREAD | CLOCAL;
    serial->tio.c_lflag = 0;
    serial->tio.c_cc[VMIN] = 1;
    serial->tio.c_cc[VTIME] = 5;

    if (strnlen(mode, 3) == 3) {
        switch (mode[0] - '0') {
            case 5: serial->tio.c_cflag |= CS5; break;
            case 6: serial->tio.c_cflag |= CS6; break;
            case 7: serial->tio.c_cflag |= CS7; break;
            default: serial->tio.c_cflag |= CS8; break;
        }
        switch (mode[1]) {
            case 'E': serial->tio.c_cflag |= PARENB; break;
            case 'O': serial->tio.c_cflag |= PARENB | PARODD; break;
        }
        if (mode[2] == '2') serial->tio.c_cflag |= CSTOPB;
    }

    speed_t speed_val = B0;
    int speed = atoi(speed_str);
    switch (speed) {
        case 50: speed_val = B50; break;
        case 75: speed_val = B75; break;
        case 110: speed_val = B110; break;
        case 134: speed_val = B134; break;
        case 150: speed_val = B150; break;
        case 200: speed_val = B200; break;
        case 300: speed_val = B300; break;
        case 600: speed_val = B600; break;
        case 1200: speed_val = B1200; break;
        case 1800: speed_val = B1800; break;
        case 2400: speed_val = B2400; break;
        case 4800: speed_val = B4800; break;
        case 9600: speed_val = B9600; break;
        case 19200: speed_val = B19200; break;
        case 38400: speed_val = B38400; break;
        case 57600: speed_val = B57600; break;
        case 115200: speed_val = B115200; break;
        case 230400: speed_val = B230400; break;
        case 460800: speed_val = B460800; break;
        case 500000: speed_val = B500000; break;
        case 576000: speed_val = B576000; break;
        case 921600: speed_val = B921600; break;
        case 1000000: speed_val = B1000000; break;
        case 1152000: speed_val = B1152000; break;
        case 1500000: speed_val = B1500000; break;
        case 2000000: speed_val = B2000000; break;
        case 2500000: speed_val = B2500000; break;
        case 3000000: speed_val = B3000000; break;
        case 3500000: speed_val = B3500000; break;
        case 4000000: speed_val = B4000000; break;
        default: speed_val = B0; break;
    }
    cfsetospeed(&serial->tio, speed_val);
    cfsetispeed(&serial->tio, speed_val);
}


static void parse_serial_params(SerialPort * serial, const char * serial_params) {
    char * tmp = g_strdup(serial_params);
    char * device = NULL, * speed = NULL, * mode = NULL;
    if (!(device = strtok(tmp, ",")) ||
        !(speed = strtok(NULL, ",")) ||
        !(mode = strtok(NULL, ","))) {
        set_serial_params(serial, "/dev/ttyS0", "9600", "8N1");
    } else {
        set_serial_params(serial, device, speed, mode);
    }
    g_free(tmp);
}


static void send_cb(GObject * source, GAsyncResult * res, gpointer user_data) {
    GError *error = NULL;
    SpicePortChannel * channel = (SpicePortChannel *)source;
    SerialPortBuffer * buffer = (SerialPortBuffer *)user_data;
    spice_port_channel_write_finish(channel, res, &error);
    g_debug("Data sent to serial port %d\n", (int)(buffer->serial - serial_ports));
    if (error) {
        g_warning("Error sending data to guest: %s", error->message);
        g_clear_error(&error);
    }
    g_free(user_data);
}


static void read_cb(GObject * source, GAsyncResult * res, gpointer user_data) {
    GError *error = NULL;
    GInputStream * istream = (GInputStream *)source;
    SerialPortBuffer * buffer;
    SerialPort * serial;
    if (istream) {
        buffer = (SerialPortBuffer *)user_data;
        serial = buffer->serial;
        if (g_input_stream_read_finish(istream, res, &error) == 1) {
            g_debug("Data from serial device %s: 0x%.2hhX\n",
                       serial->device_name, buffer->data);
            spice_port_channel_write_async(serial->channel, &buffer->data, 1,
                                           serial->cancellable, send_cb, buffer);
        } else {
            g_warning("Error reading from serial device: %s", error->message);
            g_free(user_data);
            g_clear_error(&error);
            return;
        }
    } else {
        serial = (SerialPort *)user_data;
    }
    buffer = get_buffer(serial);
    g_input_stream_read_async(serial->istream, &buffer->data, 1, G_PRIORITY_DEFAULT,
                              serial->cancellable, read_cb, buffer);
}


static void write_cb(GObject * source, GAsyncResult * res, gpointer user_data) {
    GError * error = NULL;
    GOutputStream * ostream = (GOutputStream *)source;
    g_output_stream_write_finish(ostream, res, &error);
    if (error) {
        g_warning("Error writing to serial device: %s\n", error->message);
    }
    g_clear_error(&error);
    g_free(user_data);
}


static void open_serial(SerialPort * serial) {
    if (serial->fd > 0) {
        close_serial(serial);
    }

    serial->fd = open(serial->device_name, O_RDWR | O_NONBLOCK);
    if (serial->fd < 0) {
        g_warning("Could not open %s\n", serial->device_name);
        return;
    }
    if (tcsetattr(serial->fd, TCSANOW, &serial->tio) < 0) {
        g_warning("Could not set termios attributes\n");
        return;
    }
    serial->istream = g_unix_input_stream_new(serial->fd, FALSE);
    serial->ostream = g_unix_output_stream_new(serial->fd, FALSE);
    read_cb(NULL, NULL, serial);
}


static void serial_port_data(SpicePortChannel * channel, gpointer data, int size);

void serial_port_open(SpiceChannel * channel) {
    gchar * name = NULL;
    gboolean opened = FALSE;
    g_object_get(channel, "port-name", &name, "port-opened", &opened, NULL);
    if (!strncmp(name, "serialredir", 11)) {
        int port_number = atoi(&name[11]);
        if (port_number < 0 || port_number >= num_ports) return;
        SerialPort * serial = &serial_ports[port_number];
        if (opened) {
            g_signal_connect(channel, "port-data", G_CALLBACK(serial_port_data), NULL);
            g_debug("Opened channel %s for serial port %d\n", name, port_number);
            serial->cancellable = g_cancellable_new();
            serial->channel = SPICE_PORT_CHANNEL(channel);
            parse_serial_params(serial, serial_params[port_number]);
            open_serial(serial);
        } else {
            serial->channel = NULL;
            close_serial(serial);
            g_object_unref(serial->cancellable);
            serial->cancellable = NULL;
            g_signal_handlers_disconnect_by_func(channel, G_CALLBACK(serial_port_data), NULL);
        }
    }
}


static void serial_port_data(SpicePortChannel * channel, gpointer data, int size) {
    SerialPort * serial = get_serial_port(channel);
    if (serial) {
        g_debug("Data to serial device: 0x%.2hhX\n", *((char *)data));
        gpointer wbuffer = g_memdup(data, size);
        g_output_stream_write_async(serial->ostream, wbuffer, size, G_PRIORITY_DEFAULT,
                                    serial->cancellable, write_cb, wbuffer);
    }
}
