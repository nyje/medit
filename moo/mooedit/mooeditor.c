/*
 *   mooedit/mooeditor.c
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
#include "mooedit/mooeditor.h"
#include "mooedit/mooeditdialogs.h"
#include "mooui/moouiobject.h"
#include "mooui/moomenuaction.h"
#include "mooutils/moocompat.h"
#include "mooutils/moomarshals.h"
#include "mooutils/moowin.h"
#include "mooutils/moosignal.h"
#include <string.h>


typedef struct {
    MooEditWindow *win;
    GSList        *docs;
} WindowInfo;

static WindowInfo   *window_info_new        (MooEditWindow  *win);
static void          window_info_free       (WindowInfo     *win);
static void          window_info_add        (WindowInfo     *win,
                                             MooEdit        *doc);
static void          window_info_remove     (WindowInfo     *win,
                                             MooEdit        *doc);
static MooEdit      *window_info_find       (WindowInfo     *win,
                                             const char     *filename);

static void          window_list_free       (MooEditor      *editor);
static void          window_list_delete     (MooEditor      *editor,
                                             WindowInfo     *win);
static WindowInfo   *window_list_add        (MooEditor      *editor,
                                             MooEditWindow  *win);
static WindowInfo   *window_list_find       (MooEditor      *editor,
                                             MooEditWindow  *win);
static WindowInfo   *window_list_find_doc   (MooEditor      *editor,
                                             MooEdit        *edit);
static WindowInfo   *window_list_find_filename (MooEditor   *editor,
                                             const char     *filename,
                                             MooEdit       **edit);

static GtkMenuItem  *create_recent_menu     (MooEditWindow  *window,
                                             MooAction      *action);

struct _MooEditorPrivate {
    GSList          *windows;
    GSList          *window_list;
    char            *app_name;
    MooEditLangMgr  *lang_mgr;
    MooUIXML        *ui_xml;
    MooEditFileMgr  *file_mgr;
    gboolean         open_single;
};


static void             moo_editor_finalize (GObject        *object);

static void             add_window          (MooEditor      *editor,
                                             MooEditWindow  *window);
static void             remove_window       (MooEditor      *editor,
                                             MooEditWindow  *window);
static gboolean         contains_window     (MooEditor      *editor,
                                             MooEditWindow  *window);
static MooEditWindow   *get_top_window      (MooEditor      *editor);
static gboolean         window_closed       (MooEditor      *editor,
                                             MooEditWindow  *window);
static void             document_new        (MooEditor      *editor,
                                             MooEdit        *edit,
                                             MooEditWindow  *window);
static void             document_closed     (MooEditor      *editor,
                                             MooEdit        *edit,
                                             MooEditWindow  *window);
static void             open_recent         (MooEditor      *editor,
                                             MooEditFileInfo *info,
                                             MooEditWindow  *window);
static void             file_opened_or_saved(MooEditor      *editor,
                                             MooEditFileInfo *info);


enum {
    ALL_WINDOWS_CLOSED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


/* MOO_TYPE_EDITOR */
G_DEFINE_TYPE (MooEditor, moo_editor, G_TYPE_OBJECT)


static void moo_editor_class_init (MooEditorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GObjectClass *edit_window_class;

    gobject_class->finalize = moo_editor_finalize;

    signals[ALL_WINDOWS_CLOSED] =
            moo_signal_new_cb ("all-windows-closed",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST,
                               NULL, NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    edit_window_class = g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    moo_ui_object_class_new_action (edit_window_class,
                                    "action-type::", MOO_TYPE_MENU_ACTION,
                                    "id", "OpenRecent",
                                    "create-menu-func", create_recent_menu,
                                    NULL);
    g_type_class_unref (edit_window_class);
}


static void     moo_editor_init        (MooEditor  *editor)
{
    editor->priv = g_new0 (MooEditorPrivate, 1);

    editor->priv->file_mgr = moo_edit_file_mgr_new ();
    g_signal_connect_swapped (editor->priv->file_mgr, "open-recent",
                              G_CALLBACK (open_recent), editor);
    editor->priv->open_single = TRUE;
    editor->priv->window_list = NULL;
}


static void moo_editor_finalize       (GObject      *object)
{
    MooEditor *editor = MOO_EDITOR (object);

    g_free (editor->priv->app_name);
    if (editor->priv->lang_mgr)
        g_object_unref (editor->priv->lang_mgr);
    if (editor->priv->ui_xml)
        g_object_unref (editor->priv->ui_xml);
    if (editor->priv->file_mgr)
        g_object_unref (editor->priv->file_mgr);
    window_list_free (editor);

    g_free (editor->priv);

    G_OBJECT_CLASS (moo_editor_parent_class)->finalize (object);
}


MooEditor       *moo_editor_new                 (void)
{
    return MOO_EDITOR (g_object_new (MOO_TYPE_EDITOR, NULL));
}


MooEditWindow   *moo_editor_new_window          (MooEditor  *editor)
{
    MooEditWindow *window;

    g_return_val_if_fail (MOO_IS_EDITOR (editor), NULL);

    window = MOO_EDIT_WINDOW (_moo_edit_window_new (editor));
    g_return_val_if_fail (window != NULL, NULL);

    add_window (editor, window);
    gtk_widget_show (GTK_WIDGET (window));

    return window;
}


static void              add_window     (MooEditor      *editor,
                                         MooEditWindow  *window)
{
    MooUIXML *ui_xml;
    WindowInfo *info;
    GSList *list, *l;

    g_return_if_fail (!contains_window (editor, window));
    g_return_if_fail (window != NULL);

    /* it may not be destroyed until all windows have been closed*/
    g_object_ref (editor);

    editor->priv->windows =
            g_slist_prepend (editor->priv->windows, window);
    _moo_edit_window_set_app_name (MOO_EDIT_WINDOW (window),
                                   editor->priv->app_name);

    info = window_list_add (editor, window);
    list = moo_edit_window_list_docs (window);

    for (l = list; l != NULL; l = l->next)
    {
        MooEdit *edit = l->data;
        window_info_add (info, edit);
    }

    g_slist_free (list);

    ui_xml = moo_editor_get_ui_xml (editor);
    moo_ui_object_set_ui_xml (MOO_UI_OBJECT (window), ui_xml);

    g_signal_connect_data (window, "close",
                           G_CALLBACK (window_closed), editor,
                           NULL,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

    g_signal_connect_swapped (window, "file-opened",
                              G_CALLBACK (file_opened_or_saved), editor);
    g_signal_connect_swapped (window, "file-saved",
                              G_CALLBACK (file_opened_or_saved), editor);
    g_signal_connect_swapped (window, "document-new",
                              G_CALLBACK (document_new), editor);
    g_signal_connect_swapped (window, "document-closed",
                              G_CALLBACK (document_closed), editor);
}


static void              remove_window  (MooEditor      *editor,
                                         MooEditWindow  *window)
{
    WindowInfo *info;

    g_return_if_fail (contains_window (editor, window));
    g_return_if_fail (window != NULL);

    editor->priv->windows = g_slist_remove (editor->priv->windows, window);

    info = window_list_find (editor, window);
    window_list_delete (editor, info);

    if (!editor->priv->windows)
        g_signal_emit (editor, signals[ALL_WINDOWS_CLOSED], 0, NULL);

    /* it was referenced in add_window */
    g_object_unref (editor);
}


static void             document_new        (MooEditor      *editor,
                                             MooEdit        *edit,
                                             MooEditWindow  *window)
{
    WindowInfo *info = window_list_find (editor, window);
    g_return_if_fail (info != NULL);
    window_info_add (info, edit);
}


static void             document_closed     (MooEditor      *editor,
                                             MooEdit        *edit,
                                             MooEditWindow  *window)
{
    WindowInfo *info = window_list_find_doc (editor, edit);
    g_return_if_fail (info != NULL);
    g_return_if_fail (info->win == window);
    window_info_remove (info, edit);
}


static MooEditWindow    *get_top_window (MooEditor      *editor)
{
    if (editor->priv->windows)
        return MOO_EDIT_WINDOW (moo_get_top_window (editor->priv->windows));
    else
        return NULL;
}


static gboolean         contains_window     (MooEditor      *editor,
                                             MooEditWindow  *window)
{
    return g_slist_find (editor->priv->windows, window) != NULL;
}


static gboolean         window_closed       (MooEditor      *editor,
                                             MooEditWindow  *window)
{
    g_return_val_if_fail (contains_window (editor, window), FALSE);
    remove_window (editor, window);
    return FALSE;
}


static void file_info_list_free (GSList *list)
{
    g_slist_foreach (list, (GFunc) moo_edit_file_info_free, NULL);
    g_slist_free (list);
}


gboolean         moo_editor_open                (MooEditor      *editor,
                                                 MooEditWindow  *window,
                                                 GtkWidget      *parent,
                                                 const char     *filename,
                                                 const char     *encoding)
{
    g_return_val_if_fail (MOO_IS_EDITOR (editor), FALSE);
    g_return_val_if_fail (window == NULL || MOO_IS_EDIT_WINDOW (window), FALSE);

    if (!filename)
    {
        GSList *files, *l;
        gboolean result;

        if (!parent && window)
            parent = GTK_WIDGET (window);

        files = moo_edit_file_mgr_open_dialog (editor->priv->file_mgr,
                                               parent);

        if (!files)
            return FALSE;

        for (l = files; l != NULL; l = l->next)
        {
            MooEditFileInfo *info = l->data;

            if (!info || !info->filename)
            {
                file_info_list_free (files);
                return FALSE;
            }

            result = moo_editor_open (editor, window, parent,
                                      info->filename, info->encoding);

            if (!result)
                break;
        }

        return result;
    }

    if (editor->priv->open_single)
    {
        MooEdit *edit;
        WindowInfo *info = window_list_find_filename (editor, filename, &edit);

        if (info)
        {
            gtk_window_present (GTK_WINDOW (info->win));
            moo_edit_window_set_active_doc (info->win, edit);
            return TRUE;
        }
    }

    if (!window)
        window = get_top_window (editor);

    if (!window)
        window = moo_editor_new_window (editor);

    g_return_val_if_fail (window != NULL, FALSE);

    gtk_window_present (GTK_WINDOW (window));

    return _moo_edit_window_open (window, filename, encoding);
}


MooEdit         *moo_editor_get_active_doc      (MooEditor      *editor)
{
    MooEditWindow *window;

    g_return_val_if_fail (MOO_IS_EDITOR (editor), NULL);

    window = get_top_window (editor);
    if (window)
        return moo_edit_window_get_active_doc (window);
    else
        return NULL;
}


MooEditWindow   *moo_editor_get_active_window   (MooEditor      *editor)
{
    g_return_val_if_fail (MOO_IS_EDITOR (editor), NULL);
    return get_top_window (editor);
}


gboolean         moo_editor_close_all           (MooEditor      *editor)
{
    GSList *list, *l;
    MooWindow *win;

    g_return_val_if_fail (MOO_IS_EDITOR (editor), FALSE);

    list = g_slist_copy (editor->priv->windows);

    for (l = list; l != NULL; l = l->next)
    {
        win = MOO_WINDOW (l->data);
        if (!(GTK_IN_DESTRUCTION & GTK_OBJECT_FLAGS (win)) &&
              !moo_window_close (win))
        {
            g_slist_free (list);
            return FALSE;
        }
    }

    g_slist_free (list);
    return TRUE;
}


void             moo_editor_set_app_name        (MooEditor      *editor,
                                                 const char     *name)
{
    GSList *l;

    g_return_if_fail (MOO_IS_EDITOR (editor));

    g_free (editor->priv->app_name);
    editor->priv->app_name = g_strdup (name);

    for (l = editor->priv->windows; l != NULL; l = l->next)
        _moo_edit_window_set_app_name (MOO_EDIT_WINDOW (l->data),
                                       name);
}


MooEditLangMgr  *moo_editor_get_lang_mgr        (MooEditor      *editor)
{
    g_return_val_if_fail (MOO_IS_EDITOR (editor), NULL);
    if (!editor->priv->lang_mgr)
        editor->priv->lang_mgr = moo_edit_lang_mgr_new ();
    return editor->priv->lang_mgr;
}


MooEditFileMgr  *moo_editor_get_file_mgr        (MooEditor      *editor)
{
    g_return_val_if_fail (MOO_IS_EDITOR (editor), NULL);
    return editor->priv->file_mgr;
}


MooUIXML        *moo_editor_get_ui_xml          (MooEditor      *editor)
{
    g_return_val_if_fail (MOO_IS_EDITOR (editor), NULL);

    if (!editor->priv->ui_xml)
        editor->priv->ui_xml = moo_ui_xml_new ();

    return editor->priv->ui_xml;
}


void             moo_editor_set_ui_xml          (MooEditor      *editor,
                                                 MooUIXML       *xml)
{
    GSList *l;

    g_return_if_fail (MOO_IS_EDITOR (editor));

    if (editor->priv->ui_xml == xml)
        return;

    if (editor->priv->ui_xml)
        g_object_unref (editor->priv->ui_xml);
    editor->priv->ui_xml = xml;
    if (xml)
        g_object_ref (editor->priv->ui_xml);

    for (l = editor->priv->windows; l != NULL; l = l->next)
        moo_ui_object_set_ui_xml (MOO_UI_OBJECT (l->data),
                                  editor->priv->ui_xml);
}


static WindowInfo   *window_info_new    (MooEditWindow  *win)
{
    WindowInfo *w = g_new0 (WindowInfo, 1);
    w->win = win;
    return w;
}

static void          window_info_free   (WindowInfo     *win)
{
    if (win)
    {
        g_slist_free (win->docs);
        g_free (win);
    }
}

static void          window_info_add        (WindowInfo     *win,
                                             MooEdit        *edit)
{
    g_return_if_fail (!g_slist_find (win->docs, edit));
    win->docs = g_slist_prepend (win->docs, edit);
}

static void          window_info_remove     (WindowInfo     *win,
                                             MooEdit        *edit)
{
    g_return_if_fail (g_slist_find (win->docs, edit));
    win->docs = g_slist_remove (win->docs, edit);
}


static int edit_and_file_cmp (MooEdit *edit, const char *filename)
{
    const char *edit_filename;
    g_return_val_if_fail (MOO_IS_EDIT (edit) && filename != NULL, TRUE);
    edit_filename = moo_edit_get_filename (edit);
    if (edit_filename)
        return strcmp (edit_filename, filename);
    else
        return TRUE;
}

static MooEdit      *window_info_find       (WindowInfo     *win,
                                             const char     *filename)
{
    GSList *l;
    g_return_val_if_fail (win != NULL && filename != NULL, NULL);
    l = g_slist_find_custom (win->docs, filename,
                             (GCompareFunc) edit_and_file_cmp);
    return l ? l->data : NULL;
}


static void          window_list_free   (MooEditor      *editor)
{
    g_slist_foreach (editor->priv->window_list,
                     (GFunc) window_info_free, NULL);
    g_slist_free (editor->priv->window_list);
    editor->priv->window_list = NULL;
}

static void          window_list_delete (MooEditor      *editor,
                                         WindowInfo     *win)
{
    g_return_if_fail (g_slist_find (editor->priv->window_list, win));
    window_info_free (win);
    editor->priv->window_list =
            g_slist_remove (editor->priv->window_list, win);
}

static WindowInfo   *window_list_add    (MooEditor      *editor,
                                         MooEditWindow  *win)
{
    WindowInfo *w = window_info_new (win);
    editor->priv->window_list =
            g_slist_prepend (editor->priv->window_list, w);
    return w;
}

static int window_cmp (WindowInfo *w, MooEditWindow *e)
{
    g_return_val_if_fail (w != NULL, TRUE);
    return !(w->win == e);
}

static WindowInfo   *window_list_find   (MooEditor      *editor,
                                         MooEditWindow  *win)
{
    GSList *l = g_slist_find_custom (editor->priv->window_list, win,
                                     (GCompareFunc) window_cmp);
    if (l)
        return l->data;
    else
        return NULL;
}

static int doc_cmp (WindowInfo *w, MooEdit *e)
{
    g_return_val_if_fail (w != NULL, 1);
    return !g_slist_find (w->docs, e);
}

static WindowInfo   *window_list_find_doc   (MooEditor      *editor,
                                             MooEdit        *edit)
{
    GSList *l = g_slist_find_custom (editor->priv->window_list, edit,
                                     (GCompareFunc) doc_cmp);
    if (l)
        return l->data;
    else
        return NULL;
}


struct FilenameAndDoc {
    const char *filename;
    MooEdit    *edit;
};

static int filename_and_doc_cmp (WindowInfo *w, struct FilenameAndDoc *data)
{
    g_return_val_if_fail (w != NULL, TRUE);
    data->edit = window_info_find (w, data->filename);
    return data->edit == NULL;
}

static WindowInfo   *window_list_find_filename (MooEditor   *editor,
                                                const char     *filename,
                                                MooEdit       **edit)
{
    struct FilenameAndDoc data = {filename, NULL};
    GSList *l = g_slist_find_custom (editor->priv->window_list, &data,
                                     (GCompareFunc) filename_and_doc_cmp);
    if (l)
    {
        g_assert (data.edit != NULL);
        if (edit) *edit = data.edit;
        return l->data;
    }
    else
    {
        return NULL;
    }
}


static GtkMenuItem  *create_recent_menu     (MooEditWindow  *window,
                                             G_GNUC_UNUSED MooAction *action)
{
    MooEditor *editor = _moo_edit_window_get_editor (window);
    g_return_val_if_fail (editor != NULL, NULL);
    return moo_edit_file_mgr_create_recent_files_menu (editor->priv->file_mgr, window);
}


static void             open_recent         (MooEditor       *editor,
                                             MooEditFileInfo *info,
                                             MooEditWindow   *window)
{
    WindowInfo *win_info;

    g_return_if_fail (MOO_IS_EDITOR (editor));
    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));
    g_return_if_fail (info != NULL);

    win_info = window_list_find (editor, window);
    g_return_if_fail (win_info != NULL);

    moo_editor_open (editor, window, GTK_WIDGET (window),
                     info->filename, info->encoding);
}


static void             file_opened_or_saved(MooEditor      *editor,
                                             MooEditFileInfo *info)
{
    g_return_if_fail (MOO_IS_EDITOR (editor));
    g_return_if_fail (info != NULL);
    moo_edit_file_mgr_add_recent (editor->priv->file_mgr, info);
}
