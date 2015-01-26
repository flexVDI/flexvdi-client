/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include "printclient.h"


typedef struct PrintJob {
    gint fileHandle;
    gchar * name;
    uint32_t pendingData;
} PrintJob;

static GHashTable * printJobs;


void initPrintClient() {
    printJobs = g_hash_table_new_full(g_direct_hash, NULL, NULL, g_free);
}


static void openWithApp(const char * file) {
    char command[1024];
    snprintf(command, 1024, "xdg-open %s", file);
    system(command);
}


void handlePrintJob(FlexVDIPrintJobMsg * msg) {
    PrintJob * job = g_malloc(sizeof(PrintJob));
    job->fileHandle = g_file_open_tmp("fpjXXXXXX.pdf", &job->name, NULL);
    job->pendingData = msg->dataLength;
    g_hash_table_insert(printJobs, GINT_TO_POINTER(msg->id), job);
}


void handlePrintJobData(FlexVDIPrintJobDataMsg * msg) {
    PrintJob * job = g_hash_table_lookup(printJobs, GINT_TO_POINTER(msg->id));
    if (job) {
        write(job->fileHandle, msg->data, msg->dataLength);
        job->pendingData -= msg->dataLength;
        if (!job->pendingData) {
            close(job->fileHandle);
            openWithApp(job->name);
            g_free(job->name);
            g_hash_table_remove(printJobs, GINT_TO_POINTER(msg->id));
        }
    } else {
        printf("Job %d not found\n", msg->id);
    }
}
