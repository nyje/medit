/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *   moofileview.c
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

#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#include <gtk/gtktoolbar.h>
#define GTK_DISABLE_DEPRECATED
#endif

#define MOO_FILE_SYSTEM_COMPILATION
#include "mooutils/moofileview/moofileview.h"
#include "mooutils/moofileview/moofileview-dialogs.h"
#include "mooutils/moofileview/moobookmarkmgr.h"
#include "mooutils/moofileview/moofilesystem.h"
#include "mooutils/moofileview/moofoldermodel.h"
#include "mooutils/moofileview/moofileentry.h"
#include "mooutils/moofileview/mooiconview.h"
#include "mooutils/moofileview/moofileview-private.h"
#include "mooutils/moofileview/moofileview-ui.h"
#include "mooutils/mooutils-gobject.h"
#include "mooutils/moofiltermgr.h"
#include "mooutils/mootoggleaction.h"
#include "mooutils/moouixml.h"
#include MOO_MARSHALS_H
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>


#ifndef __WIN32__
#define TYPEAHEAD_CASE_SENSITIVE_DEFAULT    FALSE
#define SORT_CASE_SENSITIVE_DEFAULT         MOO_FOLDER_MODEL_SORT_CASE_SENSITIVE_DEFAULT
#define COMPLETION_CASE_SENSITIVE_DEFAULT   TRUE
#else /* __WIN32__ */
#define TYPEAHEAD_CASE_SENSITIVE_DEFAULT    FALSE
#define SORT_CASE_SENSITIVE_DEFAULT         FALSE
#define COMPLETION_CASE_SENSITIVE_DEFAULT   FALSE
#endif /* __WIN32__ */


enum {
    TREEVIEW_PAGE = 0,
    ICONVIEW_PAGE = 1
};


enum {
    TARGET_URI_LIST,
    TARGET_TEXT
};

static GtkTargetEntry source_targets[] = {
    {(char*) "text/uri-list", 0, 0}
};

static GtkTargetEntry dest_targets[] = {
    {(char*) "text/uri-list", 0, 0}
};


typedef struct _History History;
typedef struct _Typeahead Typeahead;

struct _MooFileViewPrivate {
    GtkTreeModel    *model;
    GtkTreeModel    *filter_model;
    MooFolder       *current_dir;
    MooFileSystem   *file_system;

    guint            select_file_idle;

    GtkIconSize      icon_size;
    GtkNotebook     *notebook;
    MooFileViewType  view_type;

    GtkTreeView     *treeview;
    GtkTreeViewColumn *tree_name_column;
    MooIconView     *iconview;

    GtkMenu         *bookmarks_menu;
    MooBookmarkMgr  *bookmark_mgr;

    char            *home_dir;

    gboolean         show_hidden_files;
    gboolean         show_two_dots;
    History         *history;
    GString         *temp_visible;  /* temporary visible name, for interactive search */

    MooFilterMgr    *filter_mgr;
    GtkToggleButton *filter_button;
    MooCombo        *filter_combo;
    GtkEntry        *filter_entry;
    GtkFileFilter   *current_filter;
    gboolean         use_current_filter;

    GtkEntry        *entry;
    int              entry_state;   /* it can be one of three: nothing, typeahead, or completion,
                                       depending on text entered into the entry */
    Typeahead       *typeahead;
    gboolean         typeahead_case_sensitive;
    gboolean         sort_case_sensitive;
    gboolean         completion_case_sensitive;

    MooActionGroup  *actions;
    MooUIXML        *ui_xml;
    gboolean         has_selection;

    GtkTreeRowReference *drop_to;
    guint            drop_to_timeout;
    MooIconViewCell  drop_to_cell;
    int              drop_to_x;
    int              drop_to_y;
    gboolean         drop_to_blink_clear;
    guint            drop_to_n_blinks;
};


static void         moo_file_view_finalize      (GObject        *object);
static void         moo_file_view_set_property  (GObject        *object,
                                                 guint           prop_id,
                                                 const GValue   *value,
                                                 GParamSpec     *pspec);
static void         moo_file_view_get_property  (GObject        *object,
                                                 guint           prop_id,
                                                 GValue         *value,
                                                 GParamSpec     *pspec);

static void         moo_file_view_hide          (GtkWidget      *widget);
static gboolean     moo_file_view_key_press     (GtkWidget      *widget,
                                                 GdkEventKey    *event,
                                                 MooFileView    *fileview);
static gboolean     moo_file_view_popup_menu    (GtkWidget      *widget);

static void         moo_file_view_set_filter_mgr    (MooFileView    *fileview,
                                                     MooFilterMgr   *mgr);
static void         moo_file_view_set_bookmark_mgr  (MooFileView    *fileview,
                                                     MooBookmarkMgr *mgr);

static void         moo_file_view_set_current_dir (MooFileView  *fileview,
                                                 MooFolder      *folder);
static gboolean     moo_file_view_chdir_real    (MooFileView    *fileview,
                                                 const char     *dir,
                                                 GError        **error);

static void         moo_file_view_go_up     (MooFileView    *fileview);
static void         moo_file_view_go_home   (MooFileView    *fileview);
static void         moo_file_view_go_back   (MooFileView    *fileview);
static void         moo_file_view_go_forward(MooFileView    *fileview);
static void         toggle_show_hidden      (MooFileView    *fileview);

static void         bookmark_activate       (MooBookmarkMgr *mgr,
                                             MooBookmark    *bookmark,
                                             MooFileView    *activated,
                                             MooFileView    *fileview);

static void         history_init            (MooFileView    *fileview);
static void         history_free            (MooFileView    *fileview);
static void         history_add             (MooFileView    *fileview,
                                             const char     *dirname);
static void         history_clear           (MooFileView    *fileview);

static gboolean     filter_visible_func     (GtkTreeModel       *model,
                                             GtkTreeIter        *iter,
                                             MooFileView        *fileview);

static gboolean     moo_file_view_check_visible (MooFileView        *fileview,
                                                 MooFile            *file,
                                                 gboolean            ignore_hidden,
                                                 gboolean            ignore_two_dots);

static void icon_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
static void name_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
#ifdef USE_SIZE_AND_STUFF
static void date_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
static void size_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
#endif

static void         init_gui                (MooFileView    *fileview);
static void         focus_to_file_view      (MooFileView    *fileview);
static void         focus_to_filter_entry   (MooFileView    *fileview);
static GtkWidget   *create_toolbar          (MooFileView    *fileview);
static GtkWidget   *create_notebook         (MooFileView    *fileview);

static GtkWidget   *create_filter_combo     (MooFileView    *fileview);
static void         init_filter_combo       (MooFileView    *fileview);
static void         filter_button_toggled   (MooFileView    *fileview);
static void         filter_combo_changed    (MooFileView    *fileview);
static void         filter_entry_activate   (MooFileView    *fileview);
static void         fileview_set_filter     (MooFileView    *fileview,
                                             GtkFileFilter  *filter);
static void         fileview_set_use_filter (MooFileView    *fileview,
                                             gboolean        use,
                                             gboolean        block_signals);

static GtkWidget   *create_treeview         (MooFileView    *fileview);
static gboolean     tree_button_press       (GtkTreeView    *treeview,
                                             GdkEventButton *event,
                                             MooFileView    *fileview);

static GtkWidget   *create_iconview         (MooFileView    *fileview);
static gboolean     icon_button_press       (MooIconView    *iconview,
                                             GdkEventButton *event,
                                             MooFileView    *fileview);

static void         tree_path_activated     (MooFileView    *fileview,
                                             GtkTreePath    *filter_path);

static GtkWidget   *get_file_view_widget    (MooFileView    *fileview);
static void         file_view_move_selection(MooFileView    *fileview,
                                             GtkTreeIter    *filter_iter);

static void         path_entry_init         (MooFileView    *fileview);
static void         path_entry_deinit       (MooFileView    *fileview);
static void         path_entry_set_text     (MooFileView    *fileview,
                                             const char     *text);
static void         stop_path_entry         (MooFileView    *fileview,
                                             gboolean        focus_file_list);
static void         path_entry_delete_to_cursor (MooFileView *fileview);
static void         file_view_activate_filename (MooFileView *fileview,
                                             const char     *display_name);

static void select_display_name_in_idle     (MooFileView    *fileview,
                                             const char     *display_name);
/* returns path in the fileview->priv->filter_model */
static GtkTreePath *file_view_get_selected  (MooFileView    *fileview);
static GList       *file_view_get_selected_rows (MooFileView *fileview);

static void file_view_delete_selected       (MooFileView    *fileview);
static void file_view_create_folder         (MooFileView    *fileview);
static void file_view_properties_dialog     (MooFileView    *fileview);

static void         add_bookmark            (MooFileView    *fileview);
static void         edit_bookmarks          (MooFileView    *fileview);

/* Dnd */
static void         icon_drag_begin         (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             MooIconView    *iconview);
static void         icon_drag_data_get      (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             GtkSelectionData *data,
                                             guint           info,
                                             guint           time,
                                             MooIconView    *iconview);
static void         icon_drag_end           (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             MooIconView    *iconview);

static void         icon_drag_data_received (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             GtkSelectionData *data,
                                             guint           info,
                                             guint           time,
                                             MooIconView    *iconview);
static gboolean     icon_drag_drop          (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time,
                                             MooIconView    *iconview);
static void         icon_drag_leave         (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             guint           time,
                                             MooIconView    *iconview);
static gboolean     icon_drag_motion        (MooIconView    *iconview,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time,
                                             MooFileView    *fileview);
static gboolean     moo_file_view_drop      (MooFileView    *fileview,
                                             const char     *path,
                                             GtkWidget      *widget,
                                             GdkDragContext *context,
                                             guint           time);
static gboolean     moo_file_view_drop_data_received
                                            (MooFileView    *fileview,
                                             const char     *path,
                                             GtkWidget      *widget,
                                             GdkDragContext *context,
                                             GtkSelectionData *data,
                                             guint           info,
                                             guint           time);
static void         cancel_drop_open        (MooFileView    *fileview);


/* MOO_TYPE_FILE_VIEW */
G_DEFINE_TYPE (MooFileView, moo_file_view, GTK_TYPE_VBOX)

enum {
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_HOME_DIRECTORY,
    PROP_FILTER_MGR,
    PROP_BOOKMARK_MGR,
    PROP_SORT_CASE_SENSITIVE,
    PROP_TYPEAHEAD_CASE_SENSITIVE,
    PROP_COMPLETION_CASE_SENSITIVE,
    PROP_SHOW_HIDDEN_FILES,
    PROP_SHOW_PARENT_FOLDER,
    /* Aux properties */
    PROP_HAS_SELECTION,
    PROP_CAN_GO_BACK,
    PROP_CAN_GO_FORWARD
};

enum {
    CHDIR,
    ACTIVATE,
    POPULATE_POPUP,
    GO_UP,
    GO_BACK,
    GO_FORWARD,
    GO_HOME,
    FOCUS_TO_FILTER_ENTRY,
    FOCUS_TO_FILE_VIEW,
    TOGGLE_SHOW_HIDDEN,
    DELETE_TO_CURSOR,
    PROPERTIES_DIALOG,
    DELETE_SELECTED,
    CREATE_FOLDER,
    DROP,
    DROP_DATA_RECEIVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void moo_file_view_class_init (MooFileViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkBindingSet *binding_set;

    gobject_class->finalize = moo_file_view_finalize;
    gobject_class->set_property = moo_file_view_set_property;
    gobject_class->get_property = moo_file_view_get_property;

    widget_class->hide = moo_file_view_hide;
    widget_class->popup_menu = moo_file_view_popup_menu;

    klass->chdir = moo_file_view_chdir_real;
    klass->drop = moo_file_view_drop;
    klass->drop_data_received = moo_file_view_drop_data_received;

    g_object_class_install_property (gobject_class,
                                     PROP_HAS_SELECTION,
                                     g_param_spec_boolean ("has-selection",
                                             "has-selection", "has-selection",
                                             FALSE, G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_CAN_GO_BACK,
                                     g_param_spec_boolean ("can-go-back",
                                             "can-go-back", "can-go-back",
                                             FALSE, G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_CAN_GO_FORWARD,
                                     g_param_spec_boolean ("can-go-forward",
                                             "can-go-forward", "can-go-forward",
                                             FALSE, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     g_param_spec_string ("current-directory",
                                             "current-directory",
                                             "current-directory",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_HOME_DIRECTORY,
                                     g_param_spec_string ("home-directory",
                                             "home-directory",
                                             "home-directory",
#ifndef __WIN32__
                                             g_get_home_dir (),
#else
#warning "Do something here"
                                             NULL,
#endif
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_FILTER_MGR,
                                     g_param_spec_object ("filter-mgr",
                                             "filter-mgr",
                                             "filter-mgr",
                                             MOO_TYPE_FILTER_MGR,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_BOOKMARK_MGR,
                                     g_param_spec_object ("bookmark-mgr",
                                             "bookmark-mgr",
                                             "bookmark-mgr",
                                             MOO_TYPE_BOOKMARK_MGR,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TYPEAHEAD_CASE_SENSITIVE,
                                     g_param_spec_boolean ("typeahead-case-sensitive",
                                             "typeahead-case-sensitive",
                                             "typeahead-case-sensitive",
                                             TYPEAHEAD_CASE_SENSITIVE_DEFAULT,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SORT_CASE_SENSITIVE,
                                     g_param_spec_boolean ("sort-case-sensitive",
                                             "sort-case-sensitive",
                                             "sort-case-sensitive",
                                             SORT_CASE_SENSITIVE_DEFAULT,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_COMPLETION_CASE_SENSITIVE,
                                     g_param_spec_boolean ("completion-case-sensitive",
                                             "completion-case-sensitive",
                                             "completion-case-sensitive",
                                             COMPLETION_CASE_SENSITIVE_DEFAULT,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_HIDDEN_FILES,
                                     g_param_spec_boolean ("show-hidden-files",
                                             "show-hidden-files",
                                             "show-hidden-files",
                                             FALSE,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_PARENT_FOLDER,
                                     g_param_spec_boolean ("show-parent-folder",
                                             "show-parent-folder",
                                             "show-parent-folder",
                                             FALSE,
                                             G_PARAM_READWRITE));

    signals[CHDIR] =
            g_signal_new ("chdir",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                          G_STRUCT_OFFSET (MooFileViewClass, chdir),
                          NULL, NULL,
                          _moo_marshal_BOOLEAN__STRING_POINTER,
                          G_TYPE_BOOLEAN, 2,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          G_TYPE_POINTER);

    signals[ACTIVATE] =
            g_signal_new ("activate",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                          G_STRUCT_OFFSET (MooFileViewClass, activate),
                          NULL, NULL,
                          _moo_marshal_VOID__STRING,
                          G_TYPE_NONE, 1,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

    signals[POPULATE_POPUP] =
            g_signal_new ("populate-popup",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_FIRST,
                          G_STRUCT_OFFSET (MooFileViewClass, populate_popup),
                          NULL, NULL,
                          _moo_marshal_VOID__POINTER_OBJECT,
                          G_TYPE_NONE, 2,
                          G_TYPE_POINTER,
                          GTK_TYPE_MENU);

    signals[GO_UP] =
            moo_signal_new_cb ("go-up",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_up),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[GO_FORWARD] =
            moo_signal_new_cb ("go-forward",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_forward),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[GO_BACK] =
            moo_signal_new_cb ("go-back",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_back),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[GO_HOME] =
            moo_signal_new_cb ("go-home",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_home),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[FOCUS_TO_FILTER_ENTRY] =
            moo_signal_new_cb ("focus-to-filter-entry",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (focus_to_filter_entry),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[FOCUS_TO_FILE_VIEW] =
            moo_signal_new_cb ("focus-to-file-view",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (focus_to_file_view),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[TOGGLE_SHOW_HIDDEN] =
            moo_signal_new_cb ("toggle-show-hidden",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (toggle_show_hidden),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[DELETE_TO_CURSOR] =
            moo_signal_new_cb ("delete-to-cursor",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (path_entry_delete_to_cursor),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[DELETE_SELECTED] =
            moo_signal_new_cb ("delete-selected",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_delete_selected),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[CREATE_FOLDER] =
            moo_signal_new_cb ("create-folder",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_create_folder),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[PROPERTIES_DIALOG] =
             moo_signal_new_cb ("properties-dialog",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_properties_dialog),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[DROP] =
            g_signal_new ("drop",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooFileViewClass, drop),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOLEAN__STRING_OBJECT_OBJECT_UINT,
                          G_TYPE_BOOLEAN, 4,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          GTK_TYPE_WIDGET,
                          GDK_TYPE_DRAG_CONTEXT,
                          G_TYPE_UINT);

    signals[DROP_DATA_RECEIVED] =
            g_signal_new ("drop-data-received",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooFileViewClass, drop_data_received),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOLEAN__STRING_OBJECT_OBJECT_POINTER_UINT_UINT,
                          G_TYPE_BOOLEAN, 6,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          GTK_TYPE_WIDGET,
                          GDK_TYPE_DRAG_CONTEXT,
                          G_TYPE_POINTER,
                          G_TYPE_UINT,
                          G_TYPE_UINT);

    binding_set = gtk_binding_set_by_class (klass);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_u, GDK_CONTROL_MASK,
                                  "delete-to-cursor", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Return, GDK_MOD1_MASK,
                                  "properties-dialog", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Delete, GDK_MOD1_MASK,
                                  "delete-selected", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Up, GDK_MOD1_MASK,
                                  "go-up", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Up, GDK_MOD1_MASK,
                                  "go-up", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Left, GDK_MOD1_MASK,
                                  "go-back", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Left, GDK_MOD1_MASK,
                                  "go-back", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Right, GDK_MOD1_MASK,
                                  "go-forward", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Right, GDK_MOD1_MASK,
                                  "go-forward", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Home, GDK_MOD1_MASK,
                                  "go-home", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Home, GDK_MOD1_MASK,
                                  "go-home", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_f, GDK_MOD1_MASK | GDK_SHIFT_MASK,
                                  "focus-to-filter-entry", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_b, GDK_MOD1_MASK | GDK_SHIFT_MASK,
                                  "focus-to-file-view", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_h, GDK_MOD1_MASK | GDK_SHIFT_MASK,
                                  "toggle-show-hidden", 0);
}


static void moo_file_view_init      (MooFileView *fileview)
{
    fileview->priv = g_new0 (MooFileViewPrivate, 1);
    fileview->priv->show_hidden_files = FALSE;
    fileview->priv->view_type = MOO_FILE_VIEW_ICON;
    fileview->priv->use_current_filter = FALSE;
    fileview->priv->icon_size = GTK_ICON_SIZE_MENU;

    fileview->priv->typeahead_case_sensitive = TYPEAHEAD_CASE_SENSITIVE_DEFAULT;
    fileview->priv->sort_case_sensitive = SORT_CASE_SENSITIVE_DEFAULT;
    fileview->priv->completion_case_sensitive = COMPLETION_CASE_SENSITIVE_DEFAULT;

    history_init (fileview);

    fileview->priv->model = g_object_new (MOO_TYPE_FOLDER_MODEL,
                                          "sort-case-sensitive",
                                          fileview->priv->sort_case_sensitive,
                                          NULL);
    fileview->priv->filter_model =
            moo_folder_filter_new (MOO_FOLDER_MODEL (fileview->priv->model));
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (fileview->priv->filter_model),
                                            (GtkTreeModelFilterVisibleFunc) filter_visible_func,
                                            fileview, NULL);

    fileview->priv->file_system = moo_file_system_create ();

    init_gui (fileview);
    path_entry_init (fileview);
}


static void moo_file_view_finalize  (GObject      *object)
{
    MooFileView *fileview = MOO_FILE_VIEW (object);

    path_entry_deinit (fileview);

    if (fileview->priv->select_file_idle)
        g_source_remove (fileview->priv->select_file_idle);

    g_object_unref (fileview->priv->model);
    g_object_unref (fileview->priv->filter_model);
    history_free (fileview);

    if (fileview->priv->bookmark_mgr)
    {
        g_signal_handlers_disconnect_by_func (fileview->priv->bookmark_mgr,
                                              (gpointer) bookmark_activate,
                                              fileview);
        moo_bookmark_mgr_remove_user (fileview->priv->bookmark_mgr,
                                      fileview);
        g_object_unref (fileview->priv->bookmark_mgr);
    }

    if (fileview->priv->filter_mgr)
        g_object_unref (fileview->priv->filter_mgr);
    if (fileview->priv->current_filter)
        g_object_unref (fileview->priv->current_filter);

    if (fileview->priv->temp_visible)
        g_string_free (fileview->priv->temp_visible, TRUE);

    if (fileview->priv->current_dir)
        g_object_unref (fileview->priv->current_dir);
    g_object_unref (fileview->priv->file_system);

    g_free (fileview->priv->home_dir);

    g_object_unref (fileview->priv->actions);
    g_object_unref (fileview->priv->ui_xml);

    g_free (fileview->priv);
    fileview->priv = NULL;

    G_OBJECT_CLASS (moo_file_view_parent_class)->finalize (object);
}


static void
moo_file_view_hide (GtkWidget *widget)
{
    cancel_drop_open (MOO_FILE_VIEW (widget));
    GTK_WIDGET_CLASS(moo_file_view_parent_class)->hide (widget);
}


GtkWidget   *moo_file_view_new              (void)
{
    return GTK_WIDGET (g_object_new (MOO_TYPE_FILE_VIEW, NULL));
}


static void
moo_file_view_set_current_dir (MooFileView  *fileview,
                               MooFolder    *folder)
{
    GtkTreeIter filter_iter;
    char *path;

    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));
    g_return_if_fail (!folder || MOO_IS_FOLDER (folder));

    if (folder == fileview->priv->current_dir)
        return;

    cancel_drop_open (fileview);

    if (fileview->priv->temp_visible)
    {
        g_string_free (fileview->priv->temp_visible, TRUE);
        fileview->priv->temp_visible = NULL;
    }

    if (!folder)
    {
        if (fileview->priv->current_dir)
        {
            g_object_unref (fileview->priv->current_dir);
            fileview->priv->current_dir = NULL;
            moo_folder_model_set_folder (MOO_FOLDER_MODEL (fileview->priv->model),
                                         NULL);
        }

        history_clear (fileview);
        path_entry_set_text (fileview, "");
        g_object_set (MOO_FILE_ENTRY (fileview->priv->entry),
                      "current-dir", NULL, NULL);
        g_object_notify (G_OBJECT (fileview), "current-directory");
        return;
    }

    if (fileview->priv->current_dir)
        g_object_unref (fileview->priv->current_dir);

    fileview->priv->current_dir = g_object_ref (folder);
    moo_folder_model_set_folder (MOO_FOLDER_MODEL (fileview->priv->model),
                                 folder);

    if (gtk_tree_model_get_iter_first (fileview->priv->filter_model, &filter_iter))
        file_view_move_selection (fileview, &filter_iter);

    if (gtk_widget_is_focus (GTK_WIDGET (fileview->priv->entry)))
        focus_to_file_view (fileview);

    path = g_filename_display_name (moo_folder_get_path (folder));
    path_entry_set_text (fileview, path);
    g_free (path);

    g_object_set (MOO_FILE_ENTRY(fileview->priv->entry)->completion,
                  "current-dir", moo_folder_get_path (folder), NULL);

    history_add (fileview, moo_folder_get_path (folder));
    g_object_notify (G_OBJECT (fileview), "current-directory");
}


static gboolean
moo_file_view_chdir_real (MooFileView    *fileview,
                          const char     *new_dir,
                          GError        **error)
{
    char *real_new_dir;
    MooFolder *folder;

    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), FALSE);

    if (!new_dir)
    {
        moo_file_view_set_current_dir (fileview, NULL);
        return TRUE;
    }

    if (g_path_is_absolute (new_dir) || !fileview->priv->current_dir)
    {
        real_new_dir = g_strdup (new_dir);
    }
    else
    {
        real_new_dir = g_build_filename (moo_folder_get_path (fileview->priv->current_dir),
                                         new_dir, NULL);
    }

    folder = moo_file_system_get_folder (fileview->priv->file_system,
                                         real_new_dir,
                                         MOO_FILE_ALL_FLAGS,
                                         error);
    g_free (real_new_dir);

    if (!folder)
        return FALSE;

    moo_file_view_set_current_dir (fileview, folder);
    g_object_unref (folder);
    return TRUE;
}


static void
init_actions (MooFileView *fileview)
{
    MooAction *action;

    fileview->priv->actions = moo_action_group_new ("File Selector");
    fileview->priv->ui_xml = moo_ui_xml_new ();
    moo_ui_xml_add_ui_from_string (fileview->priv->ui_xml,
                                   MOO_FILE_VIEW_UI, -1);

    moo_action_group_add_action (fileview->priv->actions,
                                 "id", "GoUp",
                                 "label", "Parent Folder",
                                 "tooltip", "Parent Folder",
                                 "icon-stock-id", GTK_STOCK_GO_UP,
                                 "accel", "<alt>Up",
                                 "closure-object", fileview,
                                 "closure-signal", "go-up",
                                 NULL);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "id", "GoBack",
                                          "label", "Go Back",
                                          "tooltip", "Go Back",
                                          "icon-stock-id", GTK_STOCK_GO_BACK,
                                          "accel", "<alt>Left",
                                          "closure-object", fileview,
                                          "closure-signal", "go-back",
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "can-go-back", FALSE);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "id", "GoForward",
                                          "label", "Go Forward",
                                          "tooltip", "Go Forward",
                                          "icon-stock-id", GTK_STOCK_GO_FORWARD,
                                          "accel", "<alt>Right",
                                          "closure-object", fileview,
                                          "closure-signal", "go-forward",
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "can-go-forward", FALSE);

    moo_action_group_add_action (fileview->priv->actions,
                                 "id", "GoHome",
                                 "label", "Home Folder",
                                 "tooltip", "Home Folder",
                                 "icon-stock-id", GTK_STOCK_HOME,
                                 "accel", "<alt>Home",
                                 "closure-object", fileview,
                                 "closure-signal", "go-home",
                                 NULL);

    moo_action_group_add_action (fileview->priv->actions,
                                 "id", "NewFolder",
                                 "label", "New Folder...",
                                 "tooltip", "New Folder...",
                                 "icon-stock-id", GTK_STOCK_DIRECTORY,
                                 "closure-object", fileview,
                                 "closure-callback", file_view_create_folder,
                                 NULL);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "id", "Delete",
                                          "label", "Delete...",
                                          "tooltip", "Delete...",
                                          "icon-stock-id", GTK_STOCK_DELETE,
                                          "accel", "<alt>Delete",
                                          "closure-object", fileview,
                                          "closure-callback", file_view_delete_selected,
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "has-selection", FALSE);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "action-type::", MOO_TYPE_TOGGLE_ACTION,
                                          "id", "ShowHiddenFiles",
                                          "label", "Show Hidden Files",
                                          "tooltip", "Show Hidden Files",
                                          "accel", "<alt><shift>H",
                                          NULL);
    moo_sync_bool_property (action, "active", fileview, "show-hidden-files", FALSE);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "action-type::", MOO_TYPE_TOGGLE_ACTION,
                                          "id", "ShowParentFolder",
                                          "label", "Show Parent Folder",
                                          "tooltip", "Show Parent Folder",
                                          NULL);
    moo_sync_bool_property (action, "active", fileview, "show-parent-folder", FALSE);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "action-type::", MOO_TYPE_TOGGLE_ACTION,
                                          "id", "CaseSensitiveSort",
                                          "label", "Case Sensitive Sort",
                                          "tooltip", "Case Sensitive Sort",
                                          NULL);
    moo_sync_bool_property (action, "active", fileview, "sort-case-sensitive", FALSE);

    action = moo_action_group_add_action (fileview->priv->actions,
                                          "id", "Properties",
                                          "label", "Properties",
                                          "tooltip", "Properties",
                                          "icon-stock-id", GTK_STOCK_PROPERTIES,
                                          "accel", "<alt>Return",
                                          "closure-object", fileview,
                                          "closure-callback", file_view_properties_dialog,
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "has-selection", FALSE);

    moo_action_group_add_action (fileview->priv->actions,
                                 "id", "AddBookmark",
                                 "label", "Add Bookmark",
                                 "tooltip", "Add Bookmark",
                                 "icon-stock-id", GTK_STOCK_ADD,
                                 "closure-object", fileview,
                                 "closure-callback", add_bookmark,
                                 NULL);

    moo_action_group_add_action (fileview->priv->actions,
                                 "id", "EditBookmarks",
                                 "label", "Edit Bookmarks...",
                                 "tooltip", "Edit Bookmarks...",
                                 "icon-stock-id", GTK_STOCK_EDIT,
                                 "closure-object", fileview,
                                 "closure-callback", edit_bookmarks,
                                 NULL);
}


MooUIXML*
moo_file_view_get_ui_xml (MooFileView *fileview)
{
    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), NULL);
    return fileview->priv->ui_xml;
}


MooActionGroup*
moo_file_view_get_actions (MooFileView *fileview)
{
    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), NULL);
    return fileview->priv->actions;
}


static void
init_gui (MooFileView *fileview)
{
    GtkBox *box;
    GtkWidget *toolbar, *notebook, *filter_combo, *entry;

    init_actions (fileview);

    box = GTK_BOX (fileview);
    toolbar = create_toolbar (fileview);

    if (toolbar)
    {
        gtk_widget_show (toolbar);
        gtk_box_pack_start (box, toolbar, FALSE, FALSE, 0);
    }
    else
    {
        g_critical ("%s: oops", G_STRLOC);
    }

    entry = moo_file_entry_new ();
    g_object_set_data (G_OBJECT (entry), "moo-file-view", fileview);
    gtk_widget_show (entry);
    gtk_box_pack_start (box, entry, FALSE, FALSE, 0);
    fileview->priv->entry = GTK_ENTRY (entry);

    notebook = create_notebook (fileview);
    gtk_widget_show (notebook);
    gtk_box_pack_start (box, notebook, TRUE, TRUE, 0);
    fileview->priv->notebook = GTK_NOTEBOOK (notebook);

    filter_combo = create_filter_combo (fileview);
    gtk_widget_show (filter_combo);
    gtk_box_pack_start (box, filter_combo, FALSE, FALSE, 0);

    if (fileview->priv->view_type == MOO_FILE_VIEW_ICON)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                       ICONVIEW_PAGE);
    else
        gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                       TREEVIEW_PAGE);

    focus_to_file_view (fileview);
}


static void         focus_to_file_view      (MooFileView    *fileview)
{
    gtk_widget_grab_focus (get_file_view_widget (fileview));
}


static void         focus_to_filter_entry   (MooFileView    *fileview)
{
    gtk_widget_grab_focus (GTK_WIDGET(fileview->priv->filter_entry));
}


void        moo_file_view_set_view_type     (MooFileView    *fileview,
                                             MooFileViewType type)
{
    MooIconView *iconview;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkTreePath *path;

    g_return_if_fail (type == MOO_FILE_VIEW_ICON ||
            type == MOO_FILE_VIEW_LIST);

    if (fileview->priv->view_type == type)
        return;

    iconview = fileview->priv->iconview;
    treeview = fileview->priv->treeview;
    selection = gtk_tree_view_get_selection (treeview);

    fileview->priv->view_type = type;

    if (type == MOO_FILE_VIEW_LIST)
    {
        path = moo_icon_view_get_cursor (iconview);
        gtk_tree_selection_unselect_all (selection);
        if (path)
        {
            gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
            gtk_tree_path_free (path);
        }
    }
    else
    {
        gtk_tree_view_get_cursor (treeview, &path, NULL);
        moo_icon_view_unselect_all (iconview);
        if (path)
        {
            moo_icon_view_set_cursor (iconview, path, FALSE);
            gtk_tree_path_free (path);
        }
    }

    if (type == MOO_FILE_VIEW_ICON)
        gtk_notebook_set_current_page (fileview->priv->notebook,
                                       ICONVIEW_PAGE);
    else
        gtk_notebook_set_current_page (fileview->priv->notebook,
                                       TREEVIEW_PAGE);
}


static GtkWidget*
create_toolbar (MooFileView    *fileview)
{
    GtkToolbar *toolbar;

    toolbar = moo_ui_xml_create_widget (fileview->priv->ui_xml,
                                        MOO_UI_TOOLBAR,
                                        "MooFileView/Toolbar",
                                        fileview->priv->actions,
                                        NULL);
    g_return_val_if_fail (toolbar != NULL, NULL);

    gtk_toolbar_set_tooltips (toolbar, TRUE);
    gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_MENU);

    fileview->toolbar = GTK_WIDGET (toolbar);
    return fileview->toolbar;
}


static GtkWidget   *create_notebook     (MooFileView    *fileview)
{
    GtkWidget *notebook, *swin, *treeview, *iconview;

    notebook = gtk_notebook_new ();
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (swin);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), swin, NULL);
    treeview = create_treeview (fileview);
    gtk_widget_show (treeview);
    gtk_container_add (GTK_CONTAINER (swin), treeview);
    fileview->priv->treeview = GTK_TREE_VIEW (treeview);
    g_signal_connect (treeview, "key-press-event",
                      G_CALLBACK (moo_file_view_key_press),
                      fileview);
    /* gtk+ #313719 */
    g_signal_connect_swapped (treeview, "popup-menu",
                              G_CALLBACK (moo_file_view_popup_menu),
                              fileview);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (swin);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_ALWAYS,
                                    GTK_POLICY_AUTOMATIC);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), swin, NULL);
    iconview = create_iconview (fileview);
    gtk_widget_show (iconview);
    gtk_container_add (GTK_CONTAINER (swin), iconview);
    fileview->priv->iconview = MOO_ICON_VIEW (iconview);

    return notebook;
}


static GtkWidget   *create_filter_combo (G_GNUC_UNUSED MooFileView *fileview)
{
    GtkWidget *hbox, *button, *combo;

    hbox = gtk_hbox_new (FALSE, 0);

    button = gtk_toggle_button_new_with_label ("Filter");
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

    combo = moo_combo_new ();
    gtk_widget_show (combo);
    gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);

    fileview->priv->filter_button = GTK_TOGGLE_BUTTON (button);
    fileview->priv->filter_combo = MOO_COMBO (combo);
    fileview->priv->filter_entry = GTK_ENTRY (MOO_COMBO(combo)->entry);

    g_signal_connect_swapped (button, "toggled",
                              G_CALLBACK (filter_button_toggled),
                              fileview);
    g_signal_connect_data (combo, "changed",
                           G_CALLBACK (filter_combo_changed),
                           fileview, NULL,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
    g_signal_connect_swapped (MOO_COMBO(combo)->entry, "activate",
                              G_CALLBACK (filter_entry_activate),
                              fileview);

    return hbox;
}


static GtkWidget   *create_treeview     (MooFileView    *fileview)
{
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    GtkCellRenderer *cell;

    treeview = gtk_tree_view_new_with_model (fileview->priv->filter_model);

    g_signal_connect_swapped (treeview, "row-activated",
                              G_CALLBACK (tree_path_activated), fileview);
    g_signal_connect (treeview, "button-press-event",
                      G_CALLBACK (tree_button_press), fileview);

#if 0
    gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview),
                                            GDK_CONTROL_MASK,
                                            source_targets,
                                            G_N_ELEMENTS (source_targets),
                                            GDK_ACTION_COPY | GDK_ACTION_MOVE |
                                                    GDK_ACTION_LINK);
#endif

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, "Name");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    fileview->priv->tree_name_column = column;

    cell = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) icon_data_func,
                                             fileview, NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) name_data_func,
                                             fileview, NULL);

#ifdef USE_SIZE_AND_STUFF
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, "Size");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) size_data_func,
                                             fileview, NULL);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, "Date");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) date_data_func,
                                             fileview, NULL);
#endif

    return treeview;
}


static void
icon_selection_changed (MooIconView *icon_view,
                        MooFileView *file_view)
{
    if (file_view->priv->view_type == MOO_FILE_VIEW_ICON)
    {
        gboolean has_selection;
        GtkTreePath *path;

        path = moo_icon_view_get_selected_path (icon_view);
        has_selection = path != NULL;

        if (file_view->priv->has_selection != has_selection)
        {
            file_view->priv->has_selection = has_selection;
            g_object_notify (G_OBJECT (file_view), "has-selection");
        }

        gtk_tree_path_free (path);
    }
}


static GtkWidget*
create_iconview (MooFileView *fileview)
{
    GtkWidget *iconview;
    GtkCellRenderer *cell;
//     GtkTargetList *targets;

    iconview = moo_icon_view_new_with_model (fileview->priv->filter_model);
    moo_icon_view_set_selection_mode (MOO_ICON_VIEW (iconview),
                                      GTK_SELECTION_MULTIPLE);

    g_signal_connect (iconview, "key-press-event",
                      G_CALLBACK (moo_file_view_key_press),
                      fileview);

    moo_icon_view_set_cell_data_func (MOO_ICON_VIEW (iconview),
                                      MOO_ICON_VIEW_CELL_PIXBUF,
                                      (MooIconCellDataFunc) icon_data_func,
                                      fileview, NULL);
    moo_icon_view_set_cell_data_func (MOO_ICON_VIEW (iconview),
                                      MOO_ICON_VIEW_CELL_TEXT,
                                      (MooIconCellDataFunc) name_data_func,
                                      fileview, NULL);

    cell = moo_icon_view_get_cell (MOO_ICON_VIEW (iconview),
                                   MOO_ICON_VIEW_CELL_TEXT);
    g_object_set (cell, "xalign", 0.0, NULL);
    cell = moo_icon_view_get_cell (MOO_ICON_VIEW (iconview),
                                   MOO_ICON_VIEW_CELL_PIXBUF);
    g_object_set (cell, "xpad", 1, "ypad", 1, NULL);

    g_signal_connect_swapped (iconview, "row-activated",
                              G_CALLBACK (tree_path_activated), fileview);
    g_signal_connect (iconview, "button-press-event",
                      G_CALLBACK (icon_button_press), fileview);
    g_signal_connect (iconview, "selection-changed",
                      G_CALLBACK (icon_selection_changed), fileview);

    moo_icon_view_enable_drag_source (MOO_ICON_VIEW (iconview),
                                      GDK_BUTTON1_MASK,
                                      source_targets,
                                      G_N_ELEMENTS (source_targets),
                                      GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    moo_icon_view_enable_drag_dest (MOO_ICON_VIEW (iconview),
                                    dest_targets,
                                    G_N_ELEMENTS (dest_targets),
                                    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    g_signal_connect_swapped (iconview, "drag-begin",
                              G_CALLBACK (icon_drag_begin),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-data-get",
                              G_CALLBACK (icon_drag_data_get),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-end",
                              G_CALLBACK (icon_drag_end),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-data-received",
                              G_CALLBACK (icon_drag_data_received),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-drop",
                              G_CALLBACK (icon_drag_drop),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-leave",
                              G_CALLBACK (icon_drag_leave),
                              fileview);
    g_signal_connect_after (iconview, "drag-motion",
                            G_CALLBACK (icon_drag_motion),
                            fileview);

    return iconview;
}


static gboolean moo_file_view_check_visible (MooFileView    *fileview,
                                             MooFile        *file,
                                             gboolean        ignore_hidden,
                                             gboolean        ignore_two_dots)
{
    if (!file)
        return FALSE;

    if (!strcmp (moo_file_name (file), ".."))
    {
        if (!ignore_two_dots)
            return fileview->priv->show_two_dots;
        else
            return FALSE;
    }

    if (!ignore_hidden && MOO_FILE_IS_HIDDEN (file))
        return fileview->priv->show_hidden_files;

    if (MOO_FILE_IS_DIR (file))
        return TRUE;

    if (fileview->priv->current_filter && fileview->priv->use_current_filter)
    {
        GtkFileFilterInfo filter_info;
        GtkFileFilter *filter = fileview->priv->current_filter;

        filter_info.contains = gtk_file_filter_get_needed (filter);
        filter_info.filename = moo_file_name (file);
        filter_info.uri = NULL;
        filter_info.display_name = moo_file_display_name (file);
        filter_info.mime_type = moo_file_get_mime_type (file);

        return gtk_file_filter_filter (fileview->priv->current_filter,
                                       &filter_info);
    }

    return TRUE;
}


static gboolean     filter_visible_func (GtkTreeModel   *model,
                                         GtkTreeIter    *iter,
                                         MooFileView    *fileview)
{
    MooFile *file;
    gboolean visible = TRUE;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file && fileview->priv->temp_visible &&
        !strncmp (moo_file_name (file),
                  fileview->priv->temp_visible->str,
                  fileview->priv->temp_visible->len))
    {
        visible = moo_file_view_check_visible (fileview, file, TRUE, TRUE);
    }
    else
    {
        visible = moo_file_view_check_visible (fileview, file, FALSE, FALSE);
    }

    moo_file_unref (file);
    return visible;
}


static void icon_data_func  (G_GNUC_UNUSED GObject *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview)
{
    MooFile *file = NULL;
    GdkPixbuf *pixbuf = NULL;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file)
        pixbuf = moo_file_get_icon (file, GTK_WIDGET (fileview),
                                    fileview->priv->icon_size);

    g_object_set (cell, "pixbuf", pixbuf, NULL);
    moo_file_unref (file);
}


static void name_data_func  (G_GNUC_UNUSED GObject *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             G_GNUC_UNUSED MooFileView *fileview)
{
    MooFile *file = NULL;
    const char *name = NULL;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file)
        name = moo_file_display_name (file);

    g_object_set (cell, "text", name, NULL);
    moo_file_unref (file);
}


#ifdef USE_SIZE_AND_STUFF
static void date_data_func  (G_GNUC_UNUSED GObject            *column_or_iconview,
                             G_GNUC_UNUSED GtkCellRenderer    *cell,
                             G_GNUC_UNUSED GtkTreeModel       *model,
                             G_GNUC_UNUSED GtkTreeIter        *iter,
                             G_GNUC_UNUSED MooFileView *fileview)
{
}

static void size_data_func  (G_GNUC_UNUSED GObject            *column_or_iconview,
                             G_GNUC_UNUSED GtkCellRenderer    *cell,
                             G_GNUC_UNUSED GtkTreeModel       *model,
                             G_GNUC_UNUSED GtkTreeIter        *iter,
                             G_GNUC_UNUSED MooFileView *fileview)
{
}
#endif


gboolean    moo_file_view_chdir             (MooFileView    *fileview,
                                             const char     *dir,
                                             GError        **error)
{
    gboolean result;

    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), FALSE);

    g_signal_emit (fileview, signals[CHDIR], 0, dir, error, &result);

    return result;
}


/* TODO */
static void         moo_file_view_go_up     (MooFileView    *fileview)
{
    MooFolder *parent = NULL;

    if (fileview->priv->entry_state)
        stop_path_entry (fileview, TRUE);

    if (fileview->priv->current_dir)
        parent = moo_folder_get_parent (fileview->priv->current_dir,
                                        MOO_FILE_HAS_ICON);
    if (!parent)
        parent = moo_file_system_get_root_folder (fileview->priv->file_system,
            MOO_FILE_HAS_ICON);

    g_return_if_fail (parent != NULL);

    if (parent != fileview->priv->current_dir)
    {
        char *name = g_path_get_basename (
                moo_folder_get_path (fileview->priv->current_dir));
        moo_file_view_set_current_dir (fileview, parent);
        moo_file_view_select_name (fileview, name);
        g_free (name);
    }

    g_object_unref (parent);
}


static void         moo_file_view_go_home   (MooFileView    *fileview)
{
    GError *error = NULL;

    if (fileview->priv->entry_state)
        stop_path_entry (fileview, TRUE);

    if (!moo_file_view_chdir (fileview, fileview->priv->home_dir, &error))
    {
        g_warning ("%s: could not go up", G_STRLOC);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }
}


static void         tree_path_activated     (MooFileView    *fileview,
                                             GtkTreePath    *filter_path)
{
    MooFile *file = NULL;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, filter_path))
    {
        g_return_if_reached ();
    }

    gtk_tree_model_get (fileview->priv->filter_model,
                        &iter, COLUMN_FILE, &file, -1);
    g_return_if_fail (file != NULL);

    if (!strcmp (moo_file_name (file), ".."))
    {
        g_signal_emit_by_name (fileview, "go-up");
    }
    else if (MOO_FILE_IS_DIR (file))
    {
        GError *error = NULL;

        if (!moo_file_view_chdir (fileview, moo_file_name (file), &error))
        {
            g_warning ("%s: could not go into '%s'",
                       G_STRLOC, moo_file_name (file));

            if (error)
            {
                g_warning ("%s: %s", G_STRLOC, error->message);
                g_error_free (error);
            }
        }
    }
    else
    {
        char *path;
        g_assert (fileview->priv->current_dir != NULL);
        path = g_build_filename (moo_folder_get_path (fileview->priv->current_dir),
                                 moo_file_name (file), NULL);
        g_signal_emit (fileview, signals[ACTIVATE], 0, path);
        g_free (path);
    }

    moo_file_unref (file);
}


struct _History {
    GSList *back;
    GSList *fwd;
    char *current;
    guint block;
};


static void
history_init (MooFileView *fileview)
{
    fileview->priv->history = g_new0 (History, 1);
}


static void
history_clear (MooFileView *fileview)
{
    g_slist_foreach (fileview->priv->history->back, (GFunc) g_free, NULL);
    g_slist_foreach (fileview->priv->history->fwd, (GFunc) g_free, NULL);
    g_slist_free (fileview->priv->history->back);
    g_slist_free (fileview->priv->history->fwd);
    fileview->priv->history->back = NULL;
    fileview->priv->history->fwd = NULL;
    g_free (fileview->priv->history->current);
    fileview->priv->history->current = NULL;
}


static void
history_free (MooFileView *fileview)
{
    history_clear (fileview);
    g_free (fileview->priv->history);
    fileview->priv->history = NULL;
}


static const char*
history_get (MooFileView *fileview,
             int          where)
{
    History *hist = fileview->priv->history;

    g_assert (where == 1 || where == -1);

    if (where == 1)
    {
        if (!hist->fwd)
            return NULL;
        else
            return hist->fwd->data;
    }
    else
    {
        if (!hist->back)
            return NULL;
        else
            return hist->back->data;
    }
}


static void
history_go (MooFileView *fileview,
            int          where)
{
    History *hist = fileview->priv->history;
    char *tmp;
    GSList **pop_from, **push_to;
    gboolean could_go_forward, could_go_back;
    gboolean can_go_forward, can_go_back;

    g_assert (where == 1 || where == -1);
    g_assert (hist->current != NULL);

    could_go_forward = hist->fwd != NULL;
    could_go_back = hist->back != NULL;

    if (where == 1)
    {
        if (!hist->fwd)
            return;
        pop_from = &hist->fwd;
        push_to = &hist->back;
    }
    else
    {
        if (!hist->back)
            return;
        pop_from = &hist->back;
        push_to = &hist->fwd;
    }

    tmp = (*pop_from)->data;
    *pop_from = g_slist_delete_link (*pop_from, *pop_from);
    *push_to = g_slist_prepend (*push_to, hist->current);
    hist->current = tmp;

    can_go_forward = hist->fwd != NULL;
    can_go_back = hist->back != NULL;

    g_object_freeze_notify (G_OBJECT (fileview));
    if (could_go_forward != can_go_forward)
        g_object_notify (G_OBJECT (fileview), "can-go-forward");
    if (could_go_back != can_go_back)
        g_object_notify (G_OBJECT (fileview), "can-go-back");
    g_object_thaw_notify (G_OBJECT (fileview));
}


static void
history_add (MooFileView    *fileview,
             const char     *dirname)
{
    History *hist = fileview->priv->history;
    gboolean could_go_forward = FALSE, could_go_back, can_go_back;

    g_return_if_fail (dirname != NULL);

    if (hist->block)
        return;

    if (hist->current && !strcmp (dirname, hist->current))
        return;

    if (hist->fwd)
    {
        could_go_forward = TRUE;
        g_slist_foreach (hist->fwd, (GFunc) g_free, NULL);
        g_slist_free (hist->fwd);
        hist->fwd = NULL;
    }

    could_go_back = hist->back != NULL;

    if (hist->current)
    {
        hist->back = g_slist_prepend (hist->back, hist->current);
        hist->current = g_strdup (dirname);
    }
    else
    {
        hist->current = g_strdup (dirname);
    }

    can_go_back = hist->back != NULL;

    g_object_freeze_notify (G_OBJECT (fileview));
    if (could_go_back != can_go_back)
        g_object_notify (G_OBJECT (fileview), "can-go-back");
    if (could_go_forward)
        g_object_notify (G_OBJECT (fileview), "can-go-forward");
    g_object_thaw_notify (G_OBJECT (fileview));
}


static void
moo_file_view_go (MooFileView *fileview,
                  int          where)
{
    const char *dir;
    GError *error = NULL;

    g_assert (where == 1 || where == -1);

    if (fileview->priv->entry_state)
        stop_path_entry (fileview, TRUE);

    dir = history_get (fileview, where);

    if (dir)
    {
        fileview->priv->history->block++;

        if (!moo_file_view_chdir (fileview, dir, &error))
        {
            g_warning ("%s: could not go into '%s'",
                       G_STRLOC, dir);

            if (error)
            {
                g_warning ("%s: %s", G_STRLOC, error->message);
                g_error_free (error);
            }

//             history_remove (fileview, dir);
        }
        else
        {
            history_go (fileview, where);
        }

        fileview->priv->history->block--;
    }
}


static void
moo_file_view_go_back (MooFileView *fileview)
{
    moo_file_view_go (fileview, -1);
}


static void
moo_file_view_go_forward (MooFileView *fileview)
{
    moo_file_view_go (fileview, 1);
}


static void
moo_file_view_set_property (GObject        *object,
                            guint           prop_id,
                            const GValue   *value,
                            GParamSpec     *pspec)
{
    MooFileView *fileview = MOO_FILE_VIEW (object);

    switch (prop_id)
    {
        case PROP_CURRENT_DIRECTORY:
            moo_file_view_chdir (fileview, g_value_get_string (value), NULL);
            break;

        case PROP_HOME_DIRECTORY:
            g_free (fileview->priv->home_dir);
            fileview->priv->home_dir = g_strdup (g_value_get_string (value));
            g_object_notify (object, "home-directory");
            break;

        case PROP_FILTER_MGR:
            moo_file_view_set_filter_mgr (fileview, g_value_get_object (value));
            break;

        case PROP_BOOKMARK_MGR:
            moo_file_view_set_bookmark_mgr (fileview, g_value_get_object (value));
            break;

        case PROP_SORT_CASE_SENSITIVE:
            moo_file_view_set_sort_case_sensitive (fileview, g_value_get_boolean (value));
            break;

        case PROP_TYPEAHEAD_CASE_SENSITIVE:
            moo_file_view_set_typeahead_case_sensitive (fileview, g_value_get_boolean (value));
            break;

        case PROP_COMPLETION_CASE_SENSITIVE:
            g_object_set (MOO_FILE_ENTRY(fileview->priv->entry)->completion,
                          "case-sensitive", g_value_get_boolean (value), NULL);
            g_object_notify (object, "completion-case-sensitive");
            break;

        case PROP_SHOW_HIDDEN_FILES:
            moo_file_view_set_show_hidden (fileview, g_value_get_boolean (value));
            break;

        case PROP_SHOW_PARENT_FOLDER:
            moo_file_view_set_show_parent (fileview, g_value_get_boolean (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


static void
moo_file_view_get_property (GObject        *object,
                            guint           prop_id,
                            GValue         *value,
                            GParamSpec     *pspec)
{
    MooFileView *fileview = MOO_FILE_VIEW (object);
    gboolean val;
    GList *list;

    switch (prop_id)
    {
        case PROP_CURRENT_DIRECTORY:
            if (fileview->priv->current_dir)
                g_value_set_string (value, moo_folder_get_path (fileview->priv->current_dir));
            else
                g_value_set_string (value, NULL);
            break;

        case PROP_HOME_DIRECTORY:
            g_value_set_string (value, fileview->priv->home_dir);
            break;

        case PROP_FILTER_MGR:
            g_value_set_object (value, fileview->priv->filter_mgr);
            break;

        case PROP_BOOKMARK_MGR:
            g_value_set_object (value, fileview->priv->bookmark_mgr);
            break;

        case PROP_SORT_CASE_SENSITIVE:
            g_value_set_boolean (value, fileview->priv->sort_case_sensitive);
            break;

        case PROP_TYPEAHEAD_CASE_SENSITIVE:
            g_value_set_boolean (value, fileview->priv->typeahead_case_sensitive);
            break;

        case PROP_COMPLETION_CASE_SENSITIVE:
            g_object_get (MOO_FILE_ENTRY(fileview->priv->entry)->completion,
                          "case-sensitive", &val, NULL);
            g_value_set_boolean (value, val);
            break;

        case PROP_HAS_SELECTION:
            if (fileview->priv->current_dir)
            {
                list = file_view_get_selected_rows (fileview);
                g_value_set_boolean (value, list != NULL);
                g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
                g_list_free (list);
            }
            else
            {
                g_value_set_boolean (value, FALSE);
            }
            break;

        case PROP_CAN_GO_BACK:
            g_value_set_boolean (value, history_get (fileview, -1) != NULL);
            break;

        case PROP_CAN_GO_FORWARD:
            g_value_set_boolean (value, history_get (fileview, 1) != NULL);
            break;

        case PROP_SHOW_HIDDEN_FILES:
            g_value_set_boolean (value, fileview->priv->show_hidden_files);
            break;

        case PROP_SHOW_PARENT_FOLDER:
            g_value_set_boolean (value, fileview->priv->show_two_dots);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


void        moo_file_view_set_sort_case_sensitive       (MooFileView    *fileview,
                                                         gboolean        case_sensitive)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    if (case_sensitive != fileview->priv->sort_case_sensitive)
    {
        fileview->priv->sort_case_sensitive = case_sensitive;
        g_object_set (fileview->priv->model,
                      "sort-case-sensitive",
                      case_sensitive, NULL);
        g_object_notify (G_OBJECT (fileview), "sort-case-sensitive");
    }
}


/*****************************************************************************/
/* Filters
 */

static void         moo_file_view_set_filter_mgr    (MooFileView    *fileview,
                                                     MooFilterMgr   *mgr)
{
    if (!mgr)
    {
        mgr = moo_filter_mgr_new ();
        moo_file_view_set_filter_mgr (fileview, mgr);
        g_object_unref (mgr);
        g_object_notify (G_OBJECT (fileview), "filter-mgr");
        return;
    }

    if (mgr == fileview->priv->filter_mgr)
        return;

    if (fileview->priv->filter_mgr)
        g_object_unref (fileview->priv->filter_mgr);
    fileview->priv->filter_mgr = g_object_ref (mgr);

    init_filter_combo (fileview);
    g_object_notify (G_OBJECT (fileview), "filter-mgr");
}


static void         block_filter_signals    (MooFileView    *fileview)
{
    g_signal_handlers_block_by_func (fileview->priv->filter_combo,
                                     (gpointer) filter_button_toggled,
                                     fileview);
    g_signal_handlers_block_by_func (fileview->priv->filter_combo,
                                     (gpointer) filter_combo_changed,
                                     fileview);
    g_signal_handlers_block_by_func (fileview->priv->filter_combo,
                                     (gpointer) filter_entry_activate,
                                     fileview);
}

static void         unblock_filter_signals  (MooFileView    *fileview)
{
    g_signal_handlers_unblock_by_func (fileview->priv->filter_combo,
                                       (gpointer) filter_button_toggled,
                                       fileview);
    g_signal_handlers_unblock_by_func (fileview->priv->filter_combo,
                                       (gpointer) filter_combo_changed,
                                       fileview);
    g_signal_handlers_unblock_by_func (fileview->priv->filter_combo,
                                       (gpointer) filter_entry_activate,
                                       fileview);
}


static void         init_filter_combo       (MooFileView    *fileview)
{
    MooFilterMgr *mgr = fileview->priv->filter_mgr;
    GtkFileFilter *filter;

    block_filter_signals (fileview);

    moo_filter_mgr_init_filter_combo (mgr, fileview->priv->filter_combo,
                                      "MooFileView");
    if (fileview->priv->current_filter)
        g_object_unref (fileview->priv->current_filter);
    fileview->priv->current_filter = NULL;
    fileview->priv->use_current_filter = FALSE;
    gtk_toggle_button_set_active (fileview->priv->filter_button, FALSE);
    gtk_entry_set_text (fileview->priv->filter_entry, "");

    unblock_filter_signals (fileview);

    filter = moo_filter_mgr_get_last_filter (mgr, "MooFileView");

    if (filter)
        fileview_set_filter (fileview, filter);
}


static void         filter_button_toggled   (MooFileView    *fileview)
{
    gboolean active =
            gtk_toggle_button_get_active (fileview->priv->filter_button);

    if (active == fileview->priv->use_current_filter)
        return;

    /* TODO check entry content */
    fileview_set_use_filter (fileview, active, TRUE);
    focus_to_file_view (fileview);
}


static void         filter_combo_changed    (MooFileView    *fileview)
{
    GtkTreeIter iter;
    GtkFileFilter *filter;
    MooFilterMgr *mgr = fileview->priv->filter_mgr;
    MooCombo *combo = fileview->priv->filter_combo;

    if (!moo_combo_get_active_iter (combo, &iter))
        return;

    filter = moo_filter_mgr_get_filter (mgr, &iter, "MooFileView");
    g_return_if_fail (filter != NULL);
    moo_filter_mgr_set_last_filter (mgr, &iter, "MooFileView");

    fileview_set_filter (fileview, filter);
    focus_to_file_view (fileview);
}


static void         filter_entry_activate   (MooFileView    *fileview)
{
    const char *text;
    GtkFileFilter *filter;
    MooFilterMgr *mgr = fileview->priv->filter_mgr;

    text = gtk_entry_get_text (fileview->priv->filter_entry);

    if (text && text[0])
        filter = moo_filter_mgr_new_user_filter (mgr, text, "MooFileView");
    else
        filter = NULL;

    fileview_set_filter (fileview, filter);
    focus_to_file_view (fileview);
}


static void         fileview_set_filter     (MooFileView    *fileview,
                                             GtkFileFilter  *filter)
{
    GtkFileFilter *null_filter;

    if (filter && filter == fileview->priv->current_filter)
    {
        fileview_set_use_filter (fileview, TRUE, TRUE);
        return;
    }

    null_filter = moo_filter_mgr_get_null_filter (fileview->priv->filter_mgr);
    g_return_if_fail (null_filter != NULL);

    if (filter == null_filter)
        return fileview_set_filter (fileview, NULL);

    if (filter && (gtk_file_filter_get_needed (filter) & GTK_FILE_FILTER_URI))
    {
        g_warning ("%s: The filter set wants uri, but i do not know "
                   "how to work with uri's. Ignoring", G_STRLOC);
        return;
    }

    block_filter_signals (fileview);

    if (fileview->priv->current_filter)
        g_object_unref (fileview->priv->current_filter);
    fileview->priv->current_filter = filter;

    if (filter)
    {
        const char *name;
        gtk_object_sink (GTK_OBJECT (g_object_ref (filter)));
        name = gtk_file_filter_get_name (filter);
        gtk_entry_set_text (fileview->priv->filter_entry, name);
        fileview_set_use_filter (fileview, TRUE, FALSE);
    }
    else
    {
        gtk_entry_set_text (fileview->priv->filter_entry, "");
        fileview_set_use_filter (fileview, FALSE, FALSE);
    }

    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
            (fileview->priv->filter_model));

    unblock_filter_signals (fileview);
}


static void         fileview_set_use_filter (MooFileView    *fileview,
                                             gboolean        use,
                                             gboolean        block_signals)
{
    if (block_signals)
        block_filter_signals (fileview);

    gtk_toggle_button_set_active (fileview->priv->filter_button, use);

    if (fileview->priv->use_current_filter != use)
    {
        if (fileview->priv->current_filter)
        {
            fileview->priv->use_current_filter = use;
            if (block_signals)
                gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                        (fileview->priv->filter_model));
        }
        else
        {
            fileview->priv->use_current_filter = FALSE;
            gtk_toggle_button_set_active (fileview->priv->filter_button, FALSE);
        }
    }

    if (block_signals)
        unblock_filter_signals (fileview);
}


/*****************************************************************************/
/* File manager functionality
 */

/* path in the filter model; result must be unrefed */
static MooFile *
file_view_get_file_at_path (MooFileView *fileview,
                            GtkTreePath *path)
{
    GtkTreeIter iter;
    MooFile *file = NULL;

    g_return_val_if_fail (path != NULL, NULL);

    gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, path);
    gtk_tree_model_get (fileview->priv->filter_model, &iter, COLUMN_FILE, &file, -1);

    return file;
}


/* returned files must be unrefed and list freed */
static GList   *file_view_get_selected_files    (MooFileView    *fileview)
{
    GList *paths, *files, *l;
    paths = file_view_get_selected_rows (fileview);

    for (files = NULL, l = paths; l != NULL; l = l->next)
    {
        GtkTreeIter iter;
        MooFile *file = NULL;
        gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, l->data);
        gtk_tree_model_get (fileview->priv->filter_model, &iter,
                            COLUMN_FILE, &file, -1);
        if (file)
            files = g_list_prepend (files, file);
    }

    g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (paths);
    return files;
}


static void     file_view_delete_selected       (MooFileView    *fileview)
{
    GError *error = NULL;
    GList *files, *l;
    gboolean one, folder;
    char *message, *path;
    GtkWidget *parent, *dialog;
    GtkWindow *parent_window;
    int response;

    if (!fileview->priv->current_dir)
        return;

    files = file_view_get_selected_files (fileview);
    if (!files)
        return;

    one = (files->next == NULL);
    if (one)
        folder = MOO_FILE_IS_DIR (files->data);

    if (one)
    {
        if (folder)
            message = g_strdup_printf ("Delete folder %s and all its content?",
                                       moo_file_display_name (files->data));
        else
            message = g_strdup_printf ("Delete file %s?",
                                       moo_file_display_name (files->data));
    }
    else
    {
        message = g_strdup ("Delete selected files?");
    }

    parent = gtk_widget_get_toplevel (GTK_WIDGET (fileview));
    parent_window = GTK_IS_WINDOW (parent) ? GTK_WINDOW (parent) : NULL;

    dialog = gtk_message_dialog_new (parent_window,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_NONE,
                                     "%s", message);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_DELETE, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    if (response == GTK_RESPONSE_OK)
    {
        for (l = files; l != NULL; l = l->next)
        {
            path = g_build_filename (moo_folder_get_path (fileview->priv->current_dir),
                                     moo_file_name (l->data), NULL);

            if (!moo_file_system_delete_file (fileview->priv->file_system, path, TRUE, &error))
            {
                dialog = gtk_message_dialog_new (parent_window,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_NONE,
                                                 "Could not delete %s '%s'",
                                                 MOO_FILE_IS_DIR (l->data) ? "folder" : "file",
                                                 path);
                if (error)
                {
                    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                            "%s", error->message);
                    g_error_free (error);
                }

                gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
                gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
            }

            g_free (path);
        }
    }

    g_list_foreach (files, (GFunc) moo_file_unref, NULL);
    g_list_free (files);
}


static void file_view_create_folder         (MooFileView    *fileview)
{
    GError *error = NULL;
    char *path, *name;

    if (!fileview->priv->current_dir)
        return;

    name = moo_create_folder_dialog (GTK_WIDGET (fileview),
                                     fileview->priv->current_dir);

    if (!name || !name[0])
    {
        g_free (name);
        return;
    }

    path = moo_file_system_make_path (fileview->priv->file_system,
                                      moo_folder_get_path (fileview->priv->current_dir),
                                      name, &error);

    if (!path)
    {
        g_message ("%s: could not make path for '%s'", G_STRLOC, name);

        if (error)
        {
            g_message ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }

        goto out;
    }

    if (!moo_file_system_create_folder (fileview->priv->file_system, path, &error))
    {
        g_message ("%s: could not create folder '%s'", G_STRLOC, name);

        if (error)
        {
            g_message ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }

        goto out;
    }

    select_display_name_in_idle (fileview, name);

out:
    g_free (path);
    g_free (name);
}


/* XXX */
static void
file_view_properties_dialog (MooFileView *fileview)
{
    GtkWidget *dialog;
    GList *files;

    if (!fileview->priv->current_dir)
        return;

    files = file_view_get_selected_files (fileview);

    if (!files)
    {
        g_print ("no files\n");
        return;
    }

    if (files->next)
    {
        g_print ("many files\n");
        g_list_foreach (files, (GFunc) moo_file_unref, NULL);
        g_list_free (files);
        return;
    }

    dialog = g_object_get_data (G_OBJECT (fileview),
                                "moo-file-view-properties-dialog");

    if (!dialog)
    {
        dialog = moo_file_props_dialog_new (GTK_WIDGET (fileview));
        gtk_object_sink (GTK_OBJECT (g_object_ref (dialog)));
        g_object_set_data_full (G_OBJECT (fileview),
                                "moo-file-view-properties-dialog",
                                dialog, g_object_unref);
        g_signal_connect (dialog, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }

    moo_file_props_dialog_set_file (MOO_FILE_PROPS_DIALOG (dialog),
                                    files->data, fileview->priv->current_dir);
    gtk_window_present (GTK_WINDOW (dialog));

    g_list_foreach (files, (GFunc) moo_file_unref, NULL);
    g_list_free (files);
}


/*****************************************************************************/
/* Popup menu
 */

static gboolean really_destroy_menu (GtkWidget *menu)
{
    g_object_unref (menu);
    return FALSE;
}

static void destroy_menu    (GtkWidget *menu)
{
    g_idle_add ((GSourceFunc) really_destroy_menu, menu);
}

/* TODO */
static void menu_position_func  (G_GNUC_UNUSED GtkMenu *menu,
                                 gint       *x,
                                 gint       *y,
                                 gboolean   *push_in,
                                 gpointer    user_data)
{
    GdkWindow *window;

    struct {
        MooFileView *fileview;
        GList *rows;
    } *data = user_data;

    window = GTK_WIDGET(data->fileview)->window;
    gdk_window_get_origin (window, x, y);

    *push_in = TRUE;
}

static void         do_popup                (MooFileView    *fileview,
                                             GdkEventButton *event,
                                             GList          *selected)
{
    GtkWidget *menu;
    GList *files = NULL, *l;
    struct {
        MooFileView *fileview;
        GList *rows;
    } position_data = {fileview, selected};

    for (l = selected; l != NULL; l = l->next)
    {
        GtkTreeIter iter;
        MooFile *file = NULL;
        gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, l->data);
        gtk_tree_model_get (fileview->priv->filter_model, &iter,
                            COLUMN_FILE, &file, -1);
        if (file)
            files = g_list_prepend (files, file);
    }

    menu = moo_ui_xml_create_widget (fileview->priv->ui_xml,
                                     MOO_UI_MENU, "MooFileView/Menubar",
                                     fileview->priv->actions,
                                     NULL);
    gtk_object_sink (GTK_OBJECT (g_object_ref (menu)));
    g_signal_connect (menu, "deactivate", G_CALLBACK (destroy_menu), NULL);

    g_signal_emit (fileview, signals[POPULATE_POPUP], 0, files, menu);

    if (event)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        event->button, event->time);
    else
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                        menu_position_func,
                        &position_data, 0,
                        gtk_get_current_event_time ());

    g_list_foreach (files, (GFunc) moo_file_unref, NULL);
    g_list_free (files);
}


static gboolean     moo_file_view_popup_menu    (GtkWidget      *widget)
{
    GList *selected;
    MooFileView *fileview = MOO_FILE_VIEW (widget);

    selected = file_view_get_selected_rows (fileview);
    do_popup (fileview, NULL, selected);
    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);

    return TRUE;
}


static gboolean     tree_button_press       (GtkTreeView    *treeview,
                                             GdkEventButton *event,
                                             MooFileView    *fileview)
{
    GtkTreeSelection *selection;
    GtkTreePath *filter_path = NULL;
    GList *selected;

    if (event->button != 3)
        return FALSE;

    selection = gtk_tree_view_get_selection (treeview);

    if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y,
                                       &filter_path, NULL, NULL, NULL))
    {
        if (!gtk_tree_selection_path_is_selected (selection, filter_path))
        {
            gtk_tree_selection_unselect_all (selection);
            gtk_tree_view_set_cursor (treeview, filter_path,
                                      NULL, FALSE);
        }
    }
    else
    {
        gtk_tree_selection_unselect_all (selection);
    }

    selected = gtk_tree_selection_get_selected_rows (selection, NULL);
    do_popup (fileview, event, selected);
    gtk_tree_path_free (filter_path);
    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);

    return TRUE;
}


static gboolean     icon_button_press       (MooIconView    *iconview,
                                             GdkEventButton *event,
                                             MooFileView    *fileview)
{
    GtkTreePath *filter_path = NULL;
    GList *selected;

    if (event->button != 3)
        return FALSE;

    if (moo_icon_view_get_path_at_pos (iconview, event->x, event->y,
                                       &filter_path, NULL, NULL, NULL))
    {
        if (!moo_icon_view_path_is_selected (iconview, filter_path))
        {
            moo_icon_view_unselect_all (iconview);
            moo_icon_view_set_cursor (iconview, filter_path, FALSE);
        }
    }
    else
    {
        moo_icon_view_unselect_all (iconview);
    }

    selected = moo_icon_view_get_selected_rows (iconview);
    do_popup (fileview, event, selected);
    gtk_tree_path_free (filter_path);
    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);

    return TRUE;
}


void
moo_file_view_set_show_hidden (MooFileView    *fileview,
                               gboolean        show)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    show = show ? TRUE : FALSE;

    if (fileview->priv->show_hidden_files != show)
    {
        fileview->priv->show_hidden_files = show;
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (
                fileview->priv->filter_model));
        g_object_notify (G_OBJECT (fileview), "show-hidden-files");
    }
}


void
moo_file_view_set_show_parent (MooFileView    *fileview,
                               gboolean        show)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    show = show ? TRUE : FALSE;

    if (fileview->priv->show_two_dots != show)
    {
        fileview->priv->show_two_dots = show;
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (
                fileview->priv->filter_model));
        g_object_notify (G_OBJECT (fileview), "show-parent-folder");
    }
}


static void         toggle_show_hidden      (MooFileView    *fileview)
{
    moo_file_view_set_show_hidden (fileview,
                                   !fileview->priv->show_hidden_files);
}


static GtkWidget   *get_file_view_widget        (MooFileView    *fileview)
{
    if (fileview->priv->view_type == MOO_FILE_VIEW_ICON)
        return GTK_WIDGET(fileview->priv->iconview);
    else
        return GTK_WIDGET(fileview->priv->treeview);
}


/*****************************************************************************/
/* Bookmarks menu
 */

static void
moo_file_view_set_bookmark_mgr  (MooFileView    *fileview,
                                 MooBookmarkMgr *mgr)
{
    if (!mgr)
    {
        mgr = moo_bookmark_mgr_new ();
        moo_file_view_set_bookmark_mgr (fileview, mgr);
        g_object_unref (mgr);
        return;
    }

    if (fileview->priv->bookmark_mgr == mgr)
        return;

    if (fileview->priv->bookmark_mgr)
    {
        g_signal_handlers_disconnect_by_func (fileview->priv->bookmark_mgr,
                                              (gpointer) bookmark_activate,
                                              fileview);
        moo_bookmark_mgr_remove_user (fileview->priv->bookmark_mgr,
                                      fileview);
        g_object_unref (fileview->priv->bookmark_mgr);
    }

    fileview->priv->bookmark_mgr = g_object_ref (mgr);
    moo_bookmark_mgr_add_user (fileview->priv->bookmark_mgr, fileview,
                               fileview->priv->actions, fileview->priv->ui_xml,
                               "MooFileView/Toolbar/BookmarksMenu/Bookmarks");
    g_signal_connect (fileview->priv->bookmark_mgr,
                      "activate", G_CALLBACK (bookmark_activate),
                      fileview);

    g_object_set_data (G_OBJECT (fileview),
                       "moo-file-view-bookmarks-editor",
                       NULL);
    g_object_notify (G_OBJECT (fileview), "bookmark-mgr");
}


static void
add_bookmark (MooFileView *fileview)
{
    const char *path;
    char *display_path;
    MooBookmark *bookmark;

    g_return_if_fail (fileview->priv->current_dir != NULL);

    path = moo_folder_get_path (fileview->priv->current_dir);
    display_path = g_filename_display_name (path);
    bookmark = moo_bookmark_new (display_path, path,
                                 GTK_STOCK_DIRECTORY);

    moo_bookmark_mgr_add (fileview->priv->bookmark_mgr,
                          bookmark);

    moo_bookmark_free (bookmark);
    g_free (display_path);
}


static void
edit_bookmarks (MooFileView *fileview)
{
    GtkWidget *dialog, *parent;

    dialog = g_object_get_data (G_OBJECT (fileview),
                                "moo-file-view-bookmarks-editor");

    if (!dialog)
    {
        dialog = moo_bookmark_mgr_get_editor (fileview->priv->bookmark_mgr);
        gtk_object_sink (GTK_OBJECT (g_object_ref (dialog)));
        g_object_set_data_full (G_OBJECT (fileview),
                                "moo-file-view-bookmarks-editor",
                                dialog, g_object_unref);
        g_signal_connect (dialog, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }

    parent = gtk_widget_get_toplevel (GTK_WIDGET (fileview));
    if (GTK_IS_WINDOW (parent) && !GTK_WIDGET_VISIBLE (dialog))
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

    gtk_window_present (GTK_WINDOW (dialog));
}


static void
bookmark_activate (G_GNUC_UNUSED MooBookmarkMgr *mgr,
                   MooBookmark    *bookmark,
                   MooFileView    *activated,
                   MooFileView    *fileview)
{
    if (activated != fileview)
        return;

    moo_file_view_chdir (fileview, bookmark->path, NULL);
}


/****************************************************************************/
/* Auxiliary stuff
 */

static void
file_view_move_selection (MooFileView    *fileview,
                          GtkTreeIter    *filter_iter)
{
    GtkTreePath *path = NULL;

    if (filter_iter)
    {
        path = gtk_tree_model_get_path (fileview->priv->filter_model,
                                        filter_iter);
        g_return_if_fail (path != NULL);
    }

    if (fileview->priv->view_type == MOO_FILE_VIEW_LIST)
    {
        GtkTreeSelection *selection;
        GtkTreeView *treeview;

        treeview = fileview->priv->treeview;
        selection = gtk_tree_view_get_selection (treeview);

        gtk_tree_selection_unselect_all (selection);

        if (path)
        {
            gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
            gtk_tree_view_scroll_to_cell (treeview, path, NULL,
                                          FALSE, 0, 0);
        }
    }
    else
    {
        moo_icon_view_unselect_all (fileview->priv->iconview);

        if (path)
        {
            moo_icon_view_set_cursor (fileview->priv->iconview, path, FALSE);
            moo_icon_view_scroll_to_cell (fileview->priv->iconview, path);
        }
    }

    gtk_tree_path_free (path);
}


static void file_view_select_iter           (MooFileView    *fileview,
                                             GtkTreeIter    *iter)
{
    if (iter)
    {
        GtkTreeIter filter_iter;
        GtkTreePath *filter_path, *path;

        path = gtk_tree_model_get_path (fileview->priv->model, iter);
        g_return_if_fail (path != NULL);

        filter_path = gtk_tree_model_filter_convert_child_path_to_path (
                GTK_TREE_MODEL_FILTER (fileview->priv->filter_model), path);

        if (!filter_path)
        {
            gtk_tree_path_free (path);
            if (gtk_tree_model_get_iter_first (fileview->priv->filter_model,
                &filter_iter))
                file_view_move_selection (fileview, &filter_iter);
        }
        else
        {
            gtk_tree_model_get_iter (fileview->priv->filter_model,
                                     &filter_iter, filter_path);
            file_view_move_selection (fileview, &filter_iter);
            gtk_tree_path_free (path);
            gtk_tree_path_free (filter_path);
        }
    }
    else
    {
        file_view_move_selection (fileview, NULL);
    }
}


void        moo_file_view_select_name       (MooFileView    *fileview,
                                             const char     *name)
{
    GtkTreeIter iter;

    if (name && moo_folder_model_get_iter_by_name (
        MOO_FOLDER_MODEL (fileview->priv->model), name, &iter))
    {
        file_view_select_iter (fileview, &iter);
    }
    else
    {
        file_view_select_iter (fileview, NULL);
    }
}


void        moo_file_view_select_display_name   (MooFileView    *fileview,
                                                 const char     *name)
{
    GtkTreeIter iter;

    if (name && moo_folder_model_get_iter_by_display_name (
        MOO_FOLDER_MODEL (fileview->priv->model), name, &iter))
    {
        file_view_select_iter (fileview, &iter);
    }
    else
    {
        file_view_select_iter (fileview, NULL);
    }
}


static gboolean do_select_display_name      (MooFileView        *fileview)
{
    GtkTreeIter iter;
    const char *name;

    fileview->priv->select_file_idle = 0;

    name = g_object_get_data (G_OBJECT (fileview),
                              "moo-file-view-select-display-name");
    g_return_val_if_fail (name != NULL, FALSE);

    if (moo_folder_model_get_iter_by_display_name (
        MOO_FOLDER_MODEL (fileview->priv->model), name, &iter))
    {
        file_view_select_iter (fileview, &iter);
    }

    return FALSE;
}

/* TODO: this */
static void select_display_name_in_idle     (MooFileView        *fileview,
                                             const char         *display_name)
{
    GtkTreeIter iter;

    g_return_if_fail (display_name != NULL);

    if (fileview->priv->select_file_idle)
        g_source_remove (fileview->priv->select_file_idle);
    fileview->priv->select_file_idle = 0;

    if (moo_folder_model_get_iter_by_display_name (
        MOO_FOLDER_MODEL (fileview->priv->model), display_name, &iter))
    {
        file_view_select_iter (fileview, &iter);
    }
    else
    {
        g_object_set_data_full (G_OBJECT (fileview),
                                "moo-file-view-select-display-name",
                                g_strdup (display_name), g_free);
        g_idle_add_full (G_PRIORITY_LOW,
                         (GSourceFunc) do_select_display_name,
                         fileview, NULL);
    }
}


/* returns path in the fileview->priv->filter_model */
static GtkTreePath *file_view_get_selected      (MooFileView    *fileview)
{
    GList *rows;
    GtkTreePath *path;

    rows = file_view_get_selected_rows (fileview);

    if (!rows)
        return NULL;

    if (rows->next)
        g_warning ("%s: more than one row is selected", G_STRLOC);

    path = gtk_tree_path_copy (rows->data);
    g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (rows);

    return path;
}


static GList    *file_view_get_selected_rows    (MooFileView    *fileview)
{
    if (fileview->priv->view_type == MOO_FILE_VIEW_LIST)
    {
        GtkTreeSelection *selection;
        GtkTreeView *treeview;

        treeview = fileview->priv->treeview;
        selection = gtk_tree_view_get_selection (treeview);

        return gtk_tree_selection_get_selected_rows (selection, NULL);
    }
    else
    {
        return moo_icon_view_get_selected_rows (fileview->priv->iconview);
    }
}


static const char  *get_selected_display_name   (MooFileView    *fileview)
{
    GtkTreeModel *model = fileview->priv->filter_model;
    GtkTreePath *filter_path;
    GtkTreeIter filter_iter;
    const char *name;
    MooFile *file = NULL;

    filter_path = file_view_get_selected (fileview);

    if (!filter_path)
        return NULL;

    gtk_tree_model_get_iter (model, &filter_iter, filter_path);
    gtk_tree_path_free (filter_path);

    gtk_tree_model_get (model, &filter_iter, COLUMN_FILE, &file, -1);
    g_return_val_if_fail (file != NULL, NULL);
    name = moo_file_display_name (file);
    moo_file_unref (file);

    return name;
}


/****************************************************************************/
/* Path entry
 */

enum {
    ENTRY_STATE_NONE        = 0,
    ENTRY_STATE_TYPEAHEAD   = 1,
    ENTRY_STATE_COMPLETION  = 2
};

static void     typeahead_create        (MooFileView    *fileview);
static void     typeahead_destroy       (MooFileView    *fileview);
static void     typeahead_try           (MooFileView    *fileview,
                                         gboolean        need_to_refilter);
static void     typeahead_tab_key       (MooFileView    *fileview);
static gboolean typeahead_stop_tab_cycle(MooFileView    *fileview);

static void     entry_changed           (GtkEntry       *entry,
                                         MooFileView    *fileview);
static gboolean entry_key_press         (GtkEntry       *entry,
                                         GdkEventKey    *event,
                                         MooFileView    *fileview);
static gboolean entry_focus_out         (GtkWidget      *entry,
                                         GdkEventFocus  *event,
                                         MooFileView    *fileview);
static void     entry_activate          (GtkEntry       *entry,
                                         MooFileView    *fileview);

static gboolean entry_stop_tab_cycle    (MooFileView    *fileview);
static gboolean entry_tab_key           (GtkEntry       *entry,
                                         MooFileView    *fileview);

static gboolean looks_like_path         (const char     *text);


static void         path_entry_init         (MooFileView    *fileview)
{
    GtkEntry *entry = fileview->priv->entry;

    /* XXX after? */
    g_signal_connect (entry, "changed",
                      G_CALLBACK (entry_changed), fileview);
    g_signal_connect (entry, "key-press-event",
                      G_CALLBACK (entry_key_press), fileview);
    g_signal_connect (entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out), fileview);
    g_signal_connect (entry, "activate",
                      G_CALLBACK (entry_activate), fileview);

    typeahead_create (fileview);
}


static void         path_entry_deinit       (MooFileView    *fileview)
{
    typeahead_destroy (fileview);
}


static void     entry_changed       (GtkEntry       *entry,
                                     MooFileView    *fileview)
{
    gboolean need_to_refilter = FALSE;

    const char *text = gtk_entry_get_text (entry);

    /*
    First, decide what's going on (doesn't look like something can be
    going on, but, anyway).
    Then:
    1) If text typed in is a beginning of some file in the list,
    select that file (like treeview interactive search).
    2) If text is a beginning of some hidden files (but not filtered
    out, those are always ignored), show those hidden files and
    select first of them.
    3) Otherwise, parse text as <path>/<file> (<file> may be empty),
    and do the entry completion stuff.

    Tab completion doesn't interfere with this code - tab completion sets
    entry text while this handler is blocked.
    */

    /* Check if some file was shown temporarily, and hide it */
    if (fileview->priv->temp_visible)
    {
        g_string_free (fileview->priv->temp_visible, TRUE);
        fileview->priv->temp_visible = NULL;
        need_to_refilter = TRUE;
    }

    if (!text[0])
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        file_view_move_selection (fileview, NULL);
        return;
    }

    /* TODO take ~file into account */
    /* If entered text looks like path, do completion stuff */
    if (looks_like_path (text))
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        file_view_move_selection (fileview, NULL);
        fileview->priv->entry_state = ENTRY_STATE_COMPLETION;
        /* XXX call complete() or something, for automatic popup */
        return;
    }
    else
    {
        fileview->priv->entry_state = ENTRY_STATE_TYPEAHEAD;
        return typeahead_try (fileview, need_to_refilter);
    }
}


static gboolean entry_key_press     (GtkEntry       *entry,
                                     GdkEventKey    *event,
                                     MooFileView    *fileview)
{
    GtkWidget *filewidget;
    const char *name;

    if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK))
        return FALSE;

    if (event->keyval != GDK_Tab)
    {
        if (entry_stop_tab_cycle (fileview))
            g_signal_emit_by_name (entry, "changed");
    }

    switch (event->keyval)
    {
        case GDK_Escape:
            stop_path_entry (fileview, TRUE);
            return TRUE;

        case GDK_Up:
        case GDK_KP_Up:
        case GDK_Down:
        case GDK_KP_Down:
            filewidget = get_file_view_widget (fileview);
            GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS (filewidget))->
                    key_press_event (filewidget, event);
            name = get_selected_display_name (fileview);
            g_return_val_if_fail (name != NULL, TRUE);
            path_entry_set_text (fileview, name);
            gtk_editable_set_position (GTK_EDITABLE (entry), -1);
            return TRUE;

        case GDK_Tab:
            return entry_tab_key (entry, fileview);

        default:
            return FALSE;
    }
}


static gboolean entry_stop_tab_cycle    (MooFileView    *fileview)
{
    switch (fileview->priv->entry_state)
    {
        case ENTRY_STATE_TYPEAHEAD:
            return typeahead_stop_tab_cycle (fileview);
        case ENTRY_STATE_COMPLETION:
            fileview->priv->entry_state = 0;
            return FALSE;
        default:
            return FALSE;
    }
}


static gboolean entry_tab_key       (GtkEntry       *entry,
                                     MooFileView    *fileview)
{
    const char *text;

    if (!fileview->priv->entry_state)
    {
        text = gtk_entry_get_text (entry);

        if (text[0])
        {
            g_signal_emit_by_name (entry, "changed");
            g_return_val_if_fail (fileview->priv->entry_state != 0, FALSE);
            return entry_tab_key (entry, fileview);
        }
        else
        {
            return FALSE;
        }
    }

    if (fileview->priv->entry_state == ENTRY_STATE_TYPEAHEAD)
    {
        typeahead_tab_key (fileview);
        return TRUE;
    }

    return FALSE;
}


static void         path_entry_set_text     (MooFileView    *fileview,
                                             const char     *text)
{
    GtkEntry *entry = fileview->priv->entry;
    g_signal_handlers_block_by_func (entry, (gpointer) entry_changed,
                                     fileview);
    gtk_entry_set_text (entry, text);
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    g_signal_handlers_unblock_by_func (entry, (gpointer) entry_changed,
                                       fileview);
}


static void         path_entry_delete_to_cursor (MooFileView *fileview)
{
    GtkEditable *entry = GTK_EDITABLE (fileview->priv->entry);
    if (gtk_widget_is_focus (GTK_WIDGET (entry)))
        gtk_editable_delete_text (entry, 0,
                                  gtk_editable_get_position (entry));
}


static void     entry_activate      (GtkEntry       *entry,
                                     MooFileView    *fileview)
{
    char *filename = NULL;
    GtkTreePath *selected;

    selected = file_view_get_selected (fileview);

    if (selected)
    {
        GtkTreeIter iter;
        MooFile *file = NULL;
        gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, selected);
        gtk_tree_model_get (fileview->priv->filter_model, &iter, COLUMN_FILE, &file, -1);
        gtk_tree_path_free (selected);
        g_return_if_fail (file != NULL);

        /* XXX display_name() */
        filename = g_strdup (moo_file_display_name (file));
        moo_file_unref (file);
    }
    else
    {
        filename = g_strdup (gtk_entry_get_text (entry));
    }

    stop_path_entry (fileview, TRUE);
    file_view_activate_filename (fileview, filename);
    g_free (filename);
}


#if 0
#define PRINT_KEY_EVENT(event)                                      \
    g_print ("%s%s%s%s\n",                                          \
             event->state & GDK_SHIFT_MASK ? "<Shift>" : "",        \
             event->state & GDK_CONTROL_MASK ? "<Control>" : "",    \
             event->state & GDK_MOD1_MASK ? "<Alt>" : "",           \
             gdk_keyval_name (event->keyval))
#else
#define PRINT_KEY_EVENT(event)
#endif


static gboolean     moo_file_view_key_press     (GtkWidget      *widget,
                                                 GdkEventKey    *event,
                                                 MooFileView    *fileview)
{
    if (fileview->priv->entry_state)
    {
        g_warning ("%s: something wrong", G_STRLOC);
        stop_path_entry (fileview, FALSE);
        return FALSE;
    }

    /* return immediately if event doesn't look like text typed in */

    if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
        return FALSE;

    switch (event->keyval)
    {
        case GDK_VoidSymbol:
        case GDK_BackSpace:
        case GDK_Tab:
        case GDK_Linefeed:
        case GDK_Clear:
        case GDK_Return:
        case GDK_Pause:
        case GDK_Scroll_Lock:
        case GDK_Sys_Req:
        case GDK_Escape:
        case GDK_Delete:
        case GDK_Multi_key:
        case GDK_Codeinput:
        case GDK_SingleCandidate:
        case GDK_MultipleCandidate:
        case GDK_PreviousCandidate:
        case GDK_Kanji:
        case GDK_Muhenkan:
        case GDK_Henkan_Mode:
        case GDK_Romaji:
        case GDK_Hiragana:
        case GDK_Katakana:
        case GDK_Hiragana_Katakana:
        case GDK_Zenkaku:
        case GDK_Hankaku:
        case GDK_Zenkaku_Hankaku:
        case GDK_Touroku:
        case GDK_Massyo:
        case GDK_Kana_Lock:
        case GDK_Kana_Shift:
        case GDK_Eisu_Shift:
        case GDK_Eisu_toggle:
        case GDK_Home:
        case GDK_Left:
        case GDK_Up:
        case GDK_Right:
        case GDK_Down:
        case GDK_Page_Up:
        case GDK_Page_Down:
        case GDK_End:
        case GDK_Begin:
        case GDK_Select:
        case GDK_Print:
        case GDK_Execute:
        case GDK_Insert:
        case GDK_Undo:
        case GDK_Redo:
        case GDK_Menu:
        case GDK_Find:
        case GDK_Cancel:
        case GDK_Help:
        case GDK_Break:
        case GDK_Mode_switch:
        case GDK_Num_Lock:
        case GDK_KP_Tab:
        case GDK_KP_Enter:
        case GDK_KP_F1:
        case GDK_KP_F2:
        case GDK_KP_F3:
        case GDK_KP_F4:
        case GDK_KP_Home:
        case GDK_KP_Left:
        case GDK_KP_Up:
        case GDK_KP_Right:
        case GDK_KP_Down:
        case GDK_KP_Page_Up:
        case GDK_KP_Page_Down:
        case GDK_KP_End:
        case GDK_KP_Begin:
        case GDK_KP_Insert:
        case GDK_KP_Delete:
        case GDK_F1:
        case GDK_F2:
        case GDK_F3:
        case GDK_F4:
        case GDK_F5:
        case GDK_F6:
        case GDK_F7:
        case GDK_F8:
        case GDK_F9:
        case GDK_F10:
        case GDK_F11:
        case GDK_F12:
        case GDK_F13:
        case GDK_F14:
        case GDK_F15:
        case GDK_F16:
        case GDK_F17:
        case GDK_F18:
        case GDK_F19:
        case GDK_F20:
        case GDK_F21:
        case GDK_F22:
        case GDK_F23:
        case GDK_F24:
        case GDK_F25:
        case GDK_F26:
        case GDK_F27:
        case GDK_F28:
        case GDK_F29:
        case GDK_F30:
        case GDK_F31:
        case GDK_F32:
        case GDK_F33:
        case GDK_F34:
        case GDK_F35:
        case GDK_Shift_L:
        case GDK_Shift_R:
        case GDK_Control_L:
        case GDK_Control_R:
        case GDK_Caps_Lock:
        case GDK_Shift_Lock:
        case GDK_Meta_L:
        case GDK_Meta_R:
        case GDK_Alt_L:
        case GDK_Alt_R:
        case GDK_Super_L:
        case GDK_Super_R:
        case GDK_Hyper_L:
        case GDK_Hyper_R:
        case GDK_ISO_Lock:
        case GDK_ISO_Level2_Latch:
        case GDK_ISO_Level3_Shift:
        case GDK_ISO_Level3_Latch:
        case GDK_ISO_Level3_Lock:
        case GDK_ISO_Group_Latch:
        case GDK_ISO_Group_Lock:
        case GDK_ISO_Next_Group:
        case GDK_ISO_Next_Group_Lock:
        case GDK_ISO_Prev_Group:
        case GDK_ISO_Prev_Group_Lock:
        case GDK_ISO_First_Group:
        case GDK_ISO_First_Group_Lock:
        case GDK_ISO_Last_Group:
        case GDK_ISO_Last_Group_Lock:
        case GDK_ISO_Left_Tab:
        case GDK_ISO_Move_Line_Up:
        case GDK_ISO_Move_Line_Down:
        case GDK_ISO_Partial_Line_Up:
        case GDK_ISO_Partial_Line_Down:
        case GDK_ISO_Partial_Space_Left:
        case GDK_ISO_Partial_Space_Right:
        case GDK_ISO_Set_Margin_Left:
        case GDK_ISO_Set_Margin_Right:
        case GDK_ISO_Release_Margin_Left:
        case GDK_ISO_Release_Margin_Right:
        case GDK_ISO_Release_Both_Margins:
        case GDK_ISO_Fast_Cursor_Left:
        case GDK_ISO_Fast_Cursor_Right:
        case GDK_ISO_Fast_Cursor_Up:
        case GDK_ISO_Fast_Cursor_Down:
        case GDK_ISO_Continuous_Underline:
        case GDK_ISO_Discontinuous_Underline:
        case GDK_ISO_Emphasize:
        case GDK_ISO_Center_Object:
        case GDK_ISO_Enter:
        case GDK_First_Virtual_Screen:
        case GDK_Prev_Virtual_Screen:
        case GDK_Next_Virtual_Screen:
        case GDK_Last_Virtual_Screen:
        case GDK_Terminate_Server:
        case GDK_AccessX_Enable:
        case GDK_AccessX_Feedback_Enable:
        case GDK_RepeatKeys_Enable:
        case GDK_SlowKeys_Enable:
        case GDK_BounceKeys_Enable:
        case GDK_StickyKeys_Enable:
        case GDK_MouseKeys_Enable:
        case GDK_MouseKeys_Accel_Enable:
        case GDK_Overlay1_Enable:
        case GDK_Overlay2_Enable:
        case GDK_AudibleBell_Enable:
        case GDK_Pointer_Left:
        case GDK_Pointer_Right:
        case GDK_Pointer_Up:
        case GDK_Pointer_Down:
        case GDK_Pointer_UpLeft:
        case GDK_Pointer_UpRight:
        case GDK_Pointer_DownLeft:
        case GDK_Pointer_DownRight:
        case GDK_Pointer_Button_Dflt:
        case GDK_Pointer_Button1:
        case GDK_Pointer_Button2:
        case GDK_Pointer_Button3:
        case GDK_Pointer_Button4:
        case GDK_Pointer_Button5:
        case GDK_Pointer_DblClick_Dflt:
        case GDK_Pointer_DblClick1:
        case GDK_Pointer_DblClick2:
        case GDK_Pointer_DblClick3:
        case GDK_Pointer_DblClick4:
        case GDK_Pointer_DblClick5:
        case GDK_Pointer_Drag_Dflt:
        case GDK_Pointer_Drag1:
        case GDK_Pointer_Drag2:
        case GDK_Pointer_Drag3:
        case GDK_Pointer_Drag4:
        case GDK_Pointer_Drag5:
        case GDK_Pointer_EnableKeys:
        case GDK_Pointer_Accelerate:
        case GDK_Pointer_DfltBtnNext:
        case GDK_Pointer_DfltBtnPrev:
        case GDK_3270_Duplicate:
        case GDK_3270_FieldMark:
        case GDK_3270_Right2:
        case GDK_3270_Left2:
        case GDK_3270_BackTab:
        case GDK_3270_EraseEOF:
        case GDK_3270_EraseInput:
        case GDK_3270_Reset:
        case GDK_3270_Quit:
        case GDK_3270_PA1:
        case GDK_3270_PA2:
        case GDK_3270_PA3:
        case GDK_3270_Test:
        case GDK_3270_Attn:
        case GDK_3270_CursorBlink:
        case GDK_3270_AltCursor:
        case GDK_3270_KeyClick:
        case GDK_3270_Jump:
        case GDK_3270_Ident:
        case GDK_3270_Rule:
        case GDK_3270_Copy:
        case GDK_3270_Play:
        case GDK_3270_Setup:
        case GDK_3270_Record:
        case GDK_3270_ChangeScreen:
        case GDK_3270_DeleteWord:
        case GDK_3270_ExSelect:
        case GDK_3270_CursorSelect:
        case GDK_3270_PrintScreen:
        case GDK_3270_Enter:
            return FALSE;
    }

    PRINT_KEY_EVENT (event);

    if (GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS (widget))->key_press_event (widget, event))
        return TRUE;

    if (GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS (fileview))->key_press_event (widget, event))
        return TRUE;

    if (event->string && event->length)
    {
        GdkEvent *copy;
        GtkWidget *entry = GTK_WIDGET (fileview->priv->entry);

        g_return_val_if_fail (event != NULL, FALSE);
        g_return_val_if_fail (GTK_WIDGET_REALIZED (entry), FALSE);

        copy = gdk_event_copy ((GdkEvent*) event);
        g_object_unref (copy->key.window);
        copy->key.window = g_object_ref (entry->window);

        gtk_widget_grab_focus (entry);

        path_entry_set_text (fileview, "");
        gtk_widget_event (entry, copy);

        gdk_event_free (copy);
        return TRUE;
    }

    return FALSE;
}


static gboolean entry_focus_out     (GtkWidget      *entry,
                                     G_GNUC_UNUSED GdkEventFocus *event,
                                     MooFileView    *fileview)
{
    /* focus may be lost due to switching to other window, do nothing then */
    if (!gtk_widget_is_focus (entry))
        stop_path_entry (fileview, TRUE);
    return FALSE;
}


static void     stop_path_entry     (MooFileView    *fileview,
                                     gboolean        focus_file_list)
{
    char *text;

    typeahead_stop_tab_cycle (fileview);

    fileview->priv->entry_state = 0;

    if (fileview->priv->current_dir)
        text = g_filename_display_name (moo_folder_get_path (fileview->priv->current_dir));
    else
        text = g_strdup ("");

    path_entry_set_text (fileview, text);

    if (focus_file_list)
        focus_to_file_view (fileview);

    g_free (text);
}


/* WIN32_XXX */
static void         file_view_activate_filename (MooFileView    *fileview,
                                                 const char     *display_name)
{
    GError *error = NULL;
    char *dirname, *basename;
    char *path = NULL;
    const char *current_dir = NULL;

    if (fileview->priv->current_dir)
        current_dir = moo_folder_get_path (fileview->priv->current_dir);

    path = moo_file_system_get_absolute_path (fileview->priv->file_system,
                                              display_name, current_dir);

    if (!path || !g_file_test (path, G_FILE_TEST_EXISTS))
    {
        g_free (path);
        return;
    }

    if (g_file_test (path, G_FILE_TEST_IS_DIR))
    {
        if (!moo_file_view_chdir (fileview, path, &error))
        {
            g_warning ("%s: could not chdir to %s",
                       G_STRLOC, path);
            if (error)
            {
                g_warning ("%s: %s", G_STRLOC, error->message);
                g_error_free (error);
            }
        }

        g_free (path);
        return;
    }

    dirname = g_path_get_dirname (path);
    basename = g_path_get_basename (path);

    if (!dirname || !basename)
    {
        g_free (path);
        g_free (dirname);
        g_free (basename);
        g_return_if_reached ();
    }

    if (!moo_file_view_chdir (fileview, dirname, &error))
    {
        g_warning ("%s: could not chdir to %s",
                    G_STRLOC, dirname);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }

        g_free (path);
        g_free (dirname);
        g_free (basename);
        return;
    }

    moo_file_view_select_name (fileview, basename);
    g_signal_emit (fileview, signals[ACTIVATE], 0, path);

    g_free (path);
    g_free (dirname);
    g_free (basename);
}


static gboolean looks_like_path         (const char     *text)
{
    if (strchr (text, '/'))
        return TRUE;
#ifdef __WIN32__
    else if (strchr (text, '\\'))
        return TRUE;
#endif
    else
        return FALSE;
}


/*********************************************************************/
/* Typeahead
 */

struct _Typeahead {
    MooFileView *fileview;
    GtkEntry *entry;
    GString *matched_prefix;
    int matched_prefix_char_len;
    TextFuncs text_funcs;
    gboolean case_sensitive;
};

static gboolean typeahead_find_match_visible    (MooFileView    *fileview,
                                                 const char     *text,
                                                 GtkTreeIter    *filter_iter,
                                                 gboolean        exact_match);
static gboolean typeahead_find_match_hidden     (MooFileView    *fileview,
                                                 const char     *text,
                                                 GtkTreeIter    *iter,
                                                 gboolean        exact_match);


static void     typeahead_try       (MooFileView    *fileview,
                                     gboolean        need_to_refilter)
{
    const char *text;
    Typeahead *stuff = fileview->priv->typeahead;
    GtkEntry *entry = stuff->entry;
    GtkTreeIter filter_iter, iter;

    g_assert (fileview->priv->entry_state == ENTRY_STATE_TYPEAHEAD);

    if (stuff->matched_prefix)
    {
        g_string_free (stuff->matched_prefix, TRUE);
        stuff->matched_prefix = NULL;
    }

    text = gtk_entry_get_text (entry);
    g_assert (text[0] != 0);

    /* TODO windows */

    if (fileview->priv->show_hidden_files || text[0] != '.' || !text[1])
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        if (typeahead_find_match_visible (fileview, text, &filter_iter, FALSE))
            file_view_move_selection (fileview, &filter_iter);
        else
            file_view_move_selection (fileview, NULL);

        return;
    }

    /* check if partial name of hidden file was typed in */
    if (typeahead_find_match_hidden (fileview, text, &iter, FALSE))
    {
        fileview->priv->temp_visible = g_string_new (text);
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                (fileview->priv->filter_model));

        gtk_tree_model_filter_convert_child_iter_to_iter (
                GTK_TREE_MODEL_FILTER (fileview->priv->filter_model),
        &filter_iter, &iter);
        file_view_move_selection (fileview, &filter_iter);
    }
    else
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        file_view_move_selection (fileview, NULL);
    }
}


static void     typeahead_tab_key       (MooFileView    *fileview)
{
    const char *text;
    Typeahead *stuff = fileview->priv->typeahead;
    GtkEntry *entry = stuff->entry;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath *path;
    MooFile *file = NULL;
    const char *name;

    g_assert (fileview->priv->entry_state == ENTRY_STATE_TYPEAHEAD);

    model = fileview->priv->filter_model;

    if (!gtk_tree_model_get_iter_first (model, &iter))
        return;

    /* see if it's cycling over matched names */
    if (stuff->matched_prefix)
    {
        gboolean found = FALSE;

        path = file_view_get_selected (fileview);

        /* check if there is next name in the list to jump to */
        if (path && gtk_tree_model_get_iter (model, &iter, path) &&
            gtk_tree_model_iter_next (model, &iter))
        {
            found = model_find_next_match (model, &iter,
                                           stuff->matched_prefix->str,
                                           stuff->matched_prefix->len,
                                           &stuff->text_funcs,
                                           FALSE);
        }

        /* if nothing found, start cycling again */
        if (!found)
            found = typeahead_find_match_visible (fileview,
                                                  stuff->matched_prefix->str,
                                                  &iter, FALSE);

        gtk_tree_path_free (path);

        if (!found)
            goto error;
        else
            file_view_move_selection (fileview, &iter);
    }
    else
    {
        gboolean unique;

        text = gtk_entry_get_text (entry);

        if (!text[0])
            return;

        stuff->matched_prefix =
                model_find_max_prefix (fileview->priv->filter_model,
                                       text, &stuff->text_funcs, &unique, &iter);

        if (!stuff->matched_prefix)
            return;

        stuff->matched_prefix_char_len =
                g_utf8_strlen (stuff->matched_prefix->str, stuff->matched_prefix->len);

        /* if match is unique and it's a directory, append a slash */
        if (unique)
        {
            file = NULL;
            gtk_tree_model_get (model, &iter, COLUMN_FILE, &file, -1);

            if (!file)
                goto error;

            name = moo_file_display_name (file);

//             if (!file || stuff->strcmp_func (stuff->matched_prefix->str, file))
//                 goto error;

            if (MOO_FILE_IS_DIR (file))
            {
                char *new_text = g_strdup_printf ("%s%c", name, G_DIR_SEPARATOR);
                g_string_free (stuff->matched_prefix, TRUE);
                stuff->matched_prefix = NULL;
                path_entry_set_text (fileview, new_text);
                g_free (new_text);
                moo_file_unref (file);
                g_signal_emit_by_name (entry, "changed");
                return;
            }

            moo_file_unref (file);
            file = NULL;
        }

        path = file_view_get_selected (fileview);

        if (!path && !typeahead_find_match_visible (fileview, stuff->matched_prefix->str, &iter, FALSE))
            goto error;

        if (path)
            gtk_tree_model_get_iter (model, &iter, path);

        gtk_tree_path_free (path);
    }

    gtk_tree_model_get (model, &iter, COLUMN_FILE, &file, -1);

    if (!file)
        goto error;

//     if (stuff->strncmp_func (stuff->matched_prefix->str, file, stuff->matched_prefix->len))
//         goto error;

    name = moo_file_display_name (file);
    path_entry_set_text (fileview, name);
    gtk_editable_select_region (GTK_EDITABLE (entry),
                                stuff->matched_prefix_char_len,
                                -1);

    moo_file_unref (file);
    return;

error:
    if (stuff->matched_prefix)
        g_string_free (stuff->matched_prefix, TRUE);
    stuff->matched_prefix = NULL;
    moo_file_unref (file);
    g_return_if_reached ();
}


static gboolean typeahead_stop_tab_cycle(MooFileView    *fileview)
{
    Typeahead *stuff = fileview->priv->typeahead;

    if (stuff->matched_prefix)
    {
        g_string_free (stuff->matched_prefix, TRUE);
        stuff->matched_prefix = NULL;
        stuff->matched_prefix_char_len = 0;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


static void     typeahead_create        (MooFileView    *fileview)
{
    Typeahead *stuff = g_new0 (Typeahead, 1);

    stuff->fileview = fileview;
    stuff->entry = fileview->priv->entry;
    stuff->case_sensitive = fileview->priv->typeahead_case_sensitive;

    if (stuff->case_sensitive)
    {
        stuff->text_funcs.strcmp_func = strcmp_func;
        stuff->text_funcs.strncmp_func = strncmp_func;
        stuff->text_funcs.normalize_func = normalize_func;
    }
    else
    {
        stuff->text_funcs.strcmp_func = case_strcmp_func;
        stuff->text_funcs.strncmp_func = case_strncmp_func;
        stuff->text_funcs.normalize_func = case_normalize_func;
    }

    fileview->priv->typeahead = stuff;
}


static void     typeahead_destroy       (MooFileView    *fileview)
{
    Typeahead *stuff = fileview->priv->typeahead;
    if (stuff->matched_prefix)
        g_string_free (stuff->matched_prefix, TRUE);
    g_free (stuff);
    fileview->priv->typeahead = NULL;
}


void        moo_file_view_set_typeahead_case_sensitive  (MooFileView    *fileview,
                                                         gboolean        case_sensitive)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    if (case_sensitive != fileview->priv->typeahead_case_sensitive)
    {
        Typeahead *stuff = fileview->priv->typeahead;

        fileview->priv->typeahead_case_sensitive = case_sensitive;
        stuff->case_sensitive = case_sensitive;

        if (case_sensitive)
        {
            stuff->text_funcs.strcmp_func = strcmp_func;
            stuff->text_funcs.strncmp_func = strncmp_func;
            stuff->text_funcs.normalize_func = normalize_func;
        }
        else
        {
            stuff->text_funcs.strcmp_func = case_strcmp_func;
            stuff->text_funcs.strncmp_func = case_strncmp_func;
            stuff->text_funcs.normalize_func = case_normalize_func;
        }

        g_object_notify (G_OBJECT (fileview), "typeahead-case-sensitive");
    }
}


static gboolean typeahead_find_match_visible    (MooFileView    *fileview,
                                                 const char     *text,
                                                 GtkTreeIter    *filter_iter,
                                                 gboolean        exact_match)
{
    guint len;

    g_return_val_if_fail (text && text[0], FALSE);

    if (!gtk_tree_model_get_iter_first (fileview->priv->filter_model, filter_iter))
        return FALSE;

    len = strlen (text);

    return model_find_next_match (fileview->priv->filter_model,
                                  filter_iter, text, len,
                                  &fileview->priv->typeahead->text_funcs,
                                  exact_match);
}


static gboolean typeahead_find_match_hidden     (MooFileView    *fileview,
                                                 const char     *text,
                                                 GtkTreeIter    *iter,
                                                 gboolean        exact)
{
    guint len;
    GtkTreeModel *model = fileview->priv->model;

    g_return_val_if_fail (text && text[0], FALSE);

    if (!gtk_tree_model_get_iter_first (model, iter))
        return FALSE;

    len = strlen (text);

    while (TRUE)
    {
        MooFile *file = NULL;

        if (!model_find_next_match (model, iter, text, len,
                                    &fileview->priv->typeahead->text_funcs,
                                    exact))
            return FALSE;

        gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

        if (file && moo_file_view_check_visible (fileview, file, TRUE, TRUE))
        {
            moo_file_unref (file);
            return TRUE;
        }

        moo_file_unref (file);

        if (!gtk_tree_model_iter_next (model, iter))
            return FALSE;
    }
}


/***************************************************************************/
/* Drag source
 */

static void
icon_drag_begin (MooFileView    *fileview,
                 G_GNUC_UNUSED GdkDragContext *context,
                 G_GNUC_UNUSED MooIconView *iconview)
{
    GList *files, *l;
    char **uris;
    guint n_files, i;
    MooFolder *folder;

    folder = fileview->priv->current_dir;
    g_return_if_fail (folder != NULL);

    files = file_view_get_selected_files (fileview);
    g_return_if_fail (files != NULL);

    n_files = g_list_length (files);
    uris = g_new0 (char*, n_files + 1);

    for (l = files, i = 0; l != NULL; i++, l = l->next)
    {
        MooFile *file = l->data;
        uris[i] = moo_folder_get_file_uri (folder, file);
    }

    g_object_set_data_full (G_OBJECT (fileview), "moo-file-view-source-uris",
                            uris, (GDestroyNotify) g_strfreev);
    g_object_set_data_full (G_OBJECT (fileview), "moo-file-view-source-dir",
                            g_object_ref (folder), g_object_unref);

    g_list_foreach (files, (GFunc) moo_file_unref, NULL);
    g_list_free (files);
}


static void
icon_drag_data_get (MooFileView    *fileview,
                    G_GNUC_UNUSED GdkDragContext *context,
                    GtkSelectionData *data,
                    G_GNUC_UNUSED guint           info,
                    G_GNUC_UNUSED guint           time,
                    G_GNUC_UNUSED MooIconView    *iconview)
{
    char **uris;

    uris = g_object_get_data (G_OBJECT (fileview), "moo-file-view-source-uris");
    g_return_if_fail (uris && *uris);

    gtk_selection_data_set_uris (data, uris);
}


static void
icon_drag_end (MooFileView    *fileview,
               G_GNUC_UNUSED GdkDragContext *context,
               G_GNUC_UNUSED MooIconView    *iconview)
{
    g_object_set_data (G_OBJECT (fileview), "moo-file-view-source-uris", NULL);
    g_object_set_data (G_OBJECT (fileview), "moo-file-view-source-dir", NULL);
}


/***************************************************************************/
/* Drag destination
 */

#define DROP_OPEN_TIMEOUT 500
#define DROP_OPEN_BLINK_TIME 80


static gboolean
drop_open_timeout_func2 (MooFileView *fileview)
{
    GtkTreePath *path;
    MooFolder *current_dir;
    MooFile *file = NULL;
    GtkTreeIter iter;
    const char *goto_dir;

    path = gtk_tree_row_reference_get_path (fileview->priv->drop_to);
    cancel_drop_open (fileview);
    g_return_val_if_fail (path != NULL, FALSE);

    if (!gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, path))
    {
        gtk_tree_path_free (path);
        g_return_val_if_reached (FALSE);
    }

    gtk_tree_path_free (path);

    current_dir = fileview->priv->current_dir;
    g_return_val_if_fail (current_dir != NULL, FALSE);

    gtk_tree_model_get (fileview->priv->filter_model,
                        &iter, COLUMN_FILE, &file, -1);
    g_return_val_if_fail (file != NULL, FALSE);

    goto_dir = NULL;

    if (!strcmp (moo_file_name (file), "..") || MOO_FILE_IS_DIR (file))
    {
        goto_dir = moo_file_name (file);
    }
    else
    {
        g_warning ("%s: oops", G_STRLOC);
    }

    if (goto_dir && moo_file_view_chdir (fileview, goto_dir, NULL))
        moo_file_view_select_name (fileview, NULL);

    moo_file_unref (file);
    return FALSE;
}


static gboolean
drop_open_timeout_func (MooFileView *fileview)
{
    GtkTreePath *path;

    fileview->priv->drop_to_timeout = 0;

    g_return_val_if_fail (fileview->priv->drop_to != NULL, FALSE);
    path = gtk_tree_row_reference_get_path (fileview->priv->drop_to);

    if (!path)
    {
        cancel_drop_open (fileview);
        moo_icon_view_set_drag_dest_row (fileview->priv->iconview, NULL);
        g_return_val_if_reached (FALSE);
    }

    if (fileview->priv->drop_to_blink_clear)
    {
        fileview->priv->drop_to_blink_clear = FALSE;
        moo_icon_view_set_drag_dest_row (fileview->priv->iconview, path);

        if (++fileview->priv->drop_to_n_blinks > 1)
            fileview->priv->drop_to_timeout =
                    g_timeout_add (DROP_OPEN_BLINK_TIME,
                                   (GSourceFunc) drop_open_timeout_func2,
                                   fileview);
    }
    else
    {
        fileview->priv->drop_to_blink_clear = TRUE;
        moo_icon_view_set_drag_dest_row (fileview->priv->iconview, NULL);
    }

    if (!fileview->priv->drop_to_timeout)
        fileview->priv->drop_to_timeout =
                g_timeout_add (DROP_OPEN_BLINK_TIME,
                               (GSourceFunc) drop_open_timeout_func,
                               fileview);

    return FALSE;
}


static void
icon_drag_data_received (MooFileView    *fileview,
                         GdkDragContext *context,
                         int             x,
                         int             y,
                         GtkSelectionData *data,
                         guint           info,
                         guint           time,
                         MooIconView    *iconview)
{
    gboolean dummy;
    char *dir = NULL;
    GtkTreePath *path = NULL;
    MooFolder *current_dir;

    cancel_drop_open (fileview);

    current_dir = fileview->priv->current_dir;

    if (!current_dir)
    {
        g_critical ("%s: oops", G_STRLOC);
        gtk_drag_finish (context, FALSE, FALSE, time);
        return;
    }

    if (moo_icon_view_get_path_at_pos (iconview, x, y, &path, NULL, NULL, NULL))
    {
        MooFile *file = file_view_get_file_at_path (fileview, path);

        if (MOO_FILE_IS_DIR (file))
            dir = moo_folder_get_file_path (current_dir, file);

        moo_file_unref (file);
    }

    if (!dir)
        dir = g_strdup (moo_folder_get_path (current_dir));

    g_signal_emit (fileview, signals[DROP_DATA_RECEIVED], 0,
                   dir, iconview, context, data, info, time, &dummy);

    gtk_tree_path_free (path);
    g_free (dir);
}


static gboolean
icon_drag_drop (MooFileView    *fileview,
                GdkDragContext *context,
                int             x,
                int             y,
                guint           time,
                MooIconView    *iconview)
{
    gboolean dummy;

    char *dir = NULL;
    GtkTreePath *path = NULL;
    MooFolder *current_dir;

    cancel_drop_open (fileview);

    current_dir = fileview->priv->current_dir;

    if (!current_dir)
    {
        g_critical ("%s: oops", G_STRLOC);
        gtk_drag_finish (context, FALSE, FALSE, time);
        return FALSE;
    }

    if (moo_icon_view_get_path_at_pos (iconview, x, y, &path, NULL, NULL, NULL))
    {
        MooFile *file = file_view_get_file_at_path (fileview, path);

        if (MOO_FILE_IS_DIR (file))
            dir = moo_folder_get_file_path (current_dir, file);

        moo_file_unref (file);
    }

    if (!dir)
        dir = g_strdup (moo_folder_get_path (current_dir));

    g_signal_emit (fileview, signals[DROP], 0, dir, iconview, context, time, &dummy);
    moo_icon_view_set_drag_dest_row (iconview, NULL);

    gtk_tree_path_free (path);
    g_free (dir);
    return TRUE;
}


static gboolean
moo_file_view_drop (G_GNUC_UNUSED MooFileView *fileview,
                    G_GNUC_UNUSED const char *path,
                    GtkWidget      *widget,
                    GdkDragContext *context,
                    guint           time)
{
    gtk_drag_get_data (widget, context,
                       gdk_atom_intern ("text/uri-list", FALSE),
                       time);
    cancel_drop_open (fileview);
    return TRUE;
}


static gboolean
moo_file_view_drop_data_received (MooFileView    *fileview,
                                  const char     *path,
                                  GtkWidget      *widget,
                                  GdkDragContext *context,
                                  GtkSelectionData *data,
                                  guint           info,
                                  guint           time)
{
    g_print ("got uris dropped to %s!!!\n", path);
    gtk_drag_finish (context, FALSE, FALSE, time);
    cancel_drop_open (fileview);
    return TRUE;
}


static void
icon_drag_leave (MooFileView *fileview,
                 G_GNUC_UNUSED GdkDragContext *context,
                 G_GNUC_UNUSED guint time,
                 MooIconView    *iconview)
{
    moo_icon_view_set_drag_dest_row (iconview, NULL);
    cancel_drop_open (fileview);
}


static void
cancel_drop_open (MooFileView *fileview)
{
    if (fileview->priv->drop_to)
        gtk_tree_row_reference_free (fileview->priv->drop_to);

    if (fileview->priv->drop_to_timeout)
        g_source_remove (fileview->priv->drop_to_timeout);

    fileview->priv->drop_to_blink_clear = FALSE;
    fileview->priv->drop_to_n_blinks = 0;
    fileview->priv->drop_to = NULL;
    fileview->priv->drop_to_timeout = 0;
}


static gboolean
icon_drag_motion (MooIconView    *iconview,
                  GdkDragContext *context,
                  int             x,
                  int             y,
                  guint           time,
                  MooFileView    *fileview)
{
    MooFolder *source_dir;
    MooFolder *current_dir;
    GtkTreePath *path = NULL;
    gboolean drag_to_current_dir = TRUE;
    MooIconViewCell cell;
    int cell_x, cell_y;

    current_dir = fileview->priv->current_dir;
    source_dir = g_object_get_data (G_OBJECT (fileview), "moo-file-view-source-dir");

    if (!current_dir)
        goto out;

    if (moo_icon_view_get_path_at_pos (iconview, x, y, &path, &cell, &cell_x, &cell_y))
    {
        MooFile *file = file_view_get_file_at_path (fileview, path);

        if (MOO_FILE_IS_DIR (file))
            drag_to_current_dir = FALSE;

        moo_file_unref (file);
    }

    if (drag_to_current_dir)
        moo_icon_view_set_drag_dest_row (iconview, NULL);
    else
        moo_icon_view_set_drag_dest_row (iconview, path);

    if (!drag_to_current_dir || source_dir != current_dir)
        gdk_drag_status (context, context->suggested_action, time);
    else
        gdk_drag_status (context, 0, time);

    if (!drag_to_current_dir)
    {
        gboolean new_timeout = TRUE;

        if (fileview->priv->drop_to)
        {
            GtkTreePath *old_path = gtk_tree_row_reference_get_path (fileview->priv->drop_to);

            if (old_path && !gtk_tree_path_compare (path, old_path) &&
                fileview->priv->drop_to_cell == cell &&
                !gtk_drag_check_threshold (GTK_WIDGET (iconview),
                                           fileview->priv->drop_to_x,
                                           fileview->priv->drop_to_y,
                                           cell_x,
                                           cell_y))
            {
                new_timeout = FALSE;
                g_assert (fileview->priv->drop_to_timeout != 0);
            }
        }

        if (new_timeout)
        {
            cancel_drop_open (fileview);

            fileview->priv->drop_to =
                    gtk_tree_row_reference_new (fileview->priv->filter_model, path);
            fileview->priv->drop_to_timeout =
                    g_timeout_add (DROP_OPEN_TIMEOUT,
                                   (GSourceFunc) drop_open_timeout_func,
                                   fileview);
            fileview->priv->drop_to_x = cell_x;
            fileview->priv->drop_to_y = cell_y;
            fileview->priv->drop_to_cell = cell;
        }
    }
    else
    {
        cancel_drop_open (fileview);
    }

out:
    if (path)
        gtk_tree_path_free (path);
    return TRUE;
}
