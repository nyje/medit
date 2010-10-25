/*
 *   mooapp/mooapp.c
 *
 *   Copyright (C) 2004-2008 by Yevgen Muntyan <muntyan@tamu.edu>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MOO_APP_COMPILATION
#define WANT_MOO_APP_CMD_CHARS
#include "mooapp-private.h"
#include "eggsmclient/eggsmclient.h"
#include "mooapp-accels.h"
#include "mooapp-info.h"
#include "mooappabout.h"
#include "moolua/moolua.h"
#include "mooedit/mooeditprefs.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooplugin.h"
#include "mooutils/mooprefsdialog.h"
#include "mooutils/moopython.h"
#include "marshals.h"
#include "mooutils/mooappinput.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moostock.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/mooutils-debug.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moo-mime.h"
#include "mooutils/moohelp.h"
#include <glib/gmappedfile.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#ifdef MOO_USE_QUARTZ
#include <ige-mac-dock.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef VERSION
#define APP_VERSION VERSION
#else
#define APP_VERSION "<uknown version>"
#endif

#define MOO_UI_XML_FILE     "ui.xml"
#ifdef __WIN32__
#define MOO_ACTIONS_FILE    "actions.ini"
#else
#define MOO_ACTIONS_FILE    "actions"
#endif

#define SESSION_VERSION "1.0"

static struct {
    MooApp *instance;
    gboolean atexit_installed;
} moo_app_data;

static volatile int signal_received;

struct MooAppPrivate {
    MooEditor  *editor;
    char       *rc_files[2];

    gboolean    run_input;
    char       *instance_name;

    gboolean    running;
    gboolean    in_try_quit;

    int         use_session;
    EggSMClient *sm_client;
    char       *session_file;
    MooMarkupDoc *session;

    MooUiXml   *ui_xml;
    const char *default_ui;
    guint       quit_handler_id;

#ifdef MOO_USE_QUARTZ
    IgeMacDock *dock;
#endif
};


static void     moo_app_class_init      (MooAppClass        *klass);
static void     moo_app_instance_init   (MooApp             *app);
static GObject *moo_app_constructor     (GType               type,
                                         guint               n_params,
                                         GObjectConstructParam *params);
static void     moo_app_finalize        (GObject            *object);

static void     install_common_actions  (void);
static void     install_editor_actions  (void);

static void     moo_app_help            (GtkWidget          *window);
static void     moo_app_report_bug      (GtkWidget          *window);

static void     moo_app_set_property    (GObject            *object,
                                         guint               prop_id,
                                         const GValue       *value,
                                         GParamSpec         *pspec);
static void     moo_app_get_property    (GObject            *object,
                                         guint               prop_id,
                                         GValue             *value,
                                         GParamSpec         *pspec);

static gboolean moo_app_init_real       (MooApp             *app);
static int      moo_app_run_real        (MooApp             *app);
static void     moo_app_quit_real       (MooApp             *app);
static gboolean moo_app_try_quit_real   (MooApp             *app);
static void     moo_app_exec_cmd        (MooApp             *app,
                                         char                cmd,
                                         const char         *data,
                                         guint               len);
static void     moo_app_load_session_real (MooApp           *app,
                                         MooMarkupNode      *xml);
static void     moo_app_save_session_real (MooApp           *app,
                                         MooMarkupNode      *xml);
static GtkWidget *moo_app_create_prefs_dialog (MooApp       *app);

static void     moo_app_load_prefs      (MooApp             *app);
static void     moo_app_save_prefs      (MooApp             *app);

static void     moo_app_save_session    (MooApp             *app);
static void     moo_app_write_session   (MooApp             *app);

static void     moo_app_install_cleanup (void);
static void     moo_app_cleanup         (void);

static void     start_input             (MooApp             *app);

static void     moo_app_cmd_open_files  (MooApp             *app,
                                         const char         *data);


static GObjectClass *moo_app_parent_class;

GType
moo_app_get_type (void)
{
    static GType type = 0;

    if (G_UNLIKELY (!type))
    {
        static const GTypeInfo type_info = {
            sizeof (MooAppClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) moo_app_class_init,
            (GClassFinalizeFunc) NULL,
            NULL,   /* class_data */
            sizeof (MooApp),
            0,      /* n_preallocs */
            (GInstanceInitFunc) moo_app_instance_init,
            NULL    /* value_table */
        };

        type = g_type_register_static (G_TYPE_OBJECT, "MooApp",
                                       &type_info, 0);
    }

    return type;
}


enum {
    PROP_0,
    PROP_RUN_INPUT,
    PROP_USE_SESSION,
    PROP_DEFAULT_UI,
    PROP_INSTANCE_NAME
};

enum {
    INIT,
    RUN,
    QUIT,
    TRY_QUIT,
    PREFS_DIALOG,
    LOAD_SESSION,
    SAVE_SESSION,
    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL];


static void
moo_app_class_init (MooAppClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    moo_app_parent_class = g_type_class_peek_parent (klass);

    gobject_class->constructor = moo_app_constructor;
    gobject_class->finalize = moo_app_finalize;
    gobject_class->set_property = moo_app_set_property;
    gobject_class->get_property = moo_app_get_property;

    klass->init = moo_app_init_real;
    klass->run = moo_app_run_real;
    klass->quit = moo_app_quit_real;
    klass->try_quit = moo_app_try_quit_real;
    klass->prefs_dialog = moo_app_create_prefs_dialog;
    klass->load_session = moo_app_load_session_real;
    klass->save_session = moo_app_save_session_real;

    g_object_class_install_property (gobject_class,
                                     PROP_RUN_INPUT,
                                     g_param_spec_boolean ("run-input",
                                             "run-input",
                                             "run-input",
                                             TRUE,
                                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    g_object_class_install_property (gobject_class,
                                     PROP_USE_SESSION,
                                     g_param_spec_int ("use-session",
                                             "use-session",
                                             "use-session",
                                             -1, 1, -1,
                                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    g_object_class_install_property (gobject_class,
                                     PROP_INSTANCE_NAME,
                                     g_param_spec_string ("instance-name",
                                             "instance-name",
                                             "instance-name",
                                             NULL,
                                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    g_object_class_install_property (gobject_class, PROP_DEFAULT_UI,
        g_param_spec_pointer ("default-ui", "default-ui", "default-ui",
                              (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));

    signals[INIT] =
            g_signal_new ("init",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, init),
                          NULL, NULL,
                          _moo_marshal_BOOLEAN__VOID,
                          G_TYPE_BOOLEAN, 0);

    signals[RUN] =
            g_signal_new ("run",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, run),
                          NULL, NULL,
                          _moo_marshal_INT__VOID,
                          G_TYPE_INT, 0);

    signals[QUIT] =
            g_signal_new ("quit",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, quit),
                          NULL, NULL,
                          _moo_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

    signals[TRY_QUIT] =
            g_signal_new ("try-quit",
                          G_OBJECT_CLASS_TYPE (klass),
                          (GSignalFlags) (G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST),
                          G_STRUCT_OFFSET (MooAppClass, try_quit),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOLEAN__VOID,
                          G_TYPE_BOOLEAN, 0);

    signals[PREFS_DIALOG] =
            g_signal_new ("prefs-dialog",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, quit),
                          NULL, NULL,
                          _moo_marshal_OBJECT__VOID,
                          MOO_TYPE_PREFS_DIALOG, 0);

    signals[LOAD_SESSION] =
            g_signal_new ("load-session",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, load_session),
                          NULL, NULL,
                          _moo_marshal_VOID__POINTER,
                          G_TYPE_NONE, 1,
                          G_TYPE_POINTER);

    signals[SAVE_SESSION] =
            g_signal_new ("save-session",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, save_session),
                          NULL, NULL,
                          _moo_marshal_VOID__POINTER,
                          G_TYPE_NONE, 1,
                          G_TYPE_POINTER);
}


static void
moo_app_instance_init (MooApp *app)
{
    g_return_if_fail (moo_app_data.instance == NULL);

    _moo_stock_init ();

    moo_app_data.instance = app;

    app->priv = g_new0 (MooAppPrivate, 1);
    app->priv->use_session = -1;
}


#if defined(HAVE_SIGNAL)
static void
setup_signals (void(*handler)(int))
{
    signal (SIGINT, handler);
#ifdef SIGHUP
    /* TODO: maybe detach from terminal in this case? */
    signal (SIGHUP, handler);
#endif
}

static void
sigint_handler (int sig)
{
    signal_received = sig;
    setup_signals (SIG_DFL);
}
#endif

static GObject*
moo_app_constructor (GType           type,
                     guint           n_params,
                     GObjectConstructParam *params)
{
    GObject *object;
    MooApp *app;

    if (moo_app_data.instance != NULL)
    {
        g_critical ("attempt to create second instance of application class");
        g_critical ("going to crash now");
        return NULL;
    }

    object = moo_app_parent_class->constructor (type, n_params, params);
    app = MOO_APP (object);

#if defined(HAVE_SIGNAL) && defined(SIGINT)
    setup_signals (sigint_handler);
#endif
    moo_app_install_cleanup ();

    install_common_actions ();
    install_editor_actions ();

    return object;
}


static void
moo_app_finalize (GObject *object)
{
    MooApp *app = MOO_APP(object);

    moo_app_quit_real (app);

    moo_app_data.instance = NULL;

    g_free (app->priv->rc_files[0]);
    g_free (app->priv->rc_files[1]);

    g_free (app->priv->session_file);
    if (app->priv->session)
        moo_markup_doc_unref (app->priv->session);

    if (app->priv->editor)
        g_object_unref (app->priv->editor);
    if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);

    g_free (app->priv->instance_name);
    g_free (app->priv);

    G_OBJECT_CLASS (moo_app_parent_class)->finalize (object);
}


static void
moo_app_set_property (GObject        *object,
                      guint           prop_id,
                      const GValue   *value,
                      GParamSpec     *pspec)
{
    MooApp *app = MOO_APP (object);

    switch (prop_id)
    {
        case PROP_RUN_INPUT:
            app->priv->run_input = g_value_get_boolean (value);
            break;

        case PROP_USE_SESSION:
            app->priv->use_session = g_value_get_int (value);
            break;

        case PROP_INSTANCE_NAME:
            g_free (app->priv->instance_name);
            app->priv->instance_name = g_value_dup_string (value);
            break;

        case PROP_DEFAULT_UI:
            app->priv->default_ui = g_value_get_pointer (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
moo_app_get_property (GObject        *object,
                      guint           prop_id,
                      GValue         *value,
                      GParamSpec     *pspec)
{
    MooApp *app = MOO_APP (object);

    switch (prop_id)
    {
        case PROP_RUN_INPUT:
            g_value_set_boolean (value, app->priv->run_input);
            break;
        case PROP_USE_SESSION:
            g_value_set_int (value, app->priv->use_session);
            break;
        case PROP_INSTANCE_NAME:
            g_value_set_string (value, app->priv->instance_name);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


MooApp*
moo_app_get_instance (void)
{
    return moo_app_data.instance;
}


#define SCRIPT_PREFIX_LUA "lua-script:"
#define SCRIPT_PREFIX_LUA_FILE "lua-file:"

void
moo_app_run_script (MooApp     *app,
                    const char *script)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (script != NULL);

    if (g_str_has_prefix (script, SCRIPT_PREFIX_LUA))
        medit_lua_run_string (script + strlen (SCRIPT_PREFIX_LUA));
    else if (g_str_has_prefix (script, SCRIPT_PREFIX_LUA_FILE))
        medit_lua_run_file (script + strlen (SCRIPT_PREFIX_LUA_FILE));
    else
        medit_lua_run_string (script);
}

// static void
// run_python_file (MooApp     *app,
//                  const char *filename)
// {
//     FILE *file;
//     MooPyObject *res;
//
//     g_return_if_fail (MOO_IS_APP (app));
//     g_return_if_fail (filename != NULL);
//     g_return_if_fail (moo_python_running ());
//
//     file = _moo_fopen (filename, "rb");
//     g_return_if_fail (file != NULL);
//
//     res = moo_python_run_file (file, filename, NULL, NULL);
//
//     fclose (file);
//
//     if (res)
//         moo_Py_DECREF (res);
//     else
//         moo_PyErr_Print ();
// }
//
// static void
// run_python_script (const char *string)
// {
//     MooPyObject *res;
//
//     g_return_if_fail (string != NULL);
//     g_return_if_fail (moo_python_running ());
//
//     res = moo_python_run_simple_string (string);
//
//     if (res)
//         moo_Py_DECREF (res);
//     else
//         moo_PyErr_Print ();
// }


MooEditor *
moo_app_get_editor (MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    return app->priv->editor;
}


#ifdef MOO_BUILD_EDIT
static gboolean
close_editor_window (MooApp *app)
{
    GSList *windows;
    gboolean ret = FALSE;

    if (!app->priv->running || app->priv->in_try_quit)
        return FALSE;

    windows = moo_editor_list_windows (app->priv->editor);

    if (windows && !windows->next)
    {
        moo_app_quit (app);
        ret = TRUE;
    }

    g_slist_free (windows);
    return ret;
}

static void
init_plugins (MooApp *app)
{
    if (MOO_APP_GET_CLASS (app)->init_plugins)
        MOO_APP_GET_CLASS (app)->init_plugins (app);
}

static void
moo_app_init_editor (MooApp *app)
{
    app->priv->editor = moo_editor_create_instance (FALSE);

    g_signal_connect_swapped (app->priv->editor, "close-window",
                              G_CALLBACK (close_editor_window), app);

    /* if ui_xml wasn't set yet, then moo_app_get_ui_xml()
       will get editor's xml */
    moo_editor_set_ui_xml (app->priv->editor,
                           moo_app_get_ui_xml (app));

    init_plugins (app);
}
#endif /* MOO_BUILD_EDIT */


static void
moo_app_init_ui (MooApp *app)
{
    MooUiXml *xml = NULL;
    char **files, **p;

    files = moo_get_data_files (MOO_UI_XML_FILE);

    for (p = files; p && *p; ++p)
    {
        GError *error = NULL;
        GMappedFile *file;

        file = g_mapped_file_new (*p, FALSE, &error);

        if (file)
        {
            xml = moo_ui_xml_new ();
            moo_ui_xml_add_ui_from_string (xml,
                                           g_mapped_file_get_contents (file),
                                           g_mapped_file_get_length (file));
            g_mapped_file_free (file);
            break;
        }

        if (error->domain != G_FILE_ERROR || error->code != G_FILE_ERROR_NOENT)
        {
            g_warning ("%s: could not open file '%s'", G_STRLOC, *p);
            g_warning ("%s: %s", G_STRLOC, error->message);
        }

        g_error_free (error);
    }

    if (!xml && app->priv->default_ui)
    {
        xml = moo_ui_xml_new ();
        moo_ui_xml_add_ui_from_string (xml, app->priv->default_ui, -1);
    }

    if (xml)
    {
        if (app->priv->ui_xml)
            g_object_unref (app->priv->ui_xml);
        app->priv->ui_xml = xml;
    }

    g_strfreev (files);
}


#ifdef MOO_USE_QUARTZ

static void
dock_open_documents (MooApp  *app,
                     char   **files)
{
    moo_app_open_files (app, files, 0, 0, 0);
}

static void
dock_quit_activate (MooApp *app)
{
    moo_app_quit (app);
}

static void
moo_app_init_mac (MooApp *app)
{
    app->priv->dock = ige_mac_dock_get_default ();
    g_signal_connect_swapped (app->priv->dock, "open-documents",
                              G_CALLBACK (dock_open_documents), app);
    g_signal_connect_swapped (app->priv->dock, "quit-activate",
                              G_CALLBACK (dock_quit_activate), app);
}

#else /* !MOO_USE_QUARTZ */
static void
moo_app_init_mac (G_GNUC_UNUSED MooApp *app)
{
}
#endif /* !MOO_USE_QUARTZ */

static gboolean
moo_app_init_real (MooApp *app)
{
    gdk_set_program_class (MOO_APP_FULL_NAME);
    gtk_window_set_default_icon_name (MOO_APP_SHORT_NAME);

    moo_set_display_app_name (MOO_APP_SHORT_NAME);
    _moo_set_app_instance_name (app->priv->instance_name);

    moo_app_load_prefs (app);
    moo_app_init_ui (app);
    moo_app_init_mac (app);

#ifdef MOO_BUILD_EDIT
    moo_app_init_editor (app);

    if (app->priv->use_session == -1)
        app->priv->use_session = moo_prefs_get_bool (moo_edit_setting (MOO_EDIT_PREFS_SAVE_SESSION));
#endif

    if (app->priv->use_session)
        app->priv->run_input = TRUE;
    start_input (app);

    return TRUE;
}


static void
input_callback (char        cmd,
                const char *data,
                guint       len,
                gpointer    cb_data)
{
    MooApp *app = cb_data;

    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (data != NULL);

    moo_app_exec_cmd (app, cmd, data, len);
}

static void
start_input (MooApp *app)
{
    if (app->priv->run_input)
        _moo_app_input_start (app->priv->instance_name,
                              TRUE, input_callback, app);
}

gboolean
moo_app_send_msg (const char *pid,
                  const char *data,
                  int         len)
{
    g_return_val_if_fail (data != NULL, FALSE);
    return _moo_app_input_send_msg (pid, data, len);
}


static gboolean
on_gtk_main_quit (MooApp *app)
{
    app->priv->quit_handler_id = 0;

    if (!moo_app_quit (app))
        MOO_APP_GET_CLASS(app)->quit (app);

    return FALSE;
}


static gboolean
check_signal (void)
{
    if (signal_received)
    {
        g_print ("%s\n", g_strsignal (signal_received));
        if (moo_app_data.instance)
            MOO_APP_GET_CLASS(moo_app_data.instance)->quit (moo_app_data.instance);
        exit (EXIT_FAILURE);
    }

    return TRUE;
}


static gboolean
moo_app_try_quit (MooApp *app)
{
    gboolean stopped = FALSE;

    g_return_val_if_fail (MOO_IS_APP (app), FALSE);

    if (!app->priv->running)
        return TRUE;

    app->priv->in_try_quit = TRUE;
    g_signal_emit (app, signals[TRY_QUIT], 0, &stopped);
    app->priv->in_try_quit = FALSE;

    return !stopped;
}


static void
sm_quit_requested (MooApp *app)
{
    EggSMClient *sm_client;

    sm_client = app->priv->sm_client;
    g_return_if_fail (sm_client != NULL);

    g_object_ref (sm_client);
    egg_sm_client_will_quit (sm_client, moo_app_quit (app));
    g_object_unref (sm_client);
}

static void
sm_quit (MooApp *app)
{
    if (!moo_app_quit (app))
        MOO_APP_GET_CLASS(app)->quit (app);
}

static int
moo_app_run_real (MooApp *app)
{
    g_return_val_if_fail (!app->priv->running, 0);
    app->priv->running = TRUE;

    app->priv->quit_handler_id =
            gtk_quit_add (1, (GtkFunction) on_gtk_main_quit, app);

    _moo_timeout_add (100, (GSourceFunc) check_signal, NULL);

    app->priv->sm_client = egg_sm_client_get ();
    /* make it install log handler */
    g_option_group_free (egg_sm_client_get_option_group ());
    g_signal_connect_swapped (app->priv->sm_client, "quit-requested",
                              G_CALLBACK (sm_quit_requested), app);
    g_signal_connect_swapped (app->priv->sm_client, "quit",
                              G_CALLBACK (sm_quit), app);
    if (EGG_SM_CLIENT_GET_CLASS (app->priv->sm_client)->startup)
        EGG_SM_CLIENT_GET_CLASS (app->priv->sm_client)->startup (app->priv->sm_client, NULL);

    gtk_main ();

    return 0;
}


static gboolean
moo_app_try_quit_real (MooApp *app)
{
    if (!app->priv->running)
        return FALSE;

    moo_app_save_session (app);

#ifdef MOO_BUILD_EDIT
    if (!moo_editor_close_all (app->priv->editor, TRUE, TRUE))
        return TRUE;
#endif /* MOO_BUILD_EDIT */

    return FALSE;
}


static void
moo_app_install_cleanup (void)
{
    if (!moo_app_data.atexit_installed)
    {
        moo_app_data.atexit_installed = TRUE;
        atexit (moo_app_cleanup);
    }
}

static void
moo_app_cleanup (void)
{
    _moo_app_input_shutdown ();
    moo_mime_shutdown ();
    moo_cleanup ();
}


static void
moo_app_quit_real (MooApp *app)
{
    guint i;

    if (!app->priv->running)
        return;
    else
        app->priv->running = FALSE;

    g_object_unref (app->priv->sm_client);
    app->priv->sm_client = NULL;

#ifdef MOO_BUILD_EDIT
    moo_editor_close_all (app->priv->editor, FALSE, FALSE);

    moo_plugin_shutdown ();

    g_object_unref (app->priv->editor);
    app->priv->editor = NULL;
#endif /* MOO_BUILD_EDIT */

    moo_app_write_session (app);
    moo_app_save_prefs (app);

    if (app->priv->quit_handler_id)
        gtk_quit_remove (app->priv->quit_handler_id);

    i = 0;
    while (gtk_main_level () && i < 1000)
    {
        gtk_main_quit ();
        i++;
    }

    moo_app_cleanup ();
}


gboolean
moo_app_init (MooApp *app)
{
    gboolean retval;
    g_return_val_if_fail (MOO_IS_APP (app), FALSE);
    g_signal_emit (app, signals[INIT], 0, &retval);
    return retval;
}


int
moo_app_run (MooApp *app)
{
    int retval;
    g_return_val_if_fail (MOO_IS_APP (app), -1);
    g_signal_emit (app, signals[RUN], 0, &retval);
    return retval;
}


gboolean
moo_app_quit (MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), FALSE);

    if (app->priv->in_try_quit || !app->priv->running)
        return TRUE;

    if (moo_app_try_quit (app))
    {
        MOO_APP_GET_CLASS(app)->quit (app);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


static void
install_common_actions (void)
{
    MooWindowClass *klass = g_type_class_ref (MOO_TYPE_WINDOW);

    g_return_if_fail (klass != NULL);

    moo_window_class_new_action (klass, "Preferences", NULL,
                                 "display-name", GTK_STOCK_PREFERENCES,
                                 "label", GTK_STOCK_PREFERENCES,
                                 "tooltip", GTK_STOCK_PREFERENCES,
                                 "stock-id", GTK_STOCK_PREFERENCES,
                                 "closure-callback", moo_app_prefs_dialog,
                                 NULL);

    moo_window_class_new_action (klass, "About", NULL,
                                 "label", GTK_STOCK_ABOUT,
                                 "no-accel", TRUE,
                                 "stock-id", GTK_STOCK_ABOUT,
                                 "closure-callback", moo_app_about_dialog,
                                 NULL);

    moo_window_class_new_action (klass, "Help", NULL,
                                 "label", GTK_STOCK_HELP,
                                 "default-accel", MOO_APP_ACCEL_HELP,
                                 "stock-id", GTK_STOCK_HELP,
                                 "closure-callback", moo_app_help,
                                 NULL);

    moo_window_class_new_action (klass, "ReportBug", NULL,
                                 "label", _("Report a Bug..."),
                                 "closure-callback", moo_app_report_bug,
                                 NULL);

    moo_window_class_new_action (klass, "Quit", NULL,
                                 "display-name", GTK_STOCK_QUIT,
                                 "label", GTK_STOCK_QUIT,
                                 "tooltip", GTK_STOCK_QUIT,
                                 "stock-id", GTK_STOCK_QUIT,
                                 "default-accel", MOO_APP_ACCEL_QUIT,
                                 "closure-callback", moo_app_quit,
                                 "closure-proxy-func", moo_app_get_instance,
                                 NULL);

    g_type_class_unref (klass);
}


static void
install_editor_actions (void)
{
#ifdef MOO_BUILD_EDIT
    MooWindowClass *klass = g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    g_return_if_fail (klass != NULL);
    g_type_class_unref (klass);
#endif /* MOO_BUILD_EDIT */
}


MooUiXml *
moo_app_get_ui_xml (MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (!app->priv->ui_xml)
    {
#ifdef MOO_BUILD_EDIT
        if (app->priv->editor)
        {
            app->priv->ui_xml = moo_editor_get_ui_xml (app->priv->editor);
            g_object_ref (app->priv->ui_xml);
        }
#endif
        if (!app->priv->ui_xml)
            app->priv->ui_xml = moo_ui_xml_new ();
    }

    return app->priv->ui_xml;
}


void
moo_app_set_ui_xml (MooApp     *app,
                    MooUiXml   *xml)
{
    g_return_if_fail (MOO_IS_APP (app));

    if (app->priv->ui_xml == xml)
        return;

    if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);

    app->priv->ui_xml = xml;

    if (xml)
        g_object_ref (app->priv->ui_xml);

#ifdef MOO_BUILD_EDIT
    if (app->priv->editor)
        moo_editor_set_ui_xml (app->priv->editor, xml);
#endif /* MOO_BUILD_EDIT */
}



static void
moo_app_load_session_real (MooApp        *app,
                           MooMarkupNode *xml)
{
#ifdef MOO_BUILD_EDIT
    MooEditor *editor;
    editor = moo_app_get_editor (app);
    g_return_if_fail (editor != NULL);
    _moo_editor_load_session (editor, xml);
#endif /* MOO_BUILD_EDIT */
}

static void
moo_app_save_session_real (MooApp        *app,
                           MooMarkupNode *xml)
{
#ifdef MOO_BUILD_EDIT
    MooEditor *editor;
    editor = moo_app_get_editor (app);
    g_return_if_fail (editor != NULL);
    _moo_editor_save_session (editor, xml);
#endif /* MOO_BUILD_EDIT */
}

static void
moo_app_save_session (MooApp *app)
{
    MooMarkupNode *root;

    if (!app->priv->session_file)
        return;

    if (app->priv->session)
        moo_markup_doc_unref (app->priv->session);

    app->priv->session = moo_markup_doc_new ("session");
    root = moo_markup_create_root_element (app->priv->session, "session");
    moo_markup_set_prop (root, "version", SESSION_VERSION);

    g_signal_emit (app, signals[SAVE_SESSION], 0, root);
}

static void
moo_app_write_session (MooApp *app)
{
    char *filename;
    GError *error = NULL;
    MooFileWriter *writer;

    if (!app->priv->session_file)
        return;

    filename = moo_get_user_cache_file (app->priv->session_file);

    if (!app->priv->session)
    {
        _moo_unlink (filename);
        g_free (filename);
        return;
    }

    if ((writer = moo_config_writer_new (filename, FALSE, &error)))
    {
        moo_markup_write_pretty (app->priv->session, writer, 1);
        moo_file_writer_close (writer, &error);
    }

    if (error)
    {
        g_critical ("could not save session file %s: %s", filename, error->message);
        g_error_free (error);
    }

    g_free (filename);
}

void
moo_app_load_session (MooApp *app)
{
    MooMarkupDoc *doc;
    MooMarkupNode *root;
    GError *error = NULL;
    const char *version;
    char *session_file;

    g_return_if_fail (MOO_IS_APP (app));

    if (!app->priv->use_session)
        return;

    if (!app->priv->session_file)
    {
        if (app->priv->instance_name)
            app->priv->session_file = g_strdup_printf (MOO_SESSION_FILE_NAME ".%s",
                                                       app->priv->instance_name);
        else
            app->priv->session_file = g_strdup (MOO_SESSION_FILE_NAME);
    }

    session_file = moo_get_user_cache_file (app->priv->session_file);

    if (!g_file_test (session_file, G_FILE_TEST_EXISTS) ||
        !(doc = moo_markup_parse_file (session_file, &error)))
    {
        if (error)
        {
            g_warning ("could not open session file %s: %s",
                       session_file, error->message);
            g_error_free (error);
        }

        g_free (session_file);
        return;
    }

    if (!(root = moo_markup_get_root_element (doc, "session")) ||
        !(version = moo_markup_get_prop (root, "version")))
        g_warning ("malformed session file %s, ignoring", session_file);
    else if (strcmp (version, SESSION_VERSION) != 0)
        g_warning ("invalid session file version %s in %s, ignoring",
                   version, session_file);
    else
        g_signal_emit (app, signals[LOAD_SESSION], 0, root);

    moo_markup_doc_unref (doc);
    g_free (session_file);
}


// static void
// moo_app_present (MooApp *app)
// {
//     gpointer window = NULL;
//
// #ifdef MOO_BUILD_EDIT
//     if (!window && app->priv->editor)
//         window = moo_editor_get_active_window (app->priv->editor);
// #endif /* MOO_BUILD_EDIT */
//
//     if (window)
//         moo_window_present (window, 0);
// }


// static void
// moo_app_open_uris (MooApp     *app,
//                    const char *data,
//                    gboolean    has_encoding)
// {
// #ifdef MOO_BUILD_EDIT
//     char **uris;
//     guint32 stamp;
//     char *stamp_string;
//     char *line_string;
//     guint32 line;
//     char *encoding = NULL;
//
//     g_return_if_fail (strlen (data) > (has_encoding ? 32 : 16));
//
//     stamp_string = g_strndup (data, 8);
//     stamp = strtoul (stamp_string, NULL, 16);
//     line_string = g_strndup (data + 8, 8);
//     line = strtoul (line_string, NULL, 16);
//
//     if (line > G_MAXINT)
//         line = 0;
//
//     data += 16;
//
//     if (has_encoding)
//     {
//         char *p;
//         encoding = g_strndup (data, 16);
//         p = strchr (encoding, ' ');
//         if (p)
//             *p = 0;
//         data += 16;
//     }
//
//     uris = g_strsplit (data, "\r\n", 0);
//
//     if (uris && *uris)
//     {
//         char **p;
//
//         for (p = uris; p && *p && **p; ++p)
//         {
//             guint line_here = 0;
//             guint options = 0;
//             char *filename;
//
//             filename = _moo_edit_uri_to_filename (*p, &line_here, &options);
//
//             if (p != uris)
//                 line = 0;
//             if (line_here)
//                 line = line_here;
//
//             if (filename)
//                 moo_app_new_file (app, filename, encoding, line, options);
//
//             g_free (filename);
//         }
//     }
//     else
//     {
//         moo_app_new_file (app, NULL, encoding, 0, 0);
//     }
//
//     moo_editor_present (app->priv->editor, stamp);
//
//     g_free (encoding);
//     g_strfreev (uris);
//     g_free (stamp_string);
// #endif /* MOO_BUILD_EDIT */
// }

void
moo_app_open_files (MooApp         *app,
                    MooAppFileInfo *files,
                    int             n_files,
                    guint32         stamp)
{
    int i;

    g_return_if_fail (MOO_IS_APP (app));

    if (!n_files)
    {
        MooEdit *doc;

        doc = moo_editor_get_active_doc (app->priv->editor);

        if (!doc || !moo_edit_is_empty (doc))
            moo_editor_new_doc (app->priv->editor, NULL);

        return;
    }

    for (i = 0; i < n_files; ++i)
        _moo_editor_open_uri (app->priv->editor,
                              files[i].uri,
                              files[i].encoding,
                              files[i].line,
                              files[i].options);

    moo_editor_present (app->priv->editor, stamp);
}


static MooAppCmdCode
get_cmd_code (char cmd)
{
    guint i;

    for (i = 1; i < CMD_LAST; ++i)
        if (cmd == moo_app_cmd_chars[i])
            return i;

    g_return_val_if_reached (0);
}

static void
moo_app_exec_cmd (MooApp             *app,
                  char                cmd,
                  const char         *data,
                  G_GNUC_UNUSED guint len)
{
    MooAppCmdCode code;

    g_return_if_fail (MOO_IS_APP (app));

    code = get_cmd_code (cmd);

    switch (code)
    {
        case CMD_SCRIPT:
            moo_app_run_script (app, data);
            break;

        case CMD_OPEN_FILES:
            moo_app_cmd_open_files (app, data);
            break;

        default:
            g_warning ("%s: got unknown command %c %d", G_STRLOC, cmd, code);
    }
}


static void
moo_app_help (GtkWidget *window)
{
    GtkWidget *focus = gtk_window_get_focus (GTK_WINDOW (window));
    moo_help_open_any (focus ? focus : window);
}


static void
moo_app_report_bug (G_GNUC_UNUSED GtkWidget *window)
{
    char *url;
    char *os;
    char *version_escaped, *os_escaped;

    version_escaped = g_uri_escape_string (MOO_DISPLAY_VERSION, NULL, FALSE);
    os = get_system_name ();
    os_escaped = os ? g_uri_escape_string (os, NULL, FALSE) : g_strdup ("");

    url = g_strdup_printf ("http://mooedit.sourceforge.net/cgi-bin/report_bug.cgi?version=%s&os=%s",
                           version_escaped, os_escaped);

    moo_open_url (url);

    g_free (url);
    g_free (os_escaped);
    g_free (os);
    g_free (version_escaped);
}


void
moo_app_prefs_dialog (GtkWidget *parent)
{
    MooApp *app;
    GtkWidget *dialog;

    app = moo_app_get_instance ();
    dialog = MOO_APP_GET_CLASS(app)->prefs_dialog (app);
    g_return_if_fail (MOO_IS_PREFS_DIALOG (dialog));

    moo_prefs_dialog_run (MOO_PREFS_DIALOG (dialog), parent);
}


static void
prefs_dialog_apply (void)
{
    moo_app_save_prefs (moo_app_get_instance ());
}


static GtkWidget *
moo_app_create_prefs_dialog (MooApp *app)
{
    char *title;
    MooPrefsDialog *dialog;

    /* Prefs dialog title, like "medit Preferences" */
    title = g_strdup_printf (_("%s Preferences"), MOO_APP_FULL_NAME);
    dialog = MOO_PREFS_DIALOG (moo_prefs_dialog_new (title));
    g_free (title);

#ifdef MOO_BUILD_EDIT
    moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_1 (moo_app_get_editor (app)));
    moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_2 (moo_app_get_editor (app)));
    moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_3 (moo_app_get_editor (app)));
    moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_4 (moo_app_get_editor (app)));
    moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_5 (moo_app_get_editor (app)));
    moo_plugin_attach_prefs (GTK_WIDGET (dialog));
#endif

    g_signal_connect_after (dialog, "apply",
                            G_CALLBACK (prefs_dialog_apply),
                            NULL);

    return GTK_WIDGET (dialog);
}


static void
moo_app_load_prefs (MooApp *app)
{
    GError *error = NULL;
    char **sys_files;

    app->priv->rc_files[MOO_PREFS_RC] = moo_get_user_data_file (MOO_INI_FILE_NAME);
    app->priv->rc_files[MOO_PREFS_STATE] = moo_get_user_cache_file (MOO_STATE_FILE_NAME);

    sys_files = moo_get_sys_data_files (MOO_INI_FILE_NAME);

    if (!moo_prefs_load (sys_files,
                         app->priv->rc_files[MOO_PREFS_RC],
                         app->priv->rc_files[MOO_PREFS_STATE],
                         &error))
    {
        g_warning ("%s: could not read config files", G_STRLOC);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }

    g_strfreev (sys_files);
}


static void
moo_app_save_prefs (MooApp *app)
{
    GError *error = NULL;

    if (!moo_prefs_save (app->priv->rc_files[MOO_PREFS_RC],
                         app->priv->rc_files[MOO_PREFS_STATE],
                         &error))
    {
        g_warning ("%s: could not save config files", G_STRLOC);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }
}


#define MOO_APP_CMD_VERSION "1.0"

static gboolean
moo_app_parse_files (const char      *data,
                     guint32         *stamp,
                     MooAppFileInfo **filesp,
                     int             *n_files)
{
    GArray *files = NULL;
    MooMarkupDoc *xml;
    MooMarkupNode *root;
    MooMarkupNode *node;
    const char *version;

    xml = moo_markup_parse_memory (data, -1, NULL);
    g_return_val_if_fail (xml != NULL, FALSE);

    if (!(root = moo_markup_get_root_element (xml, "moo-app-open-files")) ||
        !(version = moo_markup_get_prop (root, "version")) ||
        strcmp (version, MOO_APP_CMD_VERSION) != 0)
    {
        g_warning ("%s: invalid markup", G_STRFUNC);
        moo_markup_doc_unref (xml);
        return FALSE;
    }

    *stamp = moo_markup_uint_prop (root, "stamp", 0);

    for (node = root->children; node != NULL; node = node->next)
    {
        const char *uri;
        const char *encoding;
        MooAppFileInfo file = {0};

        if (!MOO_MARKUP_IS_ELEMENT (node))
            continue;

        if (strcmp (node->name, "file") != 0 ||
            !(uri = moo_markup_get_content (node)) ||
            !uri[0])
        {
            g_critical ("%s: oops", G_STRFUNC);
            continue;
        }

        file.uri = g_strdup (uri);
        encoding = moo_markup_get_prop (node, "encoding");
        if (encoding && encoding[0])
            file.encoding = g_strdup (encoding);

        file.line = moo_markup_int_prop (node, "line", 0);
        if (moo_markup_bool_prop (node, "new-window", FALSE))
            file.options |= MOO_EDIT_OPEN_NEW_WINDOW;
        if (moo_markup_bool_prop (node, "new-tab", FALSE))
            file.options |= MOO_EDIT_OPEN_NEW_TAB;
        if (moo_markup_bool_prop (node, "reload", FALSE))
            file.options |= MOO_EDIT_OPEN_RELOAD;

        if (!files)
            files = g_array_new (FALSE, FALSE, sizeof (MooAppFileInfo));
        g_array_append_val (files, file);
    }

    if (files)
    {
        *n_files = files->len;
        *filesp = (MooAppFileInfo *) g_array_free (files, FALSE);
    }

    moo_markup_doc_unref (xml);
    return TRUE;
}

static void
moo_app_cmd_open_files (MooApp     *app,
                        const char *data)
{
    MooAppFileInfo *files = NULL;
    int n_files = 0;
    int i;
    guint32 stamp;

    if (moo_app_parse_files (data, &stamp, &files, &n_files))
        moo_app_open_files (app, files, n_files, stamp);

    for (i = 0; i < n_files; ++i)
    {
        g_free (files[i].uri);
        g_free (files[i].encoding);
    }

    g_free (files);
}

G_GNUC_PRINTF(2, 3) static void
append_escaped (GString *str, const char *format, ...)
{
    va_list args;
    char *escaped;

    va_start (args, format);

    escaped = g_markup_vprintf_escaped (format, args);
    g_string_append (str, escaped);
    g_free (escaped);

    va_end (args);
}

gboolean
moo_app_send_files (MooAppFileInfo *files,
                    int             n_files,
                    guint32         stamp,
                    const char     *pid)
{
    gboolean result;
    GString *msg;
    int i;

#if 0
    _moo_message ("moo_app_send_files: got %d files to pid %s",
                  n_files, pid ? pid : "NONE");
#endif

    msg = g_string_new (NULL);
    g_string_append_printf (msg, "%s<moo-app-open-files version=\"%s\" stamp=\"%u\">",
                            CMD_OPEN_FILES_S, MOO_APP_CMD_VERSION, stamp);

    for (i = 0; i < n_files; ++i)
    {
        g_string_append (msg, "<file");
        if (files[i].encoding)
            g_string_append_printf (msg, " encoding=\"%s\"", files[i].encoding);
        if (files[i].line)
            g_string_append_printf (msg, " line=\"%u\"", (guint) files[i].line);
        if (files[i].options & MOO_EDIT_OPEN_NEW_WINDOW)
            g_string_append_printf (msg, " new-window=\"true\"");
        if (files[i].options & MOO_EDIT_OPEN_NEW_TAB)
            g_string_append_printf (msg, " new-tab=\"true\"");
        if (files[i].options & MOO_EDIT_OPEN_RELOAD)
            g_string_append_printf (msg, " reload=\"true\"");
        append_escaped (msg, ">%s</file>", files[i].uri);
    }

    g_string_append (msg, "</moo-app-open-files>");

    result = moo_app_send_msg (pid, msg->str, msg->len);

    g_string_free (msg, TRUE);
    return result;
}
