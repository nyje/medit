/*
 *   mooedit-private.h
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

#ifndef MOOEDIT_COMPILATION
#error "Do not include this file"
#endif

#ifndef MOO_EDIT_PRIVATE_H
#define MOO_EDIT_PRIVATE_H

#include "mooedit/moolinemark.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mootextview.h"
#include "mooutils/mdhistorymgr.h"
#include <sys/types.h>

G_BEGIN_DECLS

#if defined(__WIN32__) && !defined(__GNUC__)
typedef unsigned short mode_t;
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#endif


#define PROGRESS_TIMEOUT    100
#define PROGRESS_WIDTH      300
#define PROGRESS_HEIGHT     100


extern GSList *_moo_edit_instances;
void        _moo_edit_add_class_actions     (MooEdit        *edit);
void        _moo_edit_check_actions         (MooEdit        *edit);
void        _moo_edit_class_init_actions    (MooEditClass   *klass);

void        _moo_edit_do_popup              (MooEdit        *edit,
                                             GdkEventButton *event);

gboolean    _moo_edit_has_comments          (MooEdit        *edit,
                                             gboolean       *single_line,
                                             gboolean       *multi_line);

#define MOO_EDIT_GOTO_BOOKMARK_ACTION "GoToBookmark"
void        _moo_edit_delete_bookmarks      (MooEdit        *edit,
                                             gboolean        in_destroy);
void        _moo_edit_line_mark_moved       (MooEdit        *edit,
                                             MooLineMark    *mark);
void        _moo_edit_line_mark_deleted     (MooEdit        *edit,
                                             MooLineMark    *mark);
gboolean    _moo_edit_line_mark_clicked     (MooTextView    *view,
                                             int             line);
void        _moo_edit_update_bookmarks_style(MooEdit        *edit);

void        _moo_edit_history_item_set_encoding (MdHistoryItem  *item,
                                                 const char     *encoding);
void        _moo_edit_history_item_set_line     (MdHistoryItem  *item,
                                                 int             line);
const char *_moo_edit_history_item_get_encoding (MdHistoryItem  *item);
int         _moo_edit_history_item_get_line     (MdHistoryItem  *item);


/***********************************************************************/
/* Preferences
 */
enum {
    MOO_EDIT_SETTING_LANG,
    MOO_EDIT_SETTING_INDENT,
    MOO_EDIT_SETTING_STRIP,
    MOO_EDIT_SETTING_ADD_NEWLINE,
    MOO_EDIT_SETTING_WRAP_MODE,
    MOO_EDIT_SETTING_SHOW_LINE_NUMBERS,
    MOO_EDIT_SETTING_TAB_WIDTH,
    MOO_EDIT_SETTING_WORD_CHARS,
    MOO_EDIT_LAST_SETTING
};

extern guint *_moo_edit_settings;

void        _moo_edit_update_global_config      (void);
void        _moo_edit_init_config               (void);
void        _moo_edit_update_lang_config        (void);

void        _moo_edit_apply_prefs               (MooEdit        *edit);


/***********************************************************************/
/* File operations
 */

void         _moo_edit_set_filename             (MooEdit        *edit,
                                                 const char     *file,
                                                 const char     *encoding);
void         _moo_edit_set_encoding             (MooEdit        *edit,
                                                 const char     *encoding);
const char  *_moo_edit_get_default_encoding     (void);
void         _moo_edit_ensure_newline           (MooEdit        *edit);

void         _moo_edit_stop_file_watch          (MooEdit        *edit);

void         _moo_edit_set_status               (MooEdit        *edit,
                                                 MooEditStatus   status);

void         _moo_edit_set_state                (MooEdit        *edit,
                                                 MooEditState    state,
                                                 const char     *text,
                                                 GDestroyNotify  cancel,
                                                 gpointer        data);
void         _moo_edit_create_progress_dialog   (MooEdit        *edit);
void         _moo_edit_set_progress_text        (MooEdit        *edit,
                                                 const char     *text);

GdkPixbuf   *_moo_edit_get_icon                 (MooEdit        *edit,
                                                 GtkWidget      *widget,
                                                 GtkIconSize     size);

#define MOO_EDIT_IS_UNTITLED(edit) (!(edit)->priv->filename)

struct _MooEditFileInfo {
    char *filename;
    char *encoding;
};

typedef enum {
    MOO_EDIT_LINE_END_NONE,
    MOO_EDIT_LINE_END_UNIX,
    MOO_EDIT_LINE_END_WIN32,
    MOO_EDIT_LINE_END_MAC,
    MOO_EDIT_LINE_END_MIX
} MooEditLineEndType;

struct _MooEditPrivate {
    MooEditor *editor;

    gulong modified_changed_handler_id;
    guint apply_config_idle;

    /***********************************************************************/
    /* Document
     */
    char *filename;
    char *display_filename;
    char *display_basename;

    char *encoding;
    MooEditLineEndType line_end_type;
    MooEditStatus status;

    guint file_monitor_id;
    gulong focus_in_handler_id;
    gboolean modified_on_disk;
    gboolean deleted_from_disk;

    mode_t mode;
    guint mode_set : 1;

    /***********************************************************************/
    /* Progress dialog and stuff
     */
    MooEditState state;
    guint progress_timeout;
    GtkWidget *progress;
    GtkWidget *progressbar;
    char *progress_text;
    GDestroyNotify cancel_op;
    gpointer cancel_data;

    /***********************************************************************/
    /* Bookmarks
     */
    gboolean enable_bookmarks;
    GSList *bookmarks; /* sorted by line number */
    guint update_bookmarks_idle;

    /***********************************************************************/
    /* Actions
     */
    MooActionCollection *actions;
};


G_END_DECLS

#endif /* MOO_EDIT_PRIVATE_H */
