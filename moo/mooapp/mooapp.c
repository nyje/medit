/*
 *   mooapp/mooapp.c
 *
 *   Copyright (C) 2004-2005 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef MOO_USE_PYTHON
#include <Python.h>
#include "mooapp/moopythonconsole.h"
#include "mooapp/mooapp-python.h"
#include "mooapp/moopython.h"
#endif

#define WANT_MOO_APP_CMD_CHARS
#include "mooapp/mooapp-private.h"
#include "mooapp/mooappinput.h"
#include "mooapp/mooappoutput.h"
#include "mooedit/mooeditprefs.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooplugin.h"
#include "mooedit/plugins/mooeditplugins.h"
#include "mooutils/moomarshals.h"
#include "mooutils/moocompat.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moostock.h"
#include "mooutils/mooutils-misc.h"


#ifdef VERSION
#define APP_VERSION VERSION
#else
#define APP_VERSION "<uknown version>"
#endif


/* moo-pygtk.c */
void initmoo (void);


static MooApp *moo_app_instance = NULL;
static MooAppInput *moo_app_input = NULL;
static MooAppOutput *moo_app_output = NULL;
#ifdef MOO_USE_PYTHON
static MooPython *moo_app_python = NULL;
#endif


struct _MooAppPrivate {
    char      **argv;
    int         exit_code;
    MooEditor  *editor;
    MooAppInfo *info;
    gboolean    run_python;
    gboolean    run_input;
    gboolean    run_output;
    MooAppWindowPolicy window_policy;

    gboolean    running;
    gboolean    in_try_quit;

    GType       term_window_type;
    GSList     *terminals;
    MooTermWindow *term_window;

    MooUIXML   *ui_xml;
    guint       quit_handler_id;
    gboolean    use_editor;
    gboolean    use_terminal;
};


static void     moo_app_class_init      (MooAppClass        *klass);
static void     moo_app_instance_init   (MooApp             *app);
static GObject *moo_app_constructor     (GType               type,
                                         guint               n_params,
                                         GObjectConstructParam *params);
static void     moo_app_finalize        (GObject            *object);

static MooAppInfo *moo_app_info_new     (void);
static MooAppInfo *moo_app_info_copy    (const MooAppInfo   *info);
static void     moo_app_info_free       (MooAppInfo         *info);

static void     install_actions         (MooApp             *app,
                                         GType               type);
static void     install_editor_actions  (MooApp             *app);
static void     install_terminal_actions(MooApp             *app);

static void     moo_app_set_property    (GObject            *object,
                                         guint               prop_id,
                                         const GValue       *value,
                                         GParamSpec         *pspec);
static void     moo_app_get_property    (GObject            *object,
                                         guint               prop_id,
                                         GValue             *value,
                                         GParamSpec         *pspec);

static void     moo_app_set_argv        (MooApp             *app,
                                         char              **argv);

static gboolean moo_app_init_real       (MooApp             *app);
static int      moo_app_run_real        (MooApp             *app);
static void     moo_app_quit_real       (MooApp             *app);
static gboolean moo_app_try_quit_real   (MooApp             *app);
static void     moo_app_exec_cmd_real   (MooApp             *app,
                                         char                cmd,
                                         const char         *data,
                                         guint               len);

static void     moo_app_set_name        (MooApp             *app,
                                         const char         *short_name,
                                         const char         *full_name);
static void     moo_app_set_description (MooApp             *app,
                                         const char         *description);

static void     all_editors_closed      (MooApp             *app);

static void     start_python            (MooApp             *app);
static void     start_io                (MooApp             *app);
#ifdef MOO_USE_PYTHON
static void     execute_selection       (MooEditWindow      *window);
#endif


static GObjectClass *moo_app_parent_class;

GType
moo_app_get_type (void)
{
    static GType type = 0;

    if (!type)
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
    PROP_ARGV,
    PROP_SHORT_NAME,
    PROP_FULL_NAME,
    PROP_DESCRIPTION,
    PROP_WINDOW_POLICY,
    PROP_RUN_PYTHON,
    PROP_RUN_INPUT,
    PROP_RUN_OUTPUT,
    PROP_USE_EDITOR,
    PROP_USE_TERMINAL
};

enum {
    INIT,
    RUN,
    QUIT,
    TRY_QUIT,
    PREFS_DIALOG,
    EXEC_CMD,
    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL];


static void
moo_app_class_init (MooAppClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    moo_create_stock_items ();

    moo_app_parent_class = g_type_class_peek_parent (klass);

    gobject_class->constructor = moo_app_constructor;
    gobject_class->finalize = moo_app_finalize;
    gobject_class->set_property = moo_app_set_property;
    gobject_class->get_property = moo_app_get_property;

    klass->init = moo_app_init_real;
    klass->run = moo_app_run_real;
    klass->quit = moo_app_quit_real;
    klass->try_quit = moo_app_try_quit_real;
    klass->prefs_dialog = _moo_app_create_prefs_dialog;
    klass->exec_cmd = moo_app_exec_cmd_real;

    g_object_class_install_property (gobject_class,
                                     PROP_ARGV,
                                     g_param_spec_pointer ("argv",
                                             "argv",
                                             "Null-terminated array of application arguments",
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_SHORT_NAME,
                                     g_param_spec_string ("short-name",
                                             "short-name",
                                             "short-name",
                                             "ggap",
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_FULL_NAME,
                                     g_param_spec_string ("full-name",
                                             "full-name",
                                             "full-name",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string ("description",
                                             "description",
                                             "description",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WINDOW_POLICY,
                                     g_param_spec_flags ("window-policy",
                                             "window-policy",
                                             "window-policy",
                                             MOO_TYPE_APP_WINDOW_POLICY,
                                             MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_RUN_PYTHON,
                                     g_param_spec_boolean ("run-python",
                                             "run-python",
                                             "run-python",
                                             TRUE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_RUN_INPUT,
                                     g_param_spec_boolean ("run-input",
                                             "run-input",
                                             "run-input",
                                             TRUE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_RUN_OUTPUT,
                                     g_param_spec_boolean ("run-output",
                                             "run-output",
                                             "run-output",
                                             FALSE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_USE_EDITOR,
                                     g_param_spec_boolean ("use-editor",
                                             "use-editor",
                                             "use-editor",
                                             TRUE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_USE_TERMINAL,
                                     g_param_spec_boolean ("use-terminal",
                                             "use-terminal",
                                             "use-terminal",
                                             TRUE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    signals[INIT] =
            g_signal_new ("init",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, init),
                          NULL, NULL,
                          _moo_marshal_BOOLEAN__VOID,
                          G_TYPE_STRING, 0);

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

    signals[EXEC_CMD] =
            g_signal_new ("exec-cmd",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooAppClass, exec_cmd),
                          NULL, NULL,
                          _moo_marshal_VOID__CHAR_STRING_UINT,
                          G_TYPE_NONE, 3,
                          G_TYPE_CHAR,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          G_TYPE_UINT);
}


static void
moo_app_instance_init (MooApp *app)
{
    g_return_if_fail (moo_app_instance == NULL);

    moo_app_instance = app;

    app->priv = g_new0 (MooAppPrivate, 1);
    app->priv->info = moo_app_info_new ();

    app->priv->info->version = g_strdup (APP_VERSION);
    app->priv->info->website = g_strdup ("http://ggap.sourceforge.net/");
    app->priv->info->website_label = g_strdup ("ggap.sourceforge.net");

#ifdef MOO_BUILD_TERM
    moo_app_set_terminal_type (app, MOO_TYPE_TERM_WINDOW);
#endif
}


static GObject*
moo_app_constructor (GType           type,
                     guint           n_params,
                     GObjectConstructParam *params)
{
    GObject *object;
    MooApp *app;

    if (moo_app_instance != NULL)
    {
        g_critical ("attempt to create second instance of application class");
        g_critical ("crash");
        return NULL;
    }

    object = moo_app_parent_class->constructor (type, n_params, params);
    app = MOO_APP (object);

    if (!app->priv->info->full_name)
        app->priv->info->full_name = g_strdup (app->priv->info->short_name);

    install_editor_actions (app);
    install_terminal_actions (app);

    return object;
}


static void
moo_app_finalize (GObject *object)
{
    MooApp *app = MOO_APP(object);

    moo_app_quit_real (app);

    moo_app_instance = NULL;

    g_free (app->priv->info->short_name);
    g_free (app->priv->info->full_name);
    g_free (app->priv->info->app_dir);
    g_free (app->priv->info->rc_file);
    g_free (app->priv->info);

    if (app->priv->argv)
        g_strfreev (app->priv->argv);
    if (app->priv->editor)
        g_object_unref (app->priv->editor);
    if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);

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
        case PROP_ARGV:
            moo_app_set_argv (app, (char**)g_value_get_pointer (value));
            break;

        case PROP_SHORT_NAME:
            moo_app_set_name (app, g_value_get_string (value), NULL);
            break;

        case PROP_FULL_NAME:
            moo_app_set_name (app, NULL, g_value_get_string (value));
            break;

        case PROP_DESCRIPTION:
            moo_app_set_description (app, g_value_get_string (value));
            break;

        case PROP_WINDOW_POLICY:
            app->priv->window_policy = g_value_get_flags (value);
            g_object_notify (object, "window-policy");
            break;

        case PROP_RUN_PYTHON:
            app->priv->run_python = g_value_get_boolean (value);
            break;

        case PROP_RUN_INPUT:
            app->priv->run_input = g_value_get_boolean (value);
            break;

        case PROP_RUN_OUTPUT:
            app->priv->run_output = g_value_get_boolean (value);
            break;

        case PROP_USE_EDITOR:
            app->priv->use_editor = g_value_get_boolean (value);
            break;

        case PROP_USE_TERMINAL:
            app->priv->use_terminal = g_value_get_boolean (value);
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
        case PROP_SHORT_NAME:
            g_value_set_string (value, app->priv->info->short_name);
            break;

        case PROP_FULL_NAME:
            g_value_set_string (value, app->priv->info->full_name);
            break;

        case PROP_DESCRIPTION:
            g_value_set_string (value, app->priv->info->description);
            break;

        case PROP_WINDOW_POLICY:
            g_value_set_flags (value, app->priv->window_policy);
            break;

        case PROP_RUN_PYTHON:
            g_value_set_boolean (value, app->priv->run_python);
            break;

        case PROP_RUN_INPUT:
            g_value_set_boolean (value, app->priv->run_input);
            break;

        case PROP_RUN_OUTPUT:
            g_value_set_boolean (value, app->priv->run_output);
            break;

        case PROP_USE_EDITOR:
            g_value_set_boolean (value, app->priv->use_editor);
            break;

        case PROP_USE_TERMINAL:
            g_value_set_boolean (value, app->priv->use_terminal);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


MooApp*
moo_app_get_instance (void)
{
    return moo_app_instance;
}


static void
moo_app_set_argv (MooApp         *app,
                  char          **argv)
{
    g_strfreev (app->priv->argv);
    app->priv->argv = g_strdupv (argv);
    g_object_notify (G_OBJECT (app), "argv");
}


int
moo_app_get_exit_code (MooApp      *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), -1);
    return app->priv->exit_code;
}

void
moo_app_set_exit_code (MooApp      *app,
                       int          code)
{
    g_return_if_fail (MOO_IS_APP (app));
    app->priv->exit_code = code;
}


const char*
moo_app_get_application_dir (MooApp      *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), ".");

    if (!app->priv->info->app_dir)
    {
        g_return_val_if_fail (app->priv->argv && app->priv->argv[0], ".");
        app->priv->info->app_dir = g_path_get_dirname (app->priv->argv[0]);
    }

    return app->priv->info->app_dir;
}


const char*
moo_app_get_input_pipe_name (G_GNUC_UNUSED MooApp *app)
{
    return moo_app_input ? moo_app_input->pipe_name : NULL;
}


const char*
moo_app_get_output_pipe_name (G_GNUC_UNUSED MooApp *app)
{
    return moo_app_output ? moo_app_output->pipe_name : NULL;
}


const char*
moo_app_get_rc_file_name (MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (!app->priv->info->rc_file)
    {
#ifdef __WIN32__
        char *basename = g_strdup_printf ("%s.ini",
                                          app->priv->info->short_name);
        app->priv->info->rc_file =
                g_build_filename (g_get_user_config_dir (),
                                  basename,
                                  NULL);
        g_free (basename);
#else
        char *basename = g_strdup_printf (".%src",
                                          app->priv->info->short_name);
        app->priv->info->rc_file =
                g_build_filename (g_get_home_dir (),
                                  basename,
                                  NULL);
        g_free (basename);
#endif
    }

    return app->priv->info->rc_file;
}


#ifdef MOO_USE_PYTHON
void             moo_app_python_execute_file   (G_GNUC_UNUSED GtkWindow *parent_window)
{
    GtkWidget *parent;
    const char *filename = NULL;
    FILE *file;

    g_return_if_fail (moo_app_python != NULL);

    parent = parent_window ? GTK_WIDGET (parent_window) : NULL;
    if (!filename)
        filename = moo_file_dialogp (parent,
                                     MOO_DIALOG_FILE_OPEN_EXISTING,
                                     "Choose Python Script to Execute",
                                     "python_exec_file", NULL);

    if (!filename) return;

    file = fopen (filename, "r");
    if (!file)
    {
        moo_error_dialog (parent, "Could not open file", NULL);
    }
    else
    {
        PyObject *res = (PyObject*)moo_python_run_file (moo_app_python, file, filename);
        fclose (file);
        if (res)
            Py_XDECREF (res);
        else
            PyErr_Print ();
    }
}


gboolean
moo_app_python_run_file (MooApp      *app,
                         const char  *filename)
{
    FILE *file;
    PyObject *res;

    g_return_val_if_fail (MOO_IS_APP (app), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (moo_app_python != NULL, FALSE);

    file = fopen (filename, "r");
    g_return_val_if_fail (file != NULL, FALSE);

    res = moo_python_run_file (moo_app_python, file, filename);
    fclose (file);

    if (res)
    {
        Py_XDECREF (res);
        return TRUE;
    }
    else
    {
        PyErr_Print ();
        return FALSE;
    }
}


gboolean
moo_app_python_run_string (MooApp      *app,
                           const char  *string)
{
    PyObject *res;

    g_return_val_if_fail (MOO_IS_APP (app), FALSE);
    g_return_val_if_fail (string != NULL, FALSE);
    g_return_val_if_fail (moo_app_python != NULL, FALSE);

    res = moo_python_run_string (moo_app_python, string, FALSE);

    if (res)
    {
        Py_XDECREF (res);
        return TRUE;
    }
    else
    {
        PyErr_Print ();
        return FALSE;
    }
}


GtkWidget       *moo_app_get_python_console    (G_GNUC_UNUSED MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    g_return_val_if_fail (moo_app_python != NULL, NULL);
    return GTK_WIDGET (moo_app_python->console);
}


void             moo_app_show_python_console   (G_GNUC_UNUSED MooApp *app)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (moo_app_python != NULL);
    gtk_window_present (GTK_WINDOW (moo_app_python->console));
}


void             moo_app_hide_python_console   (G_GNUC_UNUSED MooApp *app)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (moo_app_python != NULL);
    gtk_widget_hide (GTK_WIDGET (moo_app_python->console));
}
#endif /* !MOO_USE_PYTHON */


static guint strv_length (char **argv)
{
    guint len = 0;
    char **c;

    if (!argv)
        return 0;

    for (c = argv; *c != NULL; ++c)
        ++len;

    return len;
}


MooEditor       *moo_app_get_editor            (MooApp          *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    return app->priv->editor;
}


const MooAppInfo*moo_app_get_info               (MooApp     *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    return app->priv->info;
}


static gboolean
moo_app_init_real (MooApp *app)
{
    G_GNUC_UNUSED const char *app_dir;
    const char *rc_file;
    MooLangMgr *lang_mgr;
    MooUIXML *ui_xml;
    GError *error = NULL;

#if 0
    if (app->priv->enable_options)
    {
        int argc = strv_length (app->priv->argv);

        g_option_context_add_group (app->priv->option_ctx,
                                    gtk_get_option_group (TRUE));
        g_option_context_parse (app->priv->option_ctx, &argc,
                                &app->priv->argv, &error);

        if (error || argc > 1)
        {
            if (error)
            {
                g_print ("%s\n", error->message);
                g_error_free (error);
                error = NULL;
            }
            else
            {
                g_print ("Unknown option %s\n", app->priv->argv[1]);
            }

            g_print ("Type '%s --help' for usage\n", g_get_prgname ());

            exit (1);
        }
    }
#endif

#ifdef __WIN32__
    app_dir = moo_app_get_application_dir (app);
#endif

    rc_file = moo_app_get_rc_file_name (app);

    if (!moo_prefs_load (rc_file, &error))
    {
        g_warning ("%s: could not read config file", G_STRLOC);
        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }

    ui_xml = moo_app_get_ui_xml (app);

    if (app->priv->use_editor)
    {
        char *plugin_dir = NULL, *user_plugin_dir = NULL;
        char *lang_dir = NULL, *user_lang_dir = NULL;
        char *user_data_dir = NULL;

#ifdef __WIN32__

        const char *data_dir;

        if (app_dir[0])
            data_dir = app_dir;
        else
            data_dir = ".";

        lang_dir = g_build_filename (data_dir, MOO_LANG_DIR_BASENAME, NULL);
        plugin_dir = g_build_filename (data_dir, MOO_PLUGIN_DIR_BASENAME, NULL);

#else /* !__WIN32__ */

#ifdef MOO_TEXT_LANG_FILES_DIR
        lang_dir = g_strdup (MOO_TEXT_LANG_FILES_DIR);
#else
        lang_dir = g_build_filename (".", MOO_LANG_DIR_BASENAME, NULL);
#endif

#ifdef MOO_PLUGINS_DIR
        plugin_dir = g_strdup (MOO_PLUGINS_DIR);
#else
        plugin_dir = g_build_filename (".", MOO_PLUGIN_DIR_BASENAME, NULL);
#endif

        user_data_dir = g_strdup_printf (".%s", app->priv->info->short_name);
        user_plugin_dir = g_build_filename (user_data_dir, MOO_PLUGIN_DIR_BASENAME, NULL);
        user_lang_dir = g_build_filename (user_data_dir, MOO_LANG_DIR_BASENAME, NULL);

#endif /* !__WIN32__ */

        app->priv->editor = moo_editor_instance ();
        moo_editor_set_ui_xml (app->priv->editor, ui_xml);
        moo_editor_set_app_name (app->priv->editor,
                                 app->priv->info->short_name);

        lang_mgr = moo_editor_get_lang_mgr (app->priv->editor);
        moo_lang_mgr_add_dir (lang_mgr, lang_dir);

        if (user_lang_dir)
            moo_lang_mgr_add_dir (lang_mgr, user_lang_dir);

        moo_lang_mgr_read_dirs (lang_mgr);

        g_signal_connect_swapped (app->priv->editor,
                                  "all-windows-closed",
                                  G_CALLBACK (all_editors_closed),
                                  app);

        moo_plugin_init_builtin ();
        moo_plugin_read_dir (plugin_dir);
        if (user_plugin_dir)
            moo_plugin_read_dir (user_plugin_dir);

        g_free (lang_dir);
        g_free (plugin_dir);
        g_free (user_lang_dir);
        g_free (user_plugin_dir);
        g_free (user_data_dir);
    }

#if defined(__WIN32__) && defined(MOO_BUILD_TERM)
    if (app->priv->use_terminal)
    {
        moo_term_set_helper_directory (app_dir);
        g_message ("app dir: %s", app_dir);
    }
#endif /* __WIN32__ && MOO_BUILD_TERM */

    start_python (app);
    start_io (app);

    return TRUE;
}


static void     start_python            (G_GNUC_UNUSED MooApp *app)
{
#ifdef MOO_USE_PYTHON
    if (app->priv->run_python)
    {
        moo_app_python = moo_python_new ();
        moo_python_start (moo_app_python,
                          strv_length (app->priv->argv),
                          app->priv->argv);
#ifdef MOO_USE_PYGTK
        initmoo ();
#endif
    }
    else
    {
        moo_app_python = moo_python_get_instance ();
        if (moo_app_python)
            g_object_ref (moo_app_python);
    }
#endif /* !MOO_USE_PYTHON */
}


static void
start_io (MooApp *app)
{
    if (app->priv->run_input)
    {
        moo_app_input = moo_app_input_new (app->priv->info->short_name);
        moo_app_input_start (moo_app_input);
    }

    if (app->priv->run_output)
    {
        moo_app_output = moo_app_output_new (app->priv->info->short_name);
        moo_app_output_start (moo_app_output);
    }
}


static gboolean on_gtk_main_quit (MooApp *app)
{
    app->priv->quit_handler_id = 0;

    if (!moo_app_quit (app))
        MOO_APP_GET_CLASS(app)->quit (app);

    return FALSE;
}


static int      moo_app_run_real        (MooApp         *app)
{
    g_return_val_if_fail (!app->priv->running, 0);
    app->priv->running = TRUE;

    app->priv->quit_handler_id =
            gtk_quit_add (0, (GtkFunction) on_gtk_main_quit, app);

    gtk_main ();

    return app->priv->exit_code;
}


static gboolean moo_app_try_quit_real   (MooApp         *app)
{
    GSList *l, *list;

    if (!app->priv->running)
        return FALSE;

    list = g_slist_copy (app->priv->terminals);
    for (l = list; l != NULL; l = l->next)
    {
        if (!moo_window_close (MOO_WINDOW (l->data)))
        {
            g_slist_free (list);
            return TRUE;
        }
    }
    g_slist_free (list);

    if (!moo_editor_close_all (app->priv->editor))
        return TRUE;

    return FALSE;
}


static void     moo_app_quit_real       (MooApp         *app)
{
    GSList *l, *list;
    GError *error = NULL;

    if (!app->priv->running)
        return;
    else
        app->priv->running = FALSE;

    if (moo_app_input)
    {
        moo_app_input_shutdown (moo_app_input);
        moo_app_input_unref (moo_app_input);
        moo_app_input = NULL;
    }

    if (moo_app_output)
    {
        moo_app_output_shutdown (moo_app_output);
        moo_app_output_unref (moo_app_output);
        moo_app_output = NULL;
    }

#ifdef MOO_USE_PYTHON
    if (moo_app_python)
    {
        moo_python_shutdown (moo_app_python);
        g_object_unref (moo_app_python);
        moo_app_python = NULL;
    }
#endif

    list = g_slist_copy (app->priv->terminals);
    for (l = list; l != NULL; l = l->next)
        moo_window_close (MOO_WINDOW (l->data));
    g_slist_free (list);
    g_slist_free (app->priv->terminals);
    app->priv->terminals = NULL;
    app->priv->term_window = NULL;

    moo_editor_close_all (app->priv->editor);
    g_object_unref (app->priv->editor);
    app->priv->editor = NULL;

    if (!moo_prefs_save (moo_app_get_rc_file_name (app), &error))
    {
        g_warning ("%s: could not save config file", G_STRLOC);
        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }

    if (app->priv->quit_handler_id)
        gtk_quit_remove (app->priv->quit_handler_id);

    gtk_main_quit ();
}


void             moo_app_init                   (MooApp     *app)
{
    g_return_if_fail (MOO_IS_APP (app));

    MOO_APP_GET_CLASS(app)->init (app);
}


int              moo_app_run                    (MooApp     *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), -1);

    return MOO_APP_GET_CLASS(app)->run (app);
}


gboolean         moo_app_quit                   (MooApp     *app)
{
    gboolean stopped = FALSE;

    g_return_val_if_fail (MOO_IS_APP (app), FALSE);

    if (app->priv->in_try_quit || !app->priv->running)
        return TRUE;

    app->priv->in_try_quit = TRUE;
    g_signal_emit (app, signals[TRY_QUIT], 0, &stopped);
    app->priv->in_try_quit = FALSE;

    if (!stopped)
    {
        MOO_APP_GET_CLASS(app)->quit (app);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


static void     moo_app_set_name        (MooApp         *app,
                                         const char     *short_name,
                                         const char     *full_name)
{
    if (short_name)
    {
        g_free (app->priv->info->short_name);
        app->priv->info->short_name = g_strdup (short_name);
        g_object_notify (G_OBJECT (app), "short_name");
    }

    if (full_name)
    {
        g_free (app->priv->info->full_name);
        app->priv->info->full_name = g_strdup (full_name);
        g_object_notify (G_OBJECT (app), "full_name");
    }
}


static void     moo_app_set_description (MooApp         *app,
                                         const char     *description)
{
    g_free (app->priv->info->description);
    app->priv->info->description = g_strdup (description);
    g_object_notify (G_OBJECT (app), "description");
}


static void install_actions (MooApp *app, GType  type)
{
    MooWindowClass *klass = g_type_class_ref (type);
    char *about, *_about;

    g_return_if_fail (klass != NULL);

    about = g_strdup_printf ("About %s", app->priv->info->full_name);
    _about = g_strdup_printf ("_About %s", app->priv->info->full_name);

    moo_window_class_new_action (klass, "Quit",
                                 "name", "Quit",
                                 "label", "_Quit",
                                 "tooltip", "Quit",
                                 "icon-stock-id", GTK_STOCK_QUIT,
                                 "accel", "<ctrl>Q",
                                 "closure::callback", moo_app_quit,
                                 "closure::proxy-func", moo_app_get_instance,
                                 NULL);

    moo_window_class_new_action (klass, "Preferences",
                                 "name", "Preferences",
                                 "label", "Pre_ferences",
                                 "tooltip", "Preferences",
                                 "icon-stock-id", GTK_STOCK_PREFERENCES,
                                 "accel", "<ctrl>P",
                                 "closure::callback", moo_app_prefs_dialog,
                                 NULL);

    moo_window_class_new_action (klass, "About",
                                 "name", "About",
                                 "label", _about,
                                 "tooltip", about,
                                 "icon-stock-id", GTK_STOCK_ABOUT,
                                 "closure::callback", moo_app_about_dialog,
                                 NULL);

#ifdef MOO_USE_PYTHON
    moo_window_class_new_action (klass, "PythonMenu",
                                 "name", "Python Menu",
                                 "label", "P_ython",
                                 "visible", TRUE,
                                 "no-accel", TRUE,
                                 NULL);

    moo_window_class_new_action (klass, "ExecuteScript",
                                 "name", "Execute Script",
                                 "label", "_Execute Script",
                                 "tooltip", "Execute Script",
                                 "icon-stock-id", GTK_STOCK_EXECUTE,
                                 "closure::callback", moo_app_python_execute_file,
                                 NULL);

    moo_window_class_new_action (klass, "ShowPythonConsole",
                                 "name", "Show Python Console",
                                 "label", "Show Python Conso_le",
                                 "tooltip", "Show python console",
                                 "accel", "<alt>L",
                                 "closure::callback", moo_app_show_python_console,
                                 "closure::proxy-func", moo_app_get_instance,
                                 NULL);
#endif /* MOO_USE_PYTHON */

    g_type_class_unref (klass);
    g_free (about);
    g_free (_about);
}


#ifdef MOO_BUILD_TERM
static void terminal_destroyed (MooTermWindow *term,
                                MooApp        *app)
{
    MooAppWindowPolicy policy;
    gboolean quit;

    app->priv->terminals = g_slist_remove (app->priv->terminals,
                                           term);

    policy = app->priv->window_policy;
    quit = (policy & MOO_APP_QUIT_ON_CLOSE_ALL_TERMINALS) ||
            ((policy & MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS) &&
             !moo_editor_get_active_window (app->priv->editor));

    if (quit)
        moo_app_quit (app);
}


static MooTermWindow *new_terminal (MooApp *app)
{
    MooTermWindow *term;

    term = g_object_new (app->priv->term_window_type,
                         "ui-xml", app->priv->ui_xml,
                         NULL);
    app->priv->terminals = g_slist_append (app->priv->terminals, term);

    g_signal_connect (term, "destroy",
                      G_CALLBACK (terminal_destroyed), app);

    return term;
}


static void
open_terminal (void)
{
    MooApp *app = moo_app_get_instance ();
    MooTermWindow *term;

    g_return_if_fail (app != NULL);

    if (app->priv->terminals)
        term = app->priv->terminals->data;
    else
        term = new_terminal (app);

    gtk_window_present (GTK_WINDOW (term));
}


void
moo_app_set_terminal_type (MooApp     *app,
                           GType       type)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (g_type_is_a (type, MOO_TYPE_TERM_WINDOW));
    app->priv->term_window_type = type;
}


MooTermWindow*
moo_app_get_terminal (MooApp *app)
{
    MooTermWindow *term;

    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (app->priv->terminals)
    {
        return app->priv->terminals->data;
    }
    else
    {
        term = new_terminal (app);
        gtk_window_present (GTK_WINDOW (term));
        return term;
    }
}
#else /* !MOO_BUILD_TERM */
MooTermWindow *moo_app_get_terminal (MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    g_return_val_if_reached (NULL);
}
#endif /* !MOO_BUILD_TERM */


static void install_editor_actions  (MooApp *app)
{
    MooWindowClass *klass = g_type_class_ref (MOO_TYPE_EDIT_WINDOW);

    g_return_if_fail (klass != NULL);

    install_actions (app, MOO_TYPE_EDIT_WINDOW);

#ifdef MOO_USE_PYTHON
    moo_window_class_new_action (klass, "ExecuteSelection",
                                 "name", "Execute Selection",
                                 "label", "_Execute Selection",
                                 "tooltip", "Execute Selection",
                                 "icon-stock-id", GTK_STOCK_EXECUTE,
                                 "accel", "<shift><alt>Return",
                                 "closure::callback", execute_selection,
                                 NULL);
#endif /* !MOO_USE_PYTHON */

#ifdef MOO_BUILD_TERM
    moo_window_class_new_action (klass, "Terminal",
                                 "name", "Terminal",
                                 "label", "_Terminal",
                                 "tooltip", "Terminal",
                                 "icon-stock-id", MOO_STOCK_TERMINAL,
                                 "closure::callback", open_terminal,
                                 NULL);
#endif /* MOO_BUILD_TERM */

    g_type_class_unref (klass);
}


#ifdef MOO_BUILD_TERM
static void new_editor (MooApp *app)
{
    g_return_if_fail (app != NULL);
    gtk_window_present (GTK_WINDOW (moo_editor_new_window (app->priv->editor)));
}

static void open_in_editor (MooTermWindow *terminal)
{
    MooApp *app = moo_app_get_instance ();
    g_return_if_fail (app != NULL);
    moo_editor_open (app->priv->editor, NULL, GTK_WIDGET (terminal), NULL);
}


static void install_terminal_actions (MooApp *app)
{
    MooWindowClass *klass = g_type_class_ref (MOO_TYPE_TERM_WINDOW);

    g_return_if_fail (klass != NULL);

    install_actions (app, MOO_TYPE_TERM_WINDOW);

    moo_window_class_new_action (klass, "NewEditor",
                                 "name", "New Editor",
                                 "label", "_New Editor",
                                 "tooltip", "New Editor",
                                 "icon-stock-id", GTK_STOCK_EDIT,
                                 "accel", "<Alt>E",
                                 "closure::callback", new_editor,
                                 "closure::proxy-func", moo_app_get_instance,
                                 NULL);

    moo_window_class_new_action (klass, "OpenInEditor",
                                 "name", "Open In Editor",
                                 "label", "_Open In Editor",
                                 "tooltip", "Open In Editor",
                                 "icon-stock-id", GTK_STOCK_OPEN,
                                 "accel", "<Alt>O",
                                 "closure::callback", open_in_editor,
                                 NULL);

    g_type_class_unref (klass);
}
#else /* !MOO_BUILD_TERM */
static void install_terminal_actions (G_GNUC_UNUSED MooApp *app)
{
}
#endif /* !MOO_BUILD_TERM */


static void     all_editors_closed      (MooApp         *app)
{
    MooAppWindowPolicy policy = app->priv->window_policy;
    gboolean quit = (policy & MOO_APP_QUIT_ON_CLOSE_ALL_EDITORS) ||
            ((policy & MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS) && !app->priv->terminals);

    if (quit)
        moo_app_quit (app);
}


MooUIXML        *moo_app_get_ui_xml             (MooApp     *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (!app->priv->ui_xml)
        app->priv->ui_xml = moo_ui_xml_new ();

    return app->priv->ui_xml;
}


void             moo_app_set_ui_xml             (MooApp     *app,
                                                 MooUIXML   *xml)
{
    GSList *l;

    g_return_if_fail (MOO_IS_APP (app));

    if (app->priv->ui_xml == xml)
        return;

    if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);

    app->priv->ui_xml = xml;

    if (xml)
        g_object_ref (app->priv->ui_xml);

    if (app->priv->editor)
        moo_editor_set_ui_xml (app->priv->editor, xml);

    for (l = app->priv->terminals; l != NULL; l = l->next)
        moo_window_set_ui_xml (l->data, xml);
}


GType
moo_app_window_policy_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        static const GFlagsValue values[] = {
            { MOO_APP_QUIT_ON_CLOSE_ALL_EDITORS, (char*)"MOO_APP_QUIT_ON_CLOSE_ALL_EDITORS", (char*)"quit-on-close-all-editors" },
            { MOO_APP_QUIT_ON_CLOSE_ALL_TERMINALS, (char*)"MOO_APP_QUIT_ON_CLOSE_ALL_TERMINALS", (char*)"quit-on-close-all-terminals" },
            { MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS, (char*)"MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS", (char*)"quit-on-close-all-windows" },
            { 0, NULL, NULL }
        };

        type = g_flags_register_static ("MooAppWindowPolicy", values);
    }

    return type;
}


static MooAppInfo*
moo_app_info_new (void)
{
    return g_new0 (MooAppInfo, 1);
}


static MooAppInfo*
moo_app_info_copy (const MooAppInfo *info)
{
    MooAppInfo *copy;

    g_return_val_if_fail (info != NULL, NULL);

    copy = g_new (MooAppInfo, 1);

    copy->short_name = g_strdup (info->short_name);
    copy->full_name = g_strdup (info->full_name);
    copy->description = g_strdup (info->description);
    copy->version = g_strdup (info->version);
    copy->website = g_strdup (info->website);
    copy->website_label = g_strdup (info->website_label);
    copy->app_dir = g_strdup (info->app_dir);
    copy->rc_file = g_strdup (info->rc_file);

    return copy;
}


static void
moo_app_info_free (MooAppInfo *info)
{
    if (info)
    {
        g_free (info->short_name);
        g_free (info->full_name);
        g_free (info->description);
        g_free (info->version);
        g_free (info->website);
        g_free (info->website_label);
        g_free (info->app_dir);
        g_free (info->rc_file);
        g_free (info);
    }
}


GType            moo_app_info_get_type          (void)
{
    static GType type = 0;
    if (!type)
        g_boxed_type_register_static ("MooAppInfo",
                                      (GBoxedCopyFunc) moo_app_info_copy,
                                      (GBoxedFreeFunc) moo_app_info_free);
    return type;
}


#ifdef MOO_USE_PYTHON
static void     execute_selection       (MooEditWindow  *window)
{
    MooEdit *edit;
    char *text;

    edit = moo_edit_window_get_active_doc (window);

    g_return_if_fail (edit != NULL);

    text = moo_text_view_get_selection (MOO_TEXT_VIEW (edit));

    if (!text)
        text = moo_text_view_get_text (MOO_TEXT_VIEW (edit));

    if (text)
    {
        moo_app_python_run_string (moo_app_get_instance (), text);
        g_free (text);
    }
}
#endif


void
_moo_app_exec_cmd (MooApp     *app,
                   char        cmd,
                   const char *data,
                   guint       len)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (data != NULL);
    g_signal_emit (app, signals[EXEC_CMD], 0, cmd, data, len);
}


static void
moo_app_open_file (MooApp       *app,
                   const char   *filename)
{
    MooEditor *editor = moo_app_get_editor (app);
    g_return_if_fail (editor != NULL);
    moo_editor_open_file (editor, NULL, NULL, filename, NULL);
}


static void
moo_app_present (MooApp *app)
{
    gpointer window = app->priv->term_window;

    if (!window && app->priv->editor)
        window = moo_editor_get_active_window (app->priv->editor);

    g_return_if_fail (window != NULL);

    moo_window_present (window);
}


static MooAppCmdCode get_cmd_code (char cmd)
{
    guint i;

    for (i = 1; i < MOO_APP_CMD_LAST; ++i)
        if (cmd == moo_app_cmd_chars[i])
            return i;

    return MOO_APP_CMD_ZERO;
}

static void
moo_app_exec_cmd_real (MooApp             *app,
                       char                cmd,
                       const char         *data,
                       G_GNUC_UNUSED guint len)
{
    MooAppCmdCode code;

    g_return_if_fail (MOO_IS_APP (app));

    code = get_cmd_code (cmd);

    switch (code)
    {
#ifdef MOO_USE_PYTHON
        case MOO_APP_CMD_PYTHON_STRING:
            moo_app_python_run_string (app, data);
            break;
        case MOO_APP_CMD_PYTHON_FILE:
            moo_app_python_run_file (app, data);
            break;
#endif

        case MOO_APP_CMD_OPEN_FILE:
            moo_app_open_file (app, data);
            break;
        case MOO_APP_CMD_QUIT:
            moo_app_quit (app);
            break;
        case MOO_APP_CMD_DIE:
            MOO_APP_GET_CLASS(app)->quit (app);
            break;

        case MOO_APP_CMD_PRESENT:
            moo_app_present (app);
            break;

        default:
            g_warning ("%s: got unknown command %d", G_STRLOC, cmd);
    }
}
