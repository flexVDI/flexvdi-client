#include <stdlib.h>
#include <gtk/gtk.h>

#include "client-win.h"

struct _ClientAppWindow {
    GtkApplicationWindow parent;
    ClientConf * conf;
    GtkLabel * version;
    GtkLabel * info;
    GtkLabel * status;
    GtkStack * stack;
    GtkEntry * host;
    GtkCheckButton * fullscreen;
    GtkButton * config;
    GtkButton * save;
    GtkButton * login;
    GtkEntry * username;
    GtkEntry * password;
    GtkTreeView * desktops;
    GtkListStore * desk_store;
};

enum {
    CLIENT_APP_CONFIG_BUTTON_PRESSED = 0,
    CLIENT_APP_SAVE_BUTTON_PRESSED,
    CLIENT_APP_LOGIN_BUTTON_PRESSED,
    CLIENT_APP_DESKTOP_SELECTED,
    CLIENT_APP_LAST_SIGNAL
};

static guint signals[CLIENT_APP_LAST_SIGNAL];

G_DEFINE_TYPE(ClientAppWindow, client_app_window, GTK_TYPE_APPLICATION_WINDOW);

static void save_config(ClientAppWindow * win);

static void button_pressed_handler(GtkButton * button, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    if (button == win->config)
        g_signal_emit(win, signals[CLIENT_APP_CONFIG_BUTTON_PRESSED], 0);
    else if (button == win->save) {
        save_config(win);
        g_signal_emit(win, signals[CLIENT_APP_SAVE_BUTTON_PRESSED], 0);
    } else if (button == win->login)
        g_signal_emit(win, signals[CLIENT_APP_LOGIN_BUTTON_PRESSED], 0);
}

static void entry_activate_handler(GtkEntry * entry, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    if (entry == win->password)
        g_signal_emit(win, signals[CLIENT_APP_LOGIN_BUTTON_PRESSED], 0);
}

static void desktop_selected_handler(GtkTreeView * tree_view, GtkTreePath * path,
                                     GtkTreeViewColumn * column, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    g_signal_emit(win, signals[CLIENT_APP_DESKTOP_SELECTED], 0);
}

static void client_app_window_init(ClientAppWindow * win) {
    GtkCssProvider * css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/com/flexvdi/client/style.css");
    GdkScreen * screen = gtk_widget_get_screen(GTK_WIDGET(win));
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    gtk_widget_init_template(GTK_WIDGET(win));

    gtk_label_set_text(win->version, "version " VERSION_STRING);
    g_signal_connect(win->config, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->save, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->login, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->password, "activate", G_CALLBACK(entry_activate_handler), win);
    win->desk_store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_tree_view_set_model(win->desktops, GTK_TREE_MODEL(win->desk_store));
    g_signal_connect(win->desktops, "row-activated", G_CALLBACK(desktop_selected_handler), win);
}

static void client_app_window_dispose(GObject * obj) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(obj);
    g_clear_object(&win->conf);
    g_clear_object(&win->desk_store);
    G_OBJECT_CLASS(client_app_window_parent_class)->dispose(obj);
}

static void client_app_window_class_init(ClientAppWindowClass * class) {
    G_OBJECT_CLASS(class)->dispose = client_app_window_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                                "/com/flexvdi/client/window.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, version);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, info);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, status);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, stack);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, host);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, fullscreen);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, config);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, save);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, login);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, username);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, password);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, desktops);

    signals[CLIENT_APP_CONFIG_BUTTON_PRESSED] =
        g_signal_new("config-button-pressed",
                     CLIENT_APP_WINDOW_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[CLIENT_APP_SAVE_BUTTON_PRESSED] =
        g_signal_new("save-button-pressed",
                     CLIENT_APP_WINDOW_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[CLIENT_APP_LOGIN_BUTTON_PRESSED] =
        g_signal_new("login-button-pressed",
                     CLIENT_APP_WINDOW_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[CLIENT_APP_DESKTOP_SELECTED] =
        g_signal_new("desktop-selected",
                     CLIENT_APP_WINDOW_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}

static void load_config(ClientAppWindow * win) {
    const gchar * host = client_conf_get_host(win->conf);
    if (host) {
        if (client_conf_get_port(win->conf) == 443) {
            gtk_entry_set_text(win->host, host);
        } else {
            g_autofree gchar * host_str = g_strdup_printf("%s:%d", host, client_conf_get_port(win->conf));
            gtk_entry_set_text(win->host, host_str);
        }
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->fullscreen),
        client_conf_get_fullscreen(win->conf));
}

static void save_config(ClientAppWindow * win) {
    const gchar * host_port = gtk_entry_get_text(win->host);
    char ** tokens = g_strsplit(host_port, ":", 2);
    client_conf_set_host(win->conf, tokens[0]);
    int port = 443;
    if (tokens[1]) {
        port = atoi(tokens[1]);
        if (port < 1 || port > 65535) port = 443;
    }
    client_conf_set_port(win->conf, port);
    client_conf_set_fullscreen(win->conf,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->fullscreen)));
    g_strfreev(tokens);
}

void client_app_window_set_config(ClientAppWindow * win, ClientConf * conf) {
    win->conf = g_object_ref(conf);
    load_config(win);
}

void client_app_window_set_info(ClientAppWindow * win, const gchar * text) {
    gtk_label_set_text(win->info, text);
}

void client_app_window_set_status(ClientAppWindow * win, gboolean error, const gchar * text) {
    gtk_label_set_text(win->status, text);
    GtkStyleContext * style = gtk_widget_get_style_context(GTK_WIDGET(win->status));
    if (error) {
        gtk_style_context_add_class(style, "error");
    } else {
        gtk_style_context_remove_class(style, "error");
    }
}

void client_app_window_set_central_widget(ClientAppWindow * win, const gchar * name) {
    gtk_stack_set_visible_child_name(win->stack, name);
}

void client_app_window_set_central_widget_sensitive(ClientAppWindow * win, gboolean sensitive) {
    gtk_widget_set_sensitive(GTK_WIDGET(win->stack), sensitive);
}

ClientAppWindow * client_app_window_new(ClientApp * app) {
    return g_object_new(CLIENT_APP_WINDOW_TYPE, "application", app, NULL);
}

const gchar * client_app_window_get_username(ClientAppWindow * win) {
    return gtk_entry_get_text(win->username);
}

const gchar * client_app_window_get_password(ClientAppWindow * win) {
    return gtk_entry_get_text(win->password);
}

void client_app_window_set_desktops(ClientAppWindow * win, GList * desktop_names) {
    GList * name;
    GtkTreeIter it;
    gtk_list_store_clear(win->desk_store);
    for (name = desktop_names; name != NULL; name = name->next) {
        gtk_list_store_append(win->desk_store, &it);
        gtk_list_store_set(win->desk_store, &it, 0, name->data, -1);
    }
}
