/*
 * Control window for SHARP MZ-1500 on Solaris using GTK2+.
 * Copyright (c) 2026 Suikasama1968
 */

#include "osd_control.h"
#include "../config.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <gtk/gtk.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern config_t config;

struct MenuSpec {
    const char *label;
    const char *command;
    bool needs_path;
    bool save_dialog;
};

struct ControlWidgetData {
    SOLARIS_CONTROL_WINDOW *owner;
    GtkWidget *window;
    GtkWidget *cmt_label;
    GtkWidget *qd_label;
};

struct OptionSpec {
    const char *label;
    uint32_t mask;
};

struct FrequencySpec {
    const char *label;
    int value;
};

struct LatencySpec {
    const char *label;
    int value;
};

struct FrameSkipSpec {
    const char *label;
    int value;
};

// Control
static const MenuSpec control_menu[] = {
    { "Reset",  "reset",  false, false },
    { "Exit",   "exit",   false, false }
};

// CMT
static const MenuSpec cmt_menu[] = {
    { "Play...",      "cmt",      true,  false },
    { "Rec...",       "cmtrec",   true,  true  },
    { "Eject",        "cmteject", false, false },
    { "Play Button",  "cmtplay",  false, false },
    { "Stop Button",  "cmtstop",  false, false },
    { "Fast Forward", "cmtff",    false, false },
    { "Fast Rewind",  "cmtrew",   false, false }
};

// QD
static const MenuSpec qd_menu[] = {
    { "Insert...", "qd",      true,  false },
    { "Eject",     "qdeject", false, false }
};

// Device -> Option
static const OptionSpec option_menu[] = {
    { "MZ-1E05 (FD I/F)",    1u << 8  },
    { "MZ-1R12 (CMOS RAM)",  1u << 10 },
    { "MZ-1R18 (RAM File)",  1u << 11 },
    { "MZ-1R23 (Kanji ROM)", 1u << 12 },
    { "MZ-1R24 (Dict. ROM)", 1u << 13 },
    { "PIO-3034 (EMM)",      1u << 14 }
};

// Host -> Sound
static const FrequencySpec frequency_menu[] = {
    { "2000Hz",  0 },
    { "4000Hz",  1 },
    { "8000Hz",  2 },
    { "11025Hz", 3 },
    { "22050Hz", 4 },
    { "44100Hz", 5 },
    { "48000Hz", 6 },
    { "96000Hz", 7 }
};

static const LatencySpec latency_menu[] = {
    { "50msec",  0 },
    { "100msec", 1 },
    { "200msec", 2 },
    { "300msec", 3 },
    { "400msec", 4 }
};

// Host -> Frame Skip
static const FrameSkipSpec frame_skip_menu[] = {
    { "1", 1 },
    { "2", 2 },
    { "4", 4 },
    { "8", 8 }
};

static const char *file_basename(const std::string& path)
{
    const char *name = path.c_str();
    const char *p = strrchr(name, '/');
    return p != NULL ? p + 1 : name;
}

static void set_label_text(GtkWidget *label, const char *prefix, const std::string& path)
{
    if(label == NULL) return;
    std::string text = prefix;
    if(!path.empty()) {
        text += file_basename(path);
    }
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
}

struct FileDialogState {
    bool done;
    std::string path;

    FileDialogState() : done(false) {}
};

static void file_dialog_response(GtkDialog *dialog, gint response_id, gpointer data)
{
    FileDialogState *state = (FileDialogState *)data;
    if(state == NULL) return;

    if(response_id == GTK_RESPONSE_OK) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if(filename != NULL) {
            state->path = filename;
            g_free(filename);
        }
    }
    state->done = true;
}

static gboolean file_dialog_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    FileDialogState *state = (FileDialogState *)data;
    if(state != NULL) {
        state->done = true;
    }
    return TRUE;
}

static std::string choose_file(SOLARIS_CONTROL_WINDOW *owner, GtkWidget *parent, const char *title, bool save_dialog)
{
    GtkFileChooserAction action = save_dialog ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN;
    const char *accept_label = save_dialog ? "_Save" : "_Open";
    GtkWidget *dialog = gtk_file_chooser_dialog_new(title,
                                                    GTK_WINDOW(parent),
                                                    action,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    accept_label, GTK_RESPONSE_OK,
                                                    NULL);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);

    FileDialogState state;
    g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(file_dialog_response), &state);
    g_signal_connect(G_OBJECT(dialog), "delete-event", G_CALLBACK(file_dialog_delete), &state);
    gtk_widget_show_all(dialog);

    while(!state.done && owner != NULL && !owner->stop_requested()) {
        if(gtk_events_pending()) {
            gtk_main_iteration_do(FALSE);
        } else {
            g_usleep(20000);
        }
    }

    gtk_widget_destroy(dialog);
    while(gtk_events_pending()) {
        gtk_main_iteration_do(FALSE);
    }
    return owner != NULL && owner->stop_requested() ? std::string() : state.path;
}

// Quote file paths before passing them through the shared command parser.
static std::string quote_command_path(const std::string& path)
{
    return "\"" + path + "\"";
}

static void queue_command(ControlWidgetData *widget_data, const char *command, bool needs_path, bool save_dialog)
{
    if(command == NULL || widget_data == NULL || widget_data->owner == NULL) {
        return;
    }

    if(!needs_path && strcmp(command, "reset") == 0) {
        widget_data->owner->request_reset();
        return;
    }

    if(needs_path) {
        const char *title = "Select image file";
        if(strcmp(command, "cmt") == 0) {
            title = "Data Recoder Tape [Play]";
        } else if(strcmp(command, "cmtrec") == 0) {
            title = "Data Recoder Tape [Rec]";
        } else if(strcmp(command, "qd") == 0) {
            title = "Quick Disk";
        }
        std::string path = choose_file(widget_data->owner, widget_data->window, title, save_dialog);
        if(path.empty()) return;
        widget_data->owner->push_command(std::string(command) + " " + quote_command_path(path));
        if(strcmp(command, "cmt") == 0 || strcmp(command, "cmtrec") == 0) {
            set_label_text(widget_data->cmt_label, "CMT : ", path);
        } else if(strcmp(command, "qd") == 0) {
            set_label_text(widget_data->qd_label, "QD   : ", path);
        }
    } else {
        widget_data->owner->push_command(command);
        if(strcmp(command, "cmteject") == 0) {
            set_label_text(widget_data->cmt_label, "CMT : ", std::string());
        } else if(strcmp(command, "qdeject") == 0) {
            set_label_text(widget_data->qd_label, "QD   : ", std::string());
        }
    }
}

// Menu callbacks stay thin: translate the selected item to a command string
// and let the host thread touch VM state.
static void queue_button_command(GtkWidget *button, gpointer data)
{
    const char *command = (const char *)g_object_get_data(G_OBJECT(button), "command");
    bool needs_path = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "needs_path")) != 0;
    bool save_dialog = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "save_dialog")) != 0;
    queue_command((ControlWidgetData *)data, command, needs_path, save_dialog);
}

static gboolean close_control_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    bool *running = (bool *)data;

    if(running != NULL) {
        *running = false;
    }
    return TRUE;
}

static GtkWidget *create_menu_item(const MenuSpec& spec, ControlWidgetData *widget_data)
{
    GtkWidget *item = gtk_menu_item_new_with_label(spec.label);
    g_object_set_data(G_OBJECT(item), "command", (gpointer)spec.command);
    g_object_set_data(G_OBJECT(item), "needs_path", GINT_TO_POINTER(spec.needs_path ? 1 : 0));
    g_object_set_data(G_OBJECT(item), "save_dialog", GINT_TO_POINTER(spec.save_dialog ? 1 : 0));
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(queue_button_command), widget_data);
    return item;
}

static GtkWidget *create_menu(const char *label, const MenuSpec *items, size_t count, ControlWidgetData *widget_data)
{
    GtkWidget *root = gtk_menu_item_new_with_label(label);
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(root), menu);
    for(size_t i = 0; i < count; i++) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), create_menu_item(items[i], widget_data));
    }
    return root;
}

static void queue_option_command(GtkCheckMenuItem *item, gpointer data)
{
    ControlWidgetData *widget_data = (ControlWidgetData *)data;
    if(widget_data == NULL || widget_data->owner == NULL) return;

    uint32_t mask = (uint32_t)(uintptr_t)g_object_get_data(G_OBJECT(item), "mask");
    bool active = gtk_check_menu_item_get_active(item) != FALSE;
    char command[64];
    snprintf(command, sizeof(command), "option %u %d", (unsigned)mask, active ? 1 : 0);
    widget_data->owner->push_command(command);
}

static void queue_frequency_command(GtkCheckMenuItem *item, gpointer data)
{
    if(!gtk_check_menu_item_get_active(item)) return;

    ControlWidgetData *widget_data = (ControlWidgetData *)data;
    if(widget_data == NULL || widget_data->owner == NULL) return;

    int value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "frequency"));
    char command[32];
    snprintf(command, sizeof(command), "frequency %d", value);
    widget_data->owner->push_command(command);
}

static void queue_latency_command(GtkCheckMenuItem *item, gpointer data)
{
    if(!gtk_check_menu_item_get_active(item)) return;

    ControlWidgetData *widget_data = (ControlWidgetData *)data;
    if(widget_data == NULL || widget_data->owner == NULL) return;

    int value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "latency"));
    char command[32];
    snprintf(command, sizeof(command), "latency %d", value);
    widget_data->owner->push_command(command);
}

static void queue_frame_skip_command(GtkCheckMenuItem *item, gpointer data)
{
    if(!gtk_check_menu_item_get_active(item)) return;

    ControlWidgetData *widget_data = (ControlWidgetData *)data;
    if(widget_data == NULL || widget_data->owner == NULL) return;

    int value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "frame_skip"));
    char command[32];
    snprintf(command, sizeof(command), "frameskip %d", value);
    widget_data->owner->push_command(command);
}

static GtkWidget *create_device_menu(ControlWidgetData *widget_data)
{
    GtkWidget *device_root = gtk_menu_item_new_with_label("Device");
    GtkWidget *device_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(device_root), device_menu);

    GtkWidget *option_root = gtk_menu_item_new_with_label("Option");
    GtkWidget *option_submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(option_root), option_submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(device_menu), option_root);

    for(size_t i = 0; i < sizeof(option_menu) / sizeof(option_menu[0]); i++) {
        GtkWidget *item = gtk_check_menu_item_new_with_label(option_menu[i].label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), (config.option_switch & option_menu[i].mask) != 0);
        g_object_set_data(G_OBJECT(item), "mask", (gpointer)(uintptr_t)option_menu[i].mask);
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(queue_option_command), widget_data);
        gtk_menu_shell_append(GTK_MENU_SHELL(option_submenu), item);
    }

    return device_root;
}

static GtkWidget *create_host_menu(ControlWidgetData *widget_data)
{
    GtkWidget *host_root = gtk_menu_item_new_with_label("Host");
    GtkWidget *host_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(host_root), host_menu);

    GtkWidget *sound_root = gtk_menu_item_new_with_label("Sound");
    GtkWidget *sound_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(sound_root), sound_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(host_menu), sound_root);

    GSList *group = NULL;
    for(size_t i = 0; i < sizeof(frequency_menu) / sizeof(frequency_menu[0]); i++) {
        GtkWidget *item = gtk_radio_menu_item_new_with_label(group, frequency_menu[i].label);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), config.sound_frequency == frequency_menu[i].value);
        g_object_set_data(G_OBJECT(item), "frequency", GINT_TO_POINTER(frequency_menu[i].value));
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(queue_frequency_command), widget_data);
        gtk_menu_shell_append(GTK_MENU_SHELL(sound_menu), item);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(sound_menu), gtk_separator_menu_item_new());

    group = NULL;
    for(size_t i = 0; i < sizeof(latency_menu) / sizeof(latency_menu[0]); i++) {
        GtkWidget *item = gtk_radio_menu_item_new_with_label(group, latency_menu[i].label);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), config.sound_latency == latency_menu[i].value);
        g_object_set_data(G_OBJECT(item), "latency", GINT_TO_POINTER(latency_menu[i].value));
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(queue_latency_command), widget_data);
        gtk_menu_shell_append(GTK_MENU_SHELL(sound_menu), item);
    }

    GtkWidget *frame_skip_root = gtk_menu_item_new_with_label("Frame Skip");
    GtkWidget *frame_skip_submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(frame_skip_root), frame_skip_submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(host_menu), frame_skip_root);

    group = NULL;
    for(size_t i = 0; i < sizeof(frame_skip_menu) / sizeof(frame_skip_menu[0]); i++) {
        GtkWidget *item = gtk_radio_menu_item_new_with_label(group, frame_skip_menu[i].label);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), config.solaris_frame_skip == frame_skip_menu[i].value);
        g_object_set_data(G_OBJECT(item), "frame_skip", GINT_TO_POINTER(frame_skip_menu[i].value));
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(queue_frame_skip_command), widget_data);
        gtk_menu_shell_append(GTK_MENU_SHELL(frame_skip_submenu), item);
    }

    return host_root;
}

SOLARIS_CONTROL_WINDOW::SOLARIS_CONTROL_WINDOW()
    : thread_started_(false), stop_requested_(false), reset_requested_(false)
{
}

SOLARIS_CONTROL_WINDOW::~SOLARIS_CONTROL_WINDOW()
{
    stop();
}

bool SOLARIS_CONTROL_WINDOW::start(const std::string& initial_cmt, const std::string& initial_qd)
{
    initial_cmt_ = initial_cmt;
    initial_qd_ = initial_qd;
    stop_requested_ = false;
    reset_requested_ = false;
    if(pthread_create(&thread_, NULL, thread_proc, this) != 0) {
        fprintf(stderr, "pthread_create(control window) failed\n");
        return false;
    }
    thread_started_ = true;
    return true;
}

void SOLARIS_CONTROL_WINDOW::stop()
{
    if(thread_started_) {
        stop_requested_ = true;
        pthread_join(thread_, NULL);
        thread_started_ = false;
    }
}

bool SOLARIS_CONTROL_WINDOW::stop_requested() const
{
    return stop_requested_.load();
}

bool SOLARIS_CONTROL_WINDOW::pop_reset_request()
{
    return reset_requested_.exchange(false);
}

void SOLARIS_CONTROL_WINDOW::request_reset()
{
    reset_requested_ = true;
}

bool SOLARIS_CONTROL_WINDOW::pop_command(std::string *command)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(commands_.empty()) return false;
    *command = commands_.front();
    commands_.pop();
    return true;
}

void SOLARIS_CONTROL_WINDOW::push_command(const std::string& command)
{
    if(command.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push(command);
}

// GTK runs on a separate thread so SDL input/video and VM execution can keep
// their simple main-loop ownership.
void* SOLARIS_CONTROL_WINDOW::thread_proc(void *arg)
{
    ((SOLARIS_CONTROL_WINDOW *)arg)->thread_main();
    return NULL;
}

void SOLARIS_CONTROL_WINDOW::thread_main()
{
    int argc = 0;
    char **argv = NULL;
    if(!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "gtk_init_check(control window) failed\n");
        return;
    }
    GtkSettings *settings = gtk_settings_get_default();
    if(settings != NULL) {
        g_object_set(settings, "gtk-button-images", FALSE, NULL);
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MZ-1500 Control");
    gtk_window_set_default_size(GTK_WINDOW(window), 420, 110);
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);
    bool running = true;
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(close_control_window), &running);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    ControlWidgetData widget_data;
    widget_data.owner = this;
    widget_data.window = window;
    widget_data.cmt_label = NULL;
    widget_data.qd_label = NULL;

    GtkWidget *menu_bar = gtk_menu_bar_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), create_menu("Control", control_menu, sizeof(control_menu) / sizeof(control_menu[0]), &widget_data));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), create_menu("CMT", cmt_menu, sizeof(cmt_menu) / sizeof(cmt_menu[0]), &widget_data));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), create_menu("QD", qd_menu, sizeof(qd_menu) / sizeof(qd_menu[0]), &widget_data));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), create_device_menu(&widget_data));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), create_host_menu(&widget_data));
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

    GtkWidget *content = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    gtk_box_pack_start(GTK_BOX(vbox), content, TRUE, TRUE, 0);

    GtkWidget *cmt_label = gtk_label_new("CMT : ");
    gtk_misc_set_alignment(GTK_MISC(cmt_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(content), cmt_label, FALSE, FALSE, 0);
    widget_data.cmt_label = cmt_label;
    set_label_text(widget_data.cmt_label, "CMT : ", initial_cmt_);

    GtkWidget *qd_label = gtk_label_new("QD   : ");
    gtk_misc_set_alignment(GTK_MISC(qd_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(content), qd_label, FALSE, FALSE, 0);
    widget_data.qd_label = qd_label;
    set_label_text(widget_data.qd_label, "QD   : ", initial_qd_);

    gtk_widget_show_all(window);

    while(running && !stop_requested_.load()) {
        while(gtk_events_pending()) {
            gtk_main_iteration_do(FALSE);
        }
        g_usleep(20000);
    }

    if(GTK_IS_WIDGET(window)) {
        gtk_widget_destroy(window);
    }
}
