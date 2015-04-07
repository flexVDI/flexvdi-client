/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include "spice-client.h"
#include "flexvdi-spice.h"
#include "flexvdi-cmdline.h"
#include "spice-util.h"


typedef struct SerialPort {
    char * deviceName;
    SpicePortChannel * channel;
    GCancellable * cancellable;
    int fd;
    struct termios tio;
    GInputStream * istream;
    GOutputStream * ostream;
} SerialPort;


static SerialPort * serialPorts;
static int numPorts;


static SerialPort * getSerialPort(SpicePortChannel * channel) {
    int portNumber;
    for (portNumber = 0; portNumber < numPorts; ++portNumber)
        if (serialPorts[portNumber].channel == channel)
            return &serialPorts[portNumber];
    return NULL;
}


typedef struct SerialPortBuffer {
    SerialPort * serial;
    char data;
} SerialPortBuffer;


static SerialPortBuffer * getBuffer(SerialPort * serial) {
    SerialPortBuffer * buffer = g_malloc(sizeof(SerialPortBuffer));
    buffer->serial = serial;
    return buffer;
}


static void closeSerial(SerialPort * serial) {
    SPICE_DEBUG("Closing serial device");
    g_cancellable_cancel(serial->cancellable);
    close(serial->fd);
    serial->fd = 0;
    g_object_unref(serial->istream);
    g_object_unref(serial->ostream);
}


static void setSerialParams(SerialPort * serial, const char * device,
                            const char * speedStr, const char * mode) {
    flexvdiLog(L_DEBUG, "Using serial device %s, with %sbps mode %.3s\n",
               device, speedStr, mode);

    g_free(serial->deviceName);
    serial->deviceName = g_strdup(device);

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

    speed_t speedValue = B0;
    int speed = atoi(speedStr);
    switch (speed) {
        case 50: speedValue = B50; break;
        case 75: speedValue = B75; break;
        case 110: speedValue = B110; break;
        case 134: speedValue = B134; break;
        case 150: speedValue = B150; break;
        case 200: speedValue = B200; break;
        case 300: speedValue = B300; break;
        case 600: speedValue = B600; break;
        case 1200: speedValue = B1200; break;
        case 1800: speedValue = B1800; break;
        case 2400: speedValue = B2400; break;
        case 4800: speedValue = B4800; break;
        case 9600: speedValue = B9600; break;
        case 19200: speedValue = B19200; break;
        case 38400: speedValue = B38400; break;
        case 57600: speedValue = B57600; break;
        case 115200: speedValue = B115200; break;
        case 230400: speedValue = B230400; break;
        case 460800: speedValue = B460800; break;
        case 500000: speedValue = B500000; break;
        case 576000: speedValue = B576000; break;
        case 921600: speedValue = B921600; break;
        case 1000000: speedValue = B1000000; break;
        case 1152000: speedValue = B1152000; break;
        case 1500000: speedValue = B1500000; break;
        case 2000000: speedValue = B2000000; break;
        case 2500000: speedValue = B2500000; break;
        case 3000000: speedValue = B3000000; break;
        case 3500000: speedValue = B3500000; break;
        case 4000000: speedValue = B4000000; break;
        default: speedValue = B0; break;
    }
    cfsetospeed(&serial->tio, speedValue);
    cfsetispeed(&serial->tio, speedValue);
}


static void parseSerialParams(SerialPort * serial, const char * serialParams) {
    char * tmp = g_strdup(serialParams);
    char * device = NULL, * speed = NULL, * mode = NULL;
    if (!(device = strtok(tmp, ",")) ||
         !(speed = strtok(NULL, ",")) ||
         !(mode = strtok(NULL, ","))) {
        setSerialParams(serial, "/dev/ttyS0", "9600", "8N1");
    } else {
        setSerialParams(serial, device, speed, mode);
    }
    g_free(tmp);
}


static void sendCallback(GObject * sourceObject, GAsyncResult * res,
                         gpointer userData) {
    GError *error = NULL;
    SpicePortChannel * channel = (SpicePortChannel *)sourceObject;
    SerialPortBuffer * buffer = (SerialPortBuffer *)userData;
    spice_port_write_finish(channel, res, &error);
    flexvdiLog(L_DEBUG, "Data sent to serial port %d\n", buffer->serial - serialPorts);
    if (error) {
        g_warning("Error sending data to guest: %s", error->message);
        g_clear_error(&error);
    }
    g_free(userData);
}


static void readCallback(GObject * sourceObject, GAsyncResult * res,
                         gpointer userData) {
    GError *error = NULL;
    GInputStream * istream = (GInputStream *)sourceObject;
    SerialPortBuffer * buffer;
    SerialPort * serial;
    if (istream) {
        buffer = (SerialPortBuffer *)userData;
        serial = buffer->serial;
        if (g_input_stream_read_finish(istream, res, &error) == 1) {
            flexvdiLog(L_DEBUG, "Data from serial device %s: 0x%.2hhX\n",
                       serial->deviceName, buffer->data);
            spice_port_write_async(serial->channel, &buffer->data, 1, serial->cancellable,
                                   sendCallback, buffer);
        } else {
            g_warning("Error reading from serial device: %s", error->message);
            g_free(userData);
            g_clear_error(&error);
            return;
        }
    } else {
        serial = (SerialPort *)userData;
    }
    buffer = getBuffer(serial);
    g_input_stream_read_async(serial->istream, &buffer->data, 1, G_PRIORITY_DEFAULT,
                              serial->cancellable, readCallback, buffer);
}


static void writeCallback(GObject * sourceObject, GAsyncResult * res,
                          gpointer userData) {
    GError * error = NULL;
    GOutputStream * ostream = (GOutputStream *)sourceObject;
    g_output_stream_write_finish(ostream, res, &error);
    if (error) {
        g_warning("Error writing to serial device: %s\n", error->message);
    }
    g_clear_error(&error);
    g_free(userData);
}


static void openSerial(SerialPort * serial) {
    if (serial->fd > 0) {
        closeSerial(serial);
    }

    serial->fd = open(serial->deviceName, O_RDWR | O_NONBLOCK);
    if (serial->fd < 0) {
        g_warning("Could not open %s\n", serial->deviceName);
        return;
    }
    if (tcsetattr(serial->fd, TCSANOW, &serial->tio) < 0) {
        g_warning("Could not set termios attributes\n");
        return;
    }
    serial->istream = g_unix_input_stream_new(serial->fd, FALSE);
    serial->ostream = g_unix_output_stream_new(serial->fd, FALSE);
    readCallback(NULL, NULL, serial);
}


static void allocSerialPorts() {
    numPorts = 0;
    const gchar ** serialParams = getSerialPortParams();
    if (serialParams) {
        while (serialParams[numPorts]) ++numPorts;
        serialPorts = g_malloc0(sizeof(SerialPort) * numPorts);
    }
}


void serialPortOpened(SpiceChannel * channel) {
    gchar * name = NULL;
    gboolean opened = FALSE;
    g_object_get(channel, "port-name", &name, "port-opened", &opened, NULL);
    if (!strncmp(name, "serialredir", 11)) {
        if (!serialPorts) allocSerialPorts();
        int portNumber = atoi(&name[11]);
        if (opened && portNumber >= 0 && portNumber < numPorts) {
            flexvdiLog(L_DEBUG, "Opened channel %s for serial port %d\n", name, portNumber);
            SerialPort * serial = &serialPorts[portNumber];
            serial->cancellable = g_cancellable_new();
            serial->channel = SPICE_PORT_CHANNEL(channel);
            parseSerialParams(serial, getSerialPortParams()[portNumber]);
            openSerial(serial);
        }
    }
}


void serialPortData(SpicePortChannel * channel, gpointer data, int size) {
    SerialPort * serial = getSerialPort(channel);
    if (serial) {
        flexvdiLog(L_DEBUG, "Data to serial device: 0x%.2hhX\n", *((char *)data));
        gpointer wbuffer = g_memdup(data, size);
        g_output_stream_write_async(serial->ostream, wbuffer, size, G_PRIORITY_DEFAULT,
                                    serial->cancellable, writeCallback, wbuffer);
    }
}


void serialChannelDestroy(SpicePortChannel * channel) {
    if (SPICE_IS_PORT_CHANNEL(channel)) {
        SerialPort * serial = getSerialPort(channel);
        if (serial) {
            serial->channel = NULL;
            closeSerial(serial);
            g_object_unref(serial->cancellable);
            serial->cancellable = NULL;
        }
    }
}
