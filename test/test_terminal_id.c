#include <stdio.h>
#include <glib.h>

gchar * discover_terminal_id();

int main(int argc, char * argv[]) {
    g_autofree gchar * terminal_id = discover_terminal_id();
    printf("Terminal ID is '%s'\n", terminal_id);
    return 0;
}

