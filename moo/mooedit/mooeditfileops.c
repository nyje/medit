/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   mooeditfileops.c
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

#define MOOEDIT_COMPILATION
#include "mooedit/mooedit-private.h"
#include "mooedit/mooeditfileops.h"
#include "mooedit/mooeditdialogs.h"
#include "mooedit/mootextbuffer.h"
#include "mooutils/moocompat.h"
#include "mooutils/moofilewatch.h"
#include <string.h>


static MooEditLoader *default_loader = NULL;
static MooEditSaver *default_saver = NULL;
static GSList *UNTITLED = NULL;
static GHashTable *UNTITLED_NO = NULL;


static gboolean moo_edit_load_default       (MooEditLoader  *loader,
                                             MooEdit        *edit,
                                             const char     *file,
                                             const char     *encoding,
                                             GError        **error);
static gboolean moo_edit_reload_default     (MooEditLoader  *loader,
                                             MooEdit        *edit,
                                             GError        **error);
static gboolean moo_edit_save_default       (MooEditSaver   *saver,
                                             MooEdit        *edit,
                                             const char     *file,
                                             const char     *encoding,
                                             GError        **error);
static gboolean moo_edit_save_copy_default  (MooEditSaver   *saver,
                                             MooEdit        *edit,
                                             const char     *file,
                                             const char     *encoding,
                                             GError        **error);

static void     block_buffer_signals        (MooEdit        *edit);
static void     unblock_buffer_signals      (MooEdit        *edit);
static gboolean focus_in_cb                 (MooEdit        *edit);
static void     check_file_status           (MooEdit        *edit,
                                             gboolean        in_focus_only);
static void     file_modified_on_disk       (MooEdit        *edit);
static void     file_deleted                (MooEdit        *edit);
static void     add_status                  (MooEdit        *edit,
                                             MooEditStatus   s);


MooEditLoader   *moo_edit_loader_get_default    (void)
{
    if (!default_loader)
    {
        default_loader = g_new0 (MooEditLoader, 1);
        default_loader->ref_count = 1;
        default_loader->load = moo_edit_load_default;
        default_loader->reload = moo_edit_reload_default;
    }

    return default_loader;
}


MooEditSaver    *moo_edit_saver_get_default     (void)
{
    if (!default_saver)
    {
        default_saver = g_new0 (MooEditSaver, 1);
        default_saver->ref_count = 1;
        default_saver->save = moo_edit_save_default;
        default_saver->save_copy = moo_edit_save_copy_default;
    }

    return default_saver;
}


MooEditLoader   *moo_edit_loader_ref            (MooEditLoader  *loader)
{
    g_return_val_if_fail (loader != NULL, NULL);
    loader->ref_count++;
    return loader;
}


MooEditSaver    *moo_edit_saver_ref             (MooEditSaver   *saver)
{
    g_return_val_if_fail (saver != NULL, NULL);
    saver->ref_count++;
    return saver;
}


void             moo_edit_loader_unref          (MooEditLoader  *loader)
{
    if (!loader || --loader->ref_count)
        return;

    g_free (loader);

    if (loader == default_loader)
        default_loader = NULL;
}


void             moo_edit_saver_unref           (MooEditSaver   *saver)
{
    if (!saver || --saver->ref_count)
        return;

    g_free (saver);

    if (saver == default_saver)
        default_saver = NULL;
}


gboolean         moo_edit_load              (MooEditLoader  *loader,
                                             MooEdit        *edit,
                                             const char     *filename,
                                             const char     *encoding,
                                             GError        **error)
{
    char *filename_copy, *encoding_copy;
    gboolean result;

    g_return_val_if_fail (loader != NULL, FALSE);
    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    filename_copy = g_strdup (filename);
    encoding_copy = g_strdup (encoding);

    result = loader->load (loader, edit, filename_copy, encoding_copy, error);

    g_free (filename_copy);
    g_free (encoding_copy);
    return result;
}


gboolean         moo_edit_reload            (MooEditLoader  *loader,
                                             MooEdit        *edit,
                                             GError        **error)
{
    g_return_val_if_fail (loader != NULL, FALSE);
    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);

    return loader->reload (loader, edit, error);
}


gboolean         moo_edit_save              (MooEditSaver   *saver,
                                             MooEdit        *edit,
                                             const char     *filename,
                                             const char     *encoding,
                                             GError        **error)
{
    char *filename_copy, *encoding_copy;
    gboolean result;

    g_return_val_if_fail (saver != NULL, FALSE);
    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    filename_copy = g_strdup (filename);
    encoding_copy = g_strdup (encoding);

    result = saver->save (saver, edit, filename_copy, encoding_copy, error);

    g_free (filename_copy);
    g_free (encoding_copy);
    return result;
}


gboolean
moo_edit_save_copy (MooEditSaver   *saver,
                    MooEdit        *edit,
                    const char     *filename,
                    const char     *encoding,
                    GError        **error)
{
    char *filename_copy, *encoding_copy;
    gboolean result;

    g_return_val_if_fail (saver != NULL, FALSE);
    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    filename_copy = g_strdup (filename);
    encoding_copy = g_strdup (encoding);

    result = saver->save_copy (saver, edit, filename_copy, encoding_copy, error);

    g_free (filename_copy);
    g_free (encoding_copy);
    return result;
}


/***************************************************************************/
/* File loading
 */

static gboolean do_load                 (MooEdit        *edit,
                                         const char     *file,
                                         const char     *encoding,
                                         GError        **error);

static const char *common_encodings[] = {
    "UTF8",
    "ISO_8859-15",
    "ISO_8859-1"
};


static gboolean
try_load (MooEdit        *edit,
          const char     *file,
          const char     *encoding,
          GError        **error)
{
    GtkTextIter start, end;
    GtkTextBuffer *buffer;

    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (file && file[0], FALSE);
    g_return_val_if_fail (encoding && encoding[0], FALSE);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    gtk_text_buffer_get_bounds (buffer, &start, &end);
    gtk_text_buffer_delete (buffer, &start, &end);

    return do_load (edit, file, encoding, error);
}


static gboolean
moo_edit_load_default (G_GNUC_UNUSED MooEditLoader *loader,
                       MooEdit        *edit,
                       const char     *file,
                       const char     *encoding,
                       GError        **error)
{
    GtkTextIter start;
    GtkTextBuffer *buffer;
    MooTextView *view;
    gboolean undo;
    gboolean success = FALSE;

    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (file && file[0], FALSE);

    if (moo_edit_is_empty (edit))
        undo = FALSE;
    else
        undo = TRUE;

    view = MOO_TEXT_VIEW (edit);
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    block_buffer_signals (edit);

    if (undo)
        gtk_text_buffer_begin_user_action (buffer);
    else
        moo_text_view_start_not_undoable_action (view);

    if (!encoding)
    {
        guint i;

        for (i = 0; i < G_N_ELEMENTS (common_encodings); ++i)
        {
            g_clear_error (error);
            encoding = common_encodings[i];
            success = try_load (edit, file, encoding, error);
            if (success)
                break;
        }

        if (!success)
        {
            const char *locale_charset;
            if (!g_get_charset (&locale_charset))
            {
                g_clear_error (error);
                encoding = locale_charset;
                success = try_load (edit, file, encoding, error);
            }
        }
    }
    else
    {
        success = try_load (edit, file, encoding, error);
    }

    unblock_buffer_signals (edit);

    if (success)
    {
        gtk_text_buffer_get_start_iter (buffer, &start);
        gtk_text_buffer_place_cursor (buffer, &start);
        _moo_edit_set_filename (edit, file, encoding);
        _moo_edit_start_file_watch (edit);
    }
    else
    {
        _moo_edit_stop_file_watch (edit);
    }

    if (undo)
        gtk_text_buffer_end_user_action (buffer);
    else
        moo_text_view_end_not_undoable_action (view);

    edit->priv->status = 0;
    moo_edit_set_modified (edit, FALSE);

//     _moo_text_buffer_dump (buffer, "/tmp/dump.c");

    return success;
}


static gboolean
do_load (MooEdit        *edit,
         const char     *filename,
         const char     *encoding,
         GError        **error)
{
    GIOChannel *file = NULL;
    GIOStatus status;
    GtkTextBuffer *buffer;

    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (encoding != NULL, FALSE);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    if (!strcmp (encoding, "UTF-8") || !strcmp (encoding, "UTF8"))
    {
        char *contents;
        gsize len;
        GMappedFile *mapped_file = g_mapped_file_new (filename, FALSE, error);

        if (mapped_file)
        {
            contents = g_mapped_file_get_contents (mapped_file);
            len = g_mapped_file_get_length (mapped_file);

            if (!g_utf8_validate (contents, len, NULL))
            {
                g_mapped_file_free (mapped_file);
                g_set_error (error, G_CONVERT_ERROR,
                             G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                             "Invalid UTF8 data read from file");
                return FALSE;
            }

            if (len)
                gtk_text_buffer_insert_at_cursor (buffer, contents, len);

            g_mapped_file_free (mapped_file);
            return TRUE;
        }
        else
        {
            g_warning ("%s: could not create mapped file", G_STRLOC);
            g_warning ("%s: %s", G_STRLOC, (*error)->message);
            g_clear_error (error);
        }
    }

    file = g_io_channel_new_file (filename, "r", error);

    if (!file)
        return FALSE;

    if (g_io_channel_set_encoding (file, encoding, error) != G_IO_STATUS_NORMAL)
    {
        g_io_channel_shutdown (file, TRUE, NULL);
        g_io_channel_unref (file);
        return FALSE;
    }

    while (TRUE)
    {
        char *line = NULL;
        gsize len;

        status = g_io_channel_read_line (file, &line, &len, NULL, error);

        if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_EOF)
        {
            g_io_channel_shutdown (file, TRUE, NULL);
            g_io_channel_unref (file);
            g_free (line);
            return FALSE;
        }

        if (line)
        {
            if (!g_utf8_validate (line, len, NULL))
            {
                g_io_channel_shutdown (file, TRUE, NULL);
                g_io_channel_unref (file);
                g_set_error (error, G_CONVERT_ERROR,
                             G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                             "Invalid UTF8 data read from file");
                g_free (line);
                return FALSE;
            }

            gtk_text_buffer_insert_at_cursor (buffer, line, len);
            g_free (line);
        }

        if (status == G_IO_STATUS_EOF)
        {
            g_io_channel_shutdown (file, TRUE, NULL);
            g_io_channel_unref (file);
            break;
        }
    }

    g_clear_error (error);
    return TRUE;
}


static gboolean moo_edit_reload_default (MooEditLoader  *loader,
                                         MooEdit        *edit,
                                         GError        **error)
{
    GtkTextIter start, end;
    GtkTextBuffer *buffer;
    gboolean result;

    g_return_val_if_fail (edit->priv->filename != NULL, FALSE);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    block_buffer_signals (edit);
    gtk_text_buffer_begin_user_action (buffer);

    gtk_text_buffer_get_bounds (buffer, &start, &end);
    gtk_text_buffer_delete (buffer, &start, &end);

    result = moo_edit_load (loader, edit, edit->priv->filename,
                            edit->priv->encoding, error);

    gtk_text_buffer_end_user_action (buffer);
    unblock_buffer_signals (edit);

    if (result)
    {
        edit->priv->status = 0;
        moo_edit_set_modified (edit, FALSE);
        _moo_edit_start_file_watch (edit);
        g_clear_error (error);
    }

    return result;
}


/***************************************************************************/
/* File saving
 */

/* XXX */
static const char *line_end[3] = {"\n", "\r\n", "\r"};
static guint line_end_len[3] = {1, 2, 1};

static gboolean do_write                (MooEdit        *edit,
                                         const char     *file,
                                         const char     *encoding,
                                         GError        **error);


static gboolean
moo_edit_save_default (G_GNUC_UNUSED MooEditSaver *saver,
                       MooEdit        *edit,
                       const char     *filename,
                       const char     *encoding,
                       GError        **error)
{
    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (filename && filename[0], FALSE);

    if (!encoding)
        encoding = "UTF8";

    if (!do_write (edit, filename, encoding, error))
        return FALSE;

    edit->priv->status = 0;
    _moo_edit_set_filename (edit, filename, encoding);
    moo_edit_set_modified (edit, FALSE);
    _moo_edit_start_file_watch (edit);

    return TRUE;
}


static gboolean
moo_edit_save_copy_default (G_GNUC_UNUSED MooEditSaver *saver,
                            MooEdit        *edit,
                            const char     *filename,
                            const char     *encoding,
                            GError        **error)
{
    g_return_val_if_fail (MOO_IS_EDIT (edit), FALSE);
    g_return_val_if_fail (filename && filename[0], FALSE);
    return do_write (edit, filename, encoding ? encoding : "UTF8", error);
}


static gboolean do_write                (MooEdit        *edit,
                                         const char     *filename,
                                         const char     *encoding,
                                         GError        **error)
{
    GIOChannel *file;
    GIOStatus status;
    GtkTextIter line_start;
    const char *le = line_end[edit->priv->line_end_type];
    gssize le_len = line_end_len[edit->priv->line_end_type];
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    g_return_val_if_fail (filename != NULL, FALSE);

    if (encoding && (!strcmp (encoding, "UTF-8") || !strcmp (encoding, "UTF8")))
        encoding = NULL;

    file = g_io_channel_new_file (filename, "w", error);

    if (!file)
        return FALSE;

    if (encoding && g_io_channel_set_encoding (file, encoding, error) != G_IO_STATUS_NORMAL)
    {
        g_io_channel_shutdown (file, TRUE, NULL);
        g_io_channel_unref (file);
        return FALSE;
    }

    gtk_text_buffer_get_start_iter (buffer, &line_start);

    do
    {
        gsize written;
        GtkTextIter line_end = line_start;
        gboolean write_line_end = FALSE;

        if (!gtk_text_iter_ends_line (&line_start))
        {
            char *line;
            gssize len = -1;

            gtk_text_iter_forward_to_line_end (&line_end);
            line = gtk_text_buffer_get_text (buffer, &line_start, &line_end, FALSE);

            status = g_io_channel_write_chars (file, line, len, &written, error);
            g_free (line);

            /* XXX */
            if (status != G_IO_STATUS_NORMAL)
            {
                g_io_channel_shutdown (file, TRUE, NULL);
                g_io_channel_unref (file);
                return FALSE;
            }
        }

        if (gtk_text_iter_ends_line (&line_start))
            write_line_end = !gtk_text_iter_is_end (&line_start);
        else
            write_line_end = !gtk_text_iter_is_end (&line_end);

        if (write_line_end)
        {
            status = g_io_channel_write_chars (file, le, le_len, &written, error);

            if (written < (gsize)le_len || status != G_IO_STATUS_NORMAL)
            {
                g_io_channel_shutdown (file, TRUE, NULL);
                g_io_channel_unref (file);
                return FALSE;
            }
        }
    }
    while (gtk_text_iter_forward_line (&line_start));

    status = g_io_channel_shutdown (file, TRUE, error);

    if (status != G_IO_STATUS_NORMAL)
    {
        g_io_channel_unref (file);
        return FALSE;
    }

    g_io_channel_unref (file);
    g_clear_error (error);
    return TRUE;
}


/***************************************************************************/
/* Aux stuff
 */

static void     block_buffer_signals        (MooEdit        *edit)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));
    g_signal_handler_block (buffer, edit->priv->modified_changed_handler_id);
}


static void     unblock_buffer_signals      (MooEdit        *edit)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));
    g_signal_handler_unblock (buffer, edit->priv->modified_changed_handler_id);
}


static void
file_watch_event (G_GNUC_UNUSED MooFileWatch *watch,
                  MooFileWatchEvent  *event,
                  MooEdit            *edit)
{
    if (event->monitor_id != edit->priv->file_monitor_id)
        return;

    g_return_if_fail (edit->priv->filename != NULL);
    g_return_if_fail (!(edit->priv->status & MOO_EDIT_CHANGED_ON_DISK));

    switch (event->code)
    {
        case MOO_FILE_WATCH_CHANGED:
            edit->priv->modified_on_disk = TRUE;
            break;

        case MOO_FILE_WATCH_DELETED:
            edit->priv->deleted_from_disk = TRUE;
            break;

        case MOO_FILE_WATCH_CREATED:
        case MOO_FILE_WATCH_MOVED:
            g_return_if_reached ();
    }

    check_file_status (edit, TRUE);
}


void
_moo_edit_start_file_watch (MooEdit        *edit)
{
    MooFileWatch *watch;
    GError *error = NULL;

    watch = _moo_editor_get_file_watch (edit->priv->editor);
    g_return_if_fail (MOO_IS_FILE_WATCH (watch));

    if (edit->priv->file_monitor_id)
        moo_file_watch_cancel_monitor (watch, edit->priv->file_monitor_id);
    edit->priv->file_monitor_id = 0;

    g_return_if_fail ((edit->priv->status & MOO_EDIT_CHANGED_ON_DISK) == 0);
    g_return_if_fail (edit->priv->filename != NULL);

    moo_file_watch_monitor_file (watch, edit->priv->filename, edit,
                                 &edit->priv->file_monitor_id,
                                 &error);

    if (!edit->priv->file_monitor_id)
    {
        g_warning ("%s: could not start watch", G_STRLOC);
        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
        return;
    }

    /* XXX watch errors too */
    if (!edit->priv->file_watch_event_handler_id)
        edit->priv->file_watch_event_handler_id =
                g_signal_connect (watch, "event",
                                  G_CALLBACK (file_watch_event), edit);

    if (!edit->priv->focus_in_handler_id)
        edit->priv->focus_in_handler_id =
                g_signal_connect (edit, "focus-in-event",
                                  G_CALLBACK (focus_in_cb),
                                  NULL);
}


void
_moo_edit_stop_file_watch (MooEdit        *edit)
{
    MooFileWatch *watch;

    watch = _moo_editor_get_file_watch (edit->priv->editor);
    g_return_if_fail (MOO_IS_FILE_WATCH (watch));

    if (edit->priv->file_monitor_id)
        moo_file_watch_cancel_monitor (watch, edit->priv->file_monitor_id);
    edit->priv->file_monitor_id = 0;

    if (edit->priv->file_watch_event_handler_id)
    {
        g_signal_handler_disconnect (watch, edit->priv->file_watch_event_handler_id);
        edit->priv->file_watch_event_handler_id = 0;
    }

    if (edit->priv->focus_in_handler_id)
    {
        g_signal_handler_disconnect (edit, edit->priv->focus_in_handler_id);
        edit->priv->focus_in_handler_id = 0;
    }
}


static gboolean focus_in_cb                 (MooEdit        *edit)
{
    check_file_status (edit, TRUE);
    return FALSE;
}


static void
check_file_status (MooEdit        *edit,
                   gboolean        in_focus_only)
{
    if (in_focus_only && !GTK_WIDGET_HAS_FOCUS (edit))
        return;

    g_return_if_fail (edit->priv->filename != NULL);
    g_return_if_fail (!(edit->priv->status & MOO_EDIT_CHANGED_ON_DISK));

    if (edit->priv->deleted_from_disk)
        file_deleted (edit);
    else if (edit->priv->modified_on_disk)
        file_modified_on_disk (edit);
}


static void     file_modified_on_disk       (MooEdit        *edit)
{
    g_return_if_fail (edit->priv->filename != NULL);
    edit->priv->modified_on_disk = FALSE;
    edit->priv->deleted_from_disk = FALSE;
    _moo_edit_stop_file_watch (edit);
    add_status (edit, MOO_EDIT_MODIFIED_ON_DISK);
}


static void     file_deleted                (MooEdit        *edit)
{
    g_return_if_fail (edit->priv->filename != NULL);
    edit->priv->modified_on_disk = FALSE;
    edit->priv->deleted_from_disk = FALSE;
    _moo_edit_stop_file_watch (edit);
    add_status (edit, MOO_EDIT_DELETED);
}


static void     add_status                  (MooEdit        *edit,
                                             MooEditStatus   s)
{
    edit->priv->status |= s;
    g_signal_emit_by_name (edit, "doc-status-changed", NULL);
}


static void remove_untitled             (gpointer    destroyed,
                                         MooEdit    *edit)
{
    gpointer n = g_hash_table_lookup (UNTITLED_NO, edit);

    if (n)
    {
        UNTITLED = g_slist_remove (UNTITLED, n);
        g_hash_table_remove (UNTITLED_NO, edit);
        if (!destroyed)
            g_object_weak_unref (G_OBJECT (edit),
                                 (GWeakNotify) remove_untitled,
                                 GINT_TO_POINTER (TRUE));
    }
}


static int  add_untitled                (MooEdit    *edit)
{
    int n;

    if (!(n = GPOINTER_TO_INT (g_hash_table_lookup (UNTITLED_NO, edit))))
    {
        for (n = 1; ; ++n)
        {
            if (!g_slist_find (UNTITLED, GINT_TO_POINTER (n)))
            {
                UNTITLED = g_slist_prepend (UNTITLED, GINT_TO_POINTER (n));
                break;
            }
        }

        g_hash_table_insert (UNTITLED_NO, edit, GINT_TO_POINTER (n));
        g_object_weak_ref (G_OBJECT (edit),
                           (GWeakNotify) remove_untitled,
                           GINT_TO_POINTER (TRUE));
    }

    return n;
}


void        _moo_edit_set_filename      (MooEdit    *edit,
                                         const char *file,
                                         const char *encoding)
{
    g_free (edit->priv->filename);
    g_free (edit->priv->basename);
    g_free (edit->priv->display_filename);
    g_free (edit->priv->display_basename);

    if (!UNTITLED_NO)
        UNTITLED_NO = g_hash_table_new (g_direct_hash, g_direct_equal);

    if (!file)
    {
        int n = add_untitled (edit);

        edit->priv->filename = NULL;
        edit->priv->basename = NULL;

        if (n == 1)
            edit->priv->display_filename = g_strdup ("Untitled");
        else
            edit->priv->display_filename = g_strdup_printf ("Untitled %d", n);

        edit->priv->display_basename = g_strdup (edit->priv->display_filename);
    }
    else
    {
        remove_untitled (NULL, edit);

        edit->priv->filename = g_strdup (file);
        edit->priv->basename = g_path_get_basename (file);

        edit->priv->display_filename =
                _moo_edit_filename_to_utf8 (file);
        edit->priv->display_basename =
                _moo_edit_filename_to_utf8 (edit->priv->basename);
    }

    g_free (edit->priv->encoding);
    edit->priv->encoding = g_strdup (encoding);

    _moo_edit_reload_vars (edit);
    _moo_edit_choose_lang (edit);
    _moo_edit_choose_indenter (edit);

    g_signal_emit_by_name (edit, "filename-changed", edit->priv->filename, NULL);
    g_signal_emit_by_name (edit, "doc-status-changed", NULL);
}


char       *_moo_edit_filename_to_utf8      (const char         *filename)
{
    GError *err = NULL;
    char *utf_filename;

    g_return_val_if_fail (filename != NULL, NULL);

    utf_filename = g_filename_to_utf8 (filename, -1, NULL, NULL, &err);

    if (!utf_filename)
    {
        g_critical ("%s: could not convert filename to utf8", G_STRLOC);

        if (err)
        {
            g_critical ("%s: %s", G_STRLOC, err->message);
            g_error_free (err);
            err = NULL;
        }

        utf_filename = g_strdup ("<Unknown>");
    }

    return utf_filename;
}
