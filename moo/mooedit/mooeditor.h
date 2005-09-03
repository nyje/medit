/*
 *   mooeditor.h
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

#ifndef __MOO_EDITOR_H__
#define __MOO_EDITOR_H__

#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditlangmgr.h"
#include "mooedit/moorecentmgr.h"
#include "mooedit/moofileview/moofiltermgr.h"
#include "mooui/moouixml.h"

G_BEGIN_DECLS


#define MOO_TYPE_EDITOR                 (moo_editor_get_type ())
#define MOO_EDITOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOO_TYPE_EDITOR, MooEditor))
#define MOO_EDITOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_EDITOR, MooEditorClass))
#define MOO_IS_EDITOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOO_TYPE_EDITOR))
#define MOO_IS_EDITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_EDITOR))
#define MOO_EDITOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_EDITOR, MooEditorClass))

typedef struct _MooEditor               MooEditor;
typedef struct _MooEditorPrivate        MooEditorPrivate;
typedef struct _MooEditorClass          MooEditorClass;

struct _MooEditor
{
    GObject                 parent;
    MooEditorPrivate       *priv;
};

struct _MooEditorClass
{
    GObjectClass            parent_class;
};


GType            moo_editor_get_type        (void) G_GNUC_CONST;

MooEditor       *moo_editor_new             (void);

MooEditWindow   *moo_editor_new_window      (MooEditor      *editor);
MooEdit         *moo_editor_new_doc         (MooEditor      *editor,
                                             MooEditWindow  *window);

void             moo_editor_open            (MooEditor      *editor,
                                             MooEditWindow  *window,
                                             GtkWidget      *parent,
                                             GSList         *files); /* list of MooEditFileInfo* */
void             moo_editor_open_file       (MooEditor      *editor,
                                             MooEditWindow  *window,
                                             GtkWidget      *parent,
                                             const char     *filename,
                                             const char     *encoding);

MooEdit         *moo_editor_get_active_doc  (MooEditor      *editor);
MooEditWindow   *moo_editor_get_active_window (MooEditor    *editor);

void             moo_editor_set_active_window (MooEditor    *editor,
                                             MooEditWindow  *window);
void             moo_editor_set_active_doc  (MooEditor      *editor,
                                             MooEdit        *doc);

GSList          *moo_editor_list_windows    (MooEditor      *editor);

gboolean         moo_editor_close_window    (MooEditor      *editor,
                                             MooEditWindow  *window);
gboolean         moo_editor_close_doc       (MooEditor      *editor,
                                             MooEdit        *doc);
gboolean         moo_editor_close_docs      (MooEditor      *editor,
                                             GSList         *list);
gboolean         moo_editor_close_all       (MooEditor      *editor);

void             moo_editor_set_app_name    (MooEditor      *editor,
                                             const char     *name);

MooEditLangMgr  *moo_editor_get_lang_mgr    (MooEditor      *editor);
MooRecentMgr    *moo_editor_get_recent_mgr  (MooEditor      *editor);
MooFilterMgr    *moo_editor_get_filter_mgr  (MooEditor      *editor);

MooUIXML        *moo_editor_get_ui_xml      (MooEditor      *editor);
void             moo_editor_set_ui_xml      (MooEditor      *editor,
                                             MooUIXML       *xml);


#ifdef MOOEDIT_COMPILATION
MooEditWindow   *_moo_edit_window_new           (MooEditor      *editor);
// void             _moo_edit_window_close         (MooEditWindow  *window);

void             _moo_edit_window_insert_doc    (MooEditWindow  *window,
                                                 MooEdit        *doc,
                                                 int             position);
void             _moo_edit_window_remove_doc    (MooEditWindow  *window,
                                                 MooEdit        *doc);

void             _moo_edit_window_set_app_name  (MooEditWindow  *window,
                                                 const char     *name);
MooEditor       *_moo_edit_window_get_editor    (MooEditWindow  *window);

void             _moo_editor_reload             (MooEditor      *editor,
                                                 MooEdit        *doc);
gboolean         _moo_editor_save               (MooEditor      *editor,
                                                 MooEdit        *doc);
gboolean         _moo_editor_save_as            (MooEditor      *editor,
                                                 MooEdit        *doc,
                                                 const char     *filename,
                                                 const char     *encoding);
#endif /* MOOEDIT_COMPILATION */


G_END_DECLS

#endif /* __MOO_EDITOR_H__ */
