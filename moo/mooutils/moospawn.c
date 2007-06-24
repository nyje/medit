/*
 *   moospawn.c
 *
 *   Copyright (C) 2004-2007 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#include "mooutils/moospawn.h"
#include "mooutils/moomarshals.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/moocompat.h"
#include <string.h>

#ifndef __WIN32__
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


struct _MooCmdPrivate {
    GSpawnFlags flags;
    MooCmdFlags cmd_flags;
    gboolean running;
    int exit_status;
    GPid pid;
    int out;
    int err;
    GIOChannel *stdout_io;
    GIOChannel *stderr_io;
    guint child_watch;
    guint stdout_watch;
    guint stderr_watch;
    GString *out_buffer;
    GString *err_buffer;
};

static void     moo_cmd_finalize        (GObject    *object);
static void     moo_cmd_dispose         (GObject    *object);

static void     moo_cmd_check_stop      (MooCmd     *cmd);
static void     moo_cmd_cleanup         (MooCmd     *cmd);

static gboolean moo_cmd_abort_real      (MooCmd     *cmd);
static gboolean moo_cmd_stdout_line     (MooCmd     *cmd,
                                         const char *line);
static gboolean moo_cmd_stderr_line     (MooCmd     *cmd,
                                         const char *line);

static gboolean moo_cmd_run_command     (MooCmd     *cmd,
                                         const char *working_dir,
                                         char      **argv,
                                         char      **envp,
                                         GSpawnFlags flags,
                                         MooCmdFlags cmd_flags,
                                         GSpawnChildSetupFunc child_setup,
                                         gpointer    user_data,
                                         GError    **error);

#if 0 && defined(__WIN32__)
static gboolean moo_win32_spawn_async_with_pipes (const gchar *working_directory,
                                                  gchar **argv,
                                                  gchar **envp,
                                                  GSpawnFlags flags,
                                                  GPid *child_pid,
                                                  gint *standard_input,
                                                  gint *standard_output,
                                                  gint *standard_error,
                                                  GError **error);
#endif


enum {
    ABORT,
    CMD_EXIT,
    STDOUT_LINE,
    STDERR_LINE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


/* MOO_TYPE_CMD */
G_DEFINE_TYPE (MooCmd, _moo_cmd, G_TYPE_OBJECT)


static void
_moo_cmd_class_init (MooCmdClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = moo_cmd_finalize;
    gobject_class->dispose = moo_cmd_dispose;

    klass->abort = moo_cmd_abort_real;
    klass->cmd_exit = NULL;
    klass->stdout_line = moo_cmd_stdout_line;
    klass->stderr_line = moo_cmd_stderr_line;

    g_type_class_add_private (klass, sizeof (MooCmdPrivate));

    signals[ABORT] =
            g_signal_new ("abort",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                          G_STRUCT_OFFSET (MooCmdClass, abort),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOL__VOID,
                          G_TYPE_BOOLEAN, 0);

    signals[CMD_EXIT] =
            g_signal_new ("cmd-exit",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooCmdClass, cmd_exit),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOL__INT,
                          G_TYPE_BOOLEAN, 1,
                          G_TYPE_INT);

    signals[STDOUT_LINE] =
            g_signal_new ("stdout-line",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooCmdClass, stdout_line),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOL__STRING,
                          G_TYPE_BOOLEAN, 1,
                          G_TYPE_STRING);

    signals[STDERR_LINE] =
            g_signal_new ("stderr-line",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooCmdClass, stderr_line),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOL__STRING,
                          G_TYPE_BOOLEAN, 1,
                          G_TYPE_STRING);
}


static void
_moo_cmd_init (MooCmd *cmd)
{
    cmd->priv = G_TYPE_INSTANCE_GET_PRIVATE (cmd, MOO_TYPE_CMD, MooCmdPrivate);
    cmd->priv->out_buffer = g_string_new (NULL);
    cmd->priv->err_buffer = g_string_new (NULL);
}


static void
moo_cmd_finalize (GObject *object)
{
    MooCmd *cmd = MOO_CMD (object);

    g_string_free (cmd->priv->out_buffer, TRUE);
    g_string_free (cmd->priv->err_buffer, TRUE);

    G_OBJECT_CLASS (_moo_cmd_parent_class)->finalize (object);
}


static void
moo_cmd_dispose (GObject *object)
{
    MooCmd *cmd = MOO_CMD (object);

    if (cmd->priv->pid)
        _moo_cmd_abort (cmd);
    moo_cmd_cleanup (cmd);

    G_OBJECT_CLASS (_moo_cmd_parent_class)->dispose (object);
}


MooCmd*
_moo_cmd_new (const char *working_dir,
              char      **argv,
              char      **envp,
              GSpawnFlags flags,
              MooCmdFlags cmd_flags,
              GSpawnChildSetupFunc child_setup,
              gpointer    user_data,
              GError    **error)
{
    MooCmd *cmd;
    gboolean result;

    g_return_val_if_fail (argv && *argv, NULL);

    cmd = g_object_new (MOO_TYPE_CMD, NULL);

    result = moo_cmd_run_command (cmd, working_dir, argv, envp,
                                  flags, cmd_flags, child_setup,
                                  user_data, error);

    if (result)
    {
        return cmd;
    }
    else
    {
        g_object_unref (cmd);
        return NULL;
    }
}


static void
command_exit (GPid    pid,
              gint    status,
              MooCmd *cmd)
{
    g_return_if_fail (pid == cmd->priv->pid);

    cmd->priv->child_watch = 0;
    cmd->priv->exit_status = status;

    g_spawn_close_pid (cmd->priv->pid);
    cmd->priv->pid = 0;

    moo_cmd_check_stop (cmd);
}


static void
process_line (MooCmd      *cmd,
              const char  *line,
              gboolean     err)
{
    gboolean dummy = FALSE;
    const char *real_line = line;
    char *freeme = NULL;

    if (cmd->priv->cmd_flags & MOO_CMD_UTF8_OUTPUT)
    {
        const char *charset;

        real_line = NULL;

        if (g_get_charset (&charset))
        {
            const char *end;

            if (!g_utf8_validate (line, -1, &end))
            {
                g_warning ("%s: invalid unicode:\n%s", G_STRLOC, line);

                if (end > line)
                {
                    freeme = g_strndup (line, end - line);
                    real_line = freeme;
                }
            }
            else
            {
                real_line = line;
            }
        }
        else
        {
            GError *error = NULL;

            freeme = g_convert_with_fallback (line, -1, "UTF-8", charset,
                                              NULL, NULL, NULL, &error);

            if (!freeme)
            {
                g_warning ("%s: could not convert text to UTF-8:\n%s",
                           G_STRLOC, line);
                g_warning ("%s: %s", G_STRLOC, error->message);
            }

            real_line = freeme;
        }
    }

    if (real_line)
    {
        if (err)
            g_signal_emit (cmd, signals[STDERR_LINE], 0, real_line, &dummy);
        else
            g_signal_emit (cmd, signals[STDOUT_LINE], 0, real_line, &dummy);
    }

    g_free (freeme);
}


static gboolean
command_out_or_err (MooCmd         *cmd,
                    GIOChannel     *channel,
                    GIOCondition    condition,
                    gboolean        out)
{
    GSList *lines;
    GError *error = NULL;
    GIOStatus status;
    gsize count;

    count = 0;
    lines = NULL;

    while (count < 4096)
    {
        char *line = NULL;
        gsize line_end;

        status = g_io_channel_read_line (channel, &line, NULL, &line_end, &error);

        if (!line)
            break;

        count += line_end;
        line[line_end] = 0;
        lines = g_slist_prepend (lines, line);
    }

    lines = g_slist_reverse (lines);

    while (lines)
    {
        process_line (cmd, lines->data, !out);
        g_free (lines->data);
        lines = g_slist_delete_link (lines, lines);
    }

    if (error)
    {
        g_warning ("%s: %s", G_STRLOC, error->message);
        g_error_free (error);
        return FALSE;
    }

    if (condition & (G_IO_ERR | G_IO_HUP))
        return FALSE;

    if (status == G_IO_STATUS_EOF)
        return FALSE;

    return TRUE;
}


static gboolean
command_out (GIOChannel     *channel,
             GIOCondition    condition,
             MooCmd         *cmd)
{
    return command_out_or_err (cmd, channel, condition, TRUE);
}


static gboolean
command_err (GIOChannel     *channel,
             GIOCondition    condition,
             MooCmd         *cmd)
{
    return command_out_or_err (cmd, channel, condition, FALSE);
}


static void
try_channel_leftover (MooCmd      *cmd,
                      GIOChannel  *channel,
                      gboolean     out)
{
    char *text;

    g_io_channel_read_to_end (channel, &text, NULL, NULL);

    if (text)
    {
        char **lines, **p;

        lines = _moo_splitlines (text);

        if (lines)
        {
            for (p = lines; *p != NULL; p++)
                if (**p)
                    process_line (cmd, *p, !out);
        }

        g_strfreev (lines);
        g_free (text);
    }
}


static void
stdout_watch_removed (MooCmd *cmd)
{
    if (cmd->priv->stdout_io)
    {
        try_channel_leftover (cmd, cmd->priv->stdout_io, TRUE);
        g_io_channel_unref (cmd->priv->stdout_io);
    }

    cmd->priv->stdout_io = NULL;
    cmd->priv->stdout_watch = 0;
    moo_cmd_check_stop (cmd);
}


static void
stderr_watch_removed (MooCmd *cmd)
{
    if (cmd->priv->stderr_io)
    {
        try_channel_leftover (cmd, cmd->priv->stderr_io, FALSE);
        g_io_channel_unref (cmd->priv->stderr_io);
    }

    cmd->priv->stderr_io = NULL;
    cmd->priv->stderr_watch = 0;
    moo_cmd_check_stop (cmd);
}


#ifndef __WIN32__
static void
real_child_setup (gpointer user_data)
{
    struct {
        GSpawnChildSetupFunc child_setup;
        gpointer user_data;
    } *data = user_data;

    setpgid (0, 0);

    if (data->child_setup)
        data->child_setup (data->user_data);
}
#endif


char **
_moo_env_add (char **add)
{
    GPtrArray *new_env;
    char **names, **p;

    if (!add)
        return NULL;

    new_env = g_ptr_array_new ();
    names = g_listenv ();

    for (p = names; p && *p; ++p)
    {
        const char *val = g_getenv (*p);

        if (val)
            g_ptr_array_add (new_env, g_strdup_printf ("%s=%s", *p, val));
    }

    for (p = add; p && *p; ++p)
        g_ptr_array_add (new_env, g_strdup (*p));

    g_strfreev (names);

    g_ptr_array_add (new_env, NULL);
    return (char**) g_ptr_array_free (new_env, FALSE);
}


static gboolean
moo_cmd_run_command (MooCmd     *cmd,
                     const char *working_dir,
                     char      **argv,
                     char      **envp,
                     GSpawnFlags flags,
                     MooCmdFlags cmd_flags,
                     G_GNUC_UNUSED GSpawnChildSetupFunc child_setup,
                     G_GNUC_UNUSED gpointer    user_data,
                     GError    **error)
{
    gboolean result;
    int *outp, *errp;
    char **new_env;

#ifndef __WIN32__
    struct {
        GSpawnChildSetupFunc child_setup;
        gpointer user_data;
    } data;
#endif

    g_return_val_if_fail (MOO_IS_CMD (cmd), FALSE);
    g_return_val_if_fail (argv && argv[0], FALSE);
    g_return_val_if_fail (!cmd->priv->running, FALSE);

#ifdef __WIN32__
    if (cmd_flags & MOO_CMD_OPEN_CONSOLE)
    {
        outp = NULL;
        errp = NULL;
    }
    else
#endif
    {
        if ((flags & G_SPAWN_STDOUT_TO_DEV_NULL) || (cmd_flags & MOO_CMD_STDOUT_TO_PARENT))
            outp = NULL;
        else
            outp = &cmd->priv->out;

        if ((flags & G_SPAWN_STDERR_TO_DEV_NULL) || (cmd_flags & MOO_CMD_STDERR_TO_PARENT))
            errp = NULL;
        else
            errp = &cmd->priv->err;
    }

    new_env = _moo_env_add (envp);

#ifndef __WIN32__
    data.child_setup = child_setup;
    data.user_data = user_data;
#endif

#if 0 && defined(__WIN32__)
    if (!(cmd_flags & MOO_CMD_OPEN_CONSOLE))
        result = moo_win32_spawn_async_with_pipes (working_dir,
                                                   argv, new_env,
                                                   flags | G_SPAWN_DO_NOT_REAP_CHILD,
                                                   &cmd->priv->pid,
                                                   NULL, outp, errp, error);
    else
#endif
    result = g_spawn_async_with_pipes (working_dir,
                                       argv, new_env,
                                       flags | G_SPAWN_DO_NOT_REAP_CHILD,
#ifndef __WIN32__
                                       real_child_setup, &data,
#else
                                       NULL, NULL,
#endif
                                       &cmd->priv->pid,
                                       NULL, outp, errp, error);

    g_strfreev (new_env);

    if (!result)
        return FALSE;

    cmd->priv->running = TRUE;
    cmd->priv->flags = flags;
    cmd->priv->cmd_flags = cmd_flags;

    cmd->priv->child_watch =
            g_child_watch_add (cmd->priv->pid,
                               (GChildWatchFunc) command_exit,
                               cmd);

    if (outp)
    {
        cmd->priv->stdout_io = g_io_channel_unix_new (cmd->priv->out);
        g_io_channel_set_encoding (cmd->priv->stdout_io, NULL, NULL);
        g_io_channel_set_buffered (cmd->priv->stdout_io, TRUE);
        g_io_channel_set_flags (cmd->priv->stdout_io, G_IO_FLAG_NONBLOCK, NULL);
        g_io_channel_set_close_on_unref (cmd->priv->stdout_io, TRUE);
        cmd->priv->stdout_watch =
                _moo_io_add_watch_full (cmd->priv->stdout_io,
                                        G_PRIORITY_DEFAULT_IDLE,
                                        G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
                                        (GIOFunc) command_out, cmd,
                                        (GDestroyNotify) stdout_watch_removed);
    }

    if (errp)
    {
        cmd->priv->stderr_io = g_io_channel_unix_new (cmd->priv->err);
        g_io_channel_set_encoding (cmd->priv->stderr_io, NULL, NULL);
        g_io_channel_set_buffered (cmd->priv->stderr_io, TRUE);
        g_io_channel_set_flags (cmd->priv->stderr_io, G_IO_FLAG_NONBLOCK, NULL);
        g_io_channel_set_close_on_unref (cmd->priv->stderr_io, TRUE);
        cmd->priv->stderr_watch =
                _moo_io_add_watch_full (cmd->priv->stderr_io,
                                        G_PRIORITY_DEFAULT_IDLE,
                                        G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
                                        (GIOFunc) command_err, cmd,
                                        (GDestroyNotify) stderr_watch_removed);
    }

    return TRUE;
}


static void
moo_cmd_check_stop (MooCmd *cmd)
{
    gboolean result;

    if (!cmd->priv->running)
        return;

    if (!cmd->priv->child_watch && !cmd->priv->stdout_watch && !cmd->priv->stderr_watch)
    {
        g_object_ref (cmd);
        g_signal_emit (cmd, signals[CMD_EXIT], 0, cmd->priv->exit_status, &result);
        moo_cmd_cleanup (cmd);
        g_object_unref (cmd);
    }
}


static void
moo_cmd_cleanup (MooCmd *cmd)
{
    if (!cmd->priv->running)
        return;

    cmd->priv->running = FALSE;

    if (cmd->priv->child_watch)
        g_source_remove (cmd->priv->child_watch);
    if (cmd->priv->stdout_watch)
        g_source_remove (cmd->priv->stdout_watch);
    if (cmd->priv->stderr_watch)
        g_source_remove (cmd->priv->stderr_watch);

    if (cmd->priv->stdout_io)
        g_io_channel_unref (cmd->priv->stdout_io);
    if (cmd->priv->stderr_io)
        g_io_channel_unref (cmd->priv->stderr_io);

    if (cmd->priv->pid)
    {
#ifndef __WIN32__
        kill (-cmd->priv->pid, SIGHUP);
#else
        TerminateProcess (cmd->priv->pid, 1);
#endif
        g_spawn_close_pid (cmd->priv->pid);
        cmd->priv->pid = 0;
    }

    cmd->priv->pid = 0;
    cmd->priv->out = -1;
    cmd->priv->err = -1;
    cmd->priv->child_watch = 0;
    cmd->priv->stdout_watch = 0;
    cmd->priv->stderr_watch = 0;
    cmd->priv->stdout_io = NULL;
    cmd->priv->stderr_io = NULL;
}


static gboolean
moo_cmd_abort_real (MooCmd *cmd)
{
    if (!cmd->priv->running)
        return TRUE;

    g_return_val_if_fail (cmd->priv->pid != 0, TRUE);

#ifndef __WIN32__
    kill (-cmd->priv->pid, SIGHUP);
#else
    TerminateProcess (cmd->priv->pid, 1);
#endif

    if (cmd->priv->stdout_watch)
    {
        g_source_remove (cmd->priv->stdout_watch);
        cmd->priv->stdout_watch = 0;
    }

    if (cmd->priv->stderr_watch > 0)
    {
        g_source_remove (cmd->priv->stderr_watch);
        cmd->priv->stderr_watch = 0;
    }

    return TRUE;
}


void
_moo_cmd_abort (MooCmd *cmd)
{
    gboolean handled;
    g_return_if_fail (MOO_IS_CMD (cmd));
    g_signal_emit (cmd, signals[ABORT], 0, &handled);
}


static gboolean
moo_cmd_stdout_line (MooCmd     *cmd,
                     const char *line)
{
    if (cmd->priv->cmd_flags & MOO_CMD_COLLECT_STDOUT)
        g_string_append (cmd->priv->out_buffer, line);
    return FALSE;
}


static gboolean
moo_cmd_stderr_line (MooCmd     *cmd,
                     const char *line)
{
    if (cmd->priv->cmd_flags & MOO_CMD_COLLECT_STDERR)
        g_string_append (cmd->priv->err_buffer, line);
    return FALSE;
}


gboolean
_moo_unix_spawn_async (char      **argv,
                       GSpawnFlags g_flags,
                       GError    **error)
{
    return g_spawn_async (NULL, argv, NULL, g_flags,
                          NULL, NULL, NULL, error);
}
