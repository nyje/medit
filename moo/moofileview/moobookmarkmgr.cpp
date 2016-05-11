/*
 *   moobookmarkmgr.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#include "moofileview/moobookmarkmgr.h"
#include "moofileview/moofileentry.h"
#include "mooutils/mooprefs.h"
#include "marshals.h"
#include "mooutils/mooactionfactory.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/moostock.h"
#include "mooutils/mootype-macros.h"
#include "moofileview/moobookmark-editor-gxml.h"
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <gtk/gtk.h>
#include <moocpp/moocpp.h>
#include <mooglib/moo-glib.h>

using namespace moo;

#define COLUMN_BOOKMARK MOO_BOOKMARK_MGR_COLUMN_BOOKMARK

struct UserInfo
{
    g::ObjectRawPtr user;
    gobj_raw_ptr<MooUiXml> xml;
    gobj_raw_ptr<MooActionCollection> actions;
    gstr path;
    guint user_id = 0;
    guint merge_id = 0;
    std::vector<gtk::ActionPtr> bm_actions;
};

typedef std::shared_ptr<UserInfo> UserInfoPtr;

struct MooBookmarkMgrPrivate
{
    gtk::ListStorePtr store;
    std::vector<UserInfoPtr> users;
    gtk::WidgetPtr editor;
    bool loading = false;
    guint last_user_id = 0;
    guint update_idle = 0;
};


static void moo_bookmark_mgr_finalize       (GObject        *object);
static void moo_bookmark_mgr_changed        (MooBookmarkMgr *mgr);
static void emit_changed                    (MooBookmarkMgr *mgr);
static void moo_bookmark_mgr_add_separator  (MooBookmarkMgr *mgr);

static void moo_bookmark_mgr_load           (MooBookmarkMgr *mgr);
static void moo_bookmark_mgr_save           (MooBookmarkMgr *mgr);

static void mgr_remove_user                 (MooBookmarkMgr *mgr,
                                             UserInfo       &info);
static gboolean mgr_update_menus            (MooBookmarkMgr *mgr);

static MooBookmark *_moo_bookmark_copy      (MooBookmark    *bookmark);
static void _moo_bookmark_free              (MooBookmark    *bookmark);


MOO_DEFINE_BOXED_TYPE_C (MooBookmark, _moo_bookmark)
G_DEFINE_TYPE (MooBookmarkMgr, _moo_bookmark_mgr, G_TYPE_OBJECT)

enum {
    CHANGED,
    ACTIVATE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
_moo_bookmark_mgr_class_init (MooBookmarkMgrClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = moo_bookmark_mgr_finalize;

    klass->changed = moo_bookmark_mgr_changed;

    signals[CHANGED] =
            g_signal_new ("changed",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooBookmarkMgrClass, changed),
                          NULL, NULL,
                          _moo_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

    signals[ACTIVATE] =
            g_signal_new ("activate",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooBookmarkMgrClass, activate),
                          NULL, NULL,
                          _moo_marshal_VOID__BOXED_OBJECT,
                          G_TYPE_NONE, 2,
                          MOO_TYPE_BOOKMARK, G_TYPE_OBJECT);
}


static void
_moo_bookmark_mgr_init (MooBookmarkMgr *mgr)
{
    mgr->priv = new MooBookmarkMgrPrivate();

    mgr->priv->store = gtk::ListStore::create ({ MOO_TYPE_BOOKMARK });

    mgr->priv->store->connect_swapped ("row-changed", G_CALLBACK (emit_changed), mgr);
    mgr->priv->store->connect_swapped ("rows-reordered", G_CALLBACK (emit_changed), mgr);
    mgr->priv->store->connect_swapped ("row-inserted", G_CALLBACK (emit_changed), mgr);
    mgr->priv->store->connect_swapped ("row-deleted", G_CALLBACK (emit_changed), mgr);
}


static void
moo_bookmark_mgr_finalize (GObject *object)
{
    MooBookmarkMgr *mgr = MOO_BOOKMARK_MGR (object);

    if (mgr->priv->update_idle)
        g_source_remove (mgr->priv->update_idle);

    if (mgr->priv->editor)
    {
        mgr->priv->editor->destroy ();
        mgr->priv->editor = nullptr;
    }

    std::vector<UserInfoPtr> users = mgr->priv->users;
    for (const auto& u: users)
        mgr_remove_user (mgr, *u);
    g_assert (mgr->priv->users.empty());
    users.clear ();

    delete mgr->priv;
    mgr->priv = nullptr;

    G_OBJECT_CLASS (_moo_bookmark_mgr_parent_class)->finalize (object);
}


static void
emit_changed (MooBookmarkMgr *mgr)
{
    g_signal_emit (mgr, signals[CHANGED], 0);
}


static void
moo_bookmark_mgr_changed (MooBookmarkMgr *mgr)
{
    if (!mgr->priv->loading)
        moo_bookmark_mgr_save (mgr);
    if (!mgr->priv->update_idle)
        mgr->priv->update_idle = g_idle_add ((GSourceFunc) mgr_update_menus, mgr);
}


void
_moo_bookmark_mgr_add (MooBookmarkMgr *mgr, objp<MooBookmark> bookmark)
{
    g_return_if_fail (MOO_IS_BOOKMARK_MGR (mgr));
    g_return_if_fail (bookmark != nullptr);

    /* XXX validate bookmark */

    GtkTreeIter iter;
    mgr->priv->store->append (iter);
    mgr->priv->store->set (iter, COLUMN_BOOKMARK, bookmark.release());
}


static void
moo_bookmark_mgr_add_separator (MooBookmarkMgr *mgr)
{
    GtkTreeIter iter;
    g_return_if_fail (MOO_IS_BOOKMARK_MGR (mgr));
    mgr->priv->store->append (iter);
}


MooBookmarkMgr *
_moo_bookmark_mgr_new (void)
{
    auto mgr = create_gobj<MooBookmarkMgr> ();
    moo_bookmark_mgr_load (mgr.gobj ());
    return mgr.release ();
}


gtk::TreeModelPtr
_moo_bookmark_mgr_get_model (MooBookmarkMgr *mgr)
{
    g_return_val_if_fail (MOO_IS_BOOKMARK_MGR (mgr), nullptr);
    return mgr->priv->store;
}


MooBookmark::MooBookmark (const char* label,
                          const char* path,
                          const char* icon)
    : path (path, mem_transfer::make_copy)
    , display_path (path ? g::filename_display_name (path) : gstr::null)
    , label(label, mem_transfer::make_copy)
    , icon_stock_id(icon, mem_transfer::make_copy)
{
}

MooBookmark::~MooBookmark ()
{
}


static MooBookmark*
_moo_bookmark_copy (MooBookmark *bookmark)
{
    g_return_val_if_fail (bookmark != NULL, NULL);
    return new MooBookmark (*bookmark);
}


static void
_moo_bookmark_free (MooBookmark *bookmark)
{
    delete bookmark;
}


#if 0
void
_moo_bookmark_set_path (MooBookmark    *bookmark,
                        const char     *path)
{
    char *display_path;
    g_return_if_fail (bookmark != NULL);
    g_return_if_fail (path != NULL);

    display_path = g_filename_display_name (path);
    g_return_if_fail (display_path != NULL);

    g_free (bookmark->path);
    g_free (bookmark->display_path);
    bookmark->display_path = display_path;
    bookmark->path = g_strdup (path);
}
#endif


static void
_moo_bookmark_set_display_path (MooBookmark  *bookmark,
                                const char   *display_path)
{
    g_return_if_fail (bookmark != NULL);
    g_return_if_fail (display_path != NULL);

    gstr path = g::filename_from_utf8 (display_path);
    g_return_if_fail (!path.empty());

    bookmark->path = path;
    bookmark->display_path.set (display_path);
}


/***************************************************************************/
/* Loading and saving
 */
#define BOOKMARKS_ROOT "FileSelector/bookmarks"
#define ELEMENT_BOOKMARK "bookmark"
#define ELEMENT_SEPARATOR "separator"
#define PROP_LABEL "label"
#define PROP_ICON "icon"

static void
moo_bookmark_mgr_load (MooBookmarkMgr *mgr)
{
    MooMarkupNode *xml;
    MooMarkupNode *root, *node;

    xml = moo_prefs_get_markup (MOO_PREFS_RC);
    g_return_if_fail (xml != NULL);

    root = moo_markup_get_element (xml, BOOKMARKS_ROOT);

    if (!root)
        return;

    mgr->priv->loading = TRUE;

    for (node = root->children; node != NULL; node = node->next)
    {
        if (!MOO_MARKUP_IS_ELEMENT (node))
            continue;

        if (!strcmp (node->name, ELEMENT_BOOKMARK))
        {
            const char *label = moo_markup_get_prop (node, PROP_LABEL);
            const char *icon = moo_markup_get_prop (node, PROP_ICON);
            const char *path_utf8 = moo_markup_get_content (node);

            if (!path_utf8 || !path_utf8[0])
            {
                g_warning ("empty path in bookmark");
                continue;
            }

            gstr path = g::filename_from_utf8 (path_utf8);

            if (path.empty())
            {
                g_warning ("could not convert '%s' to filename encoding", path_utf8);
                continue;
            }

            auto bookmark = objp<MooBookmark>::make (label ? label : path_utf8, path, icon);
            _moo_bookmark_mgr_add (mgr, std::move (bookmark));
        }
        else if (!strcmp (node->name, ELEMENT_SEPARATOR))
        {
            moo_bookmark_mgr_add_separator (mgr);
        }
        else
        {
            g_warning ("invalid '%s' element", node->name);
        }
    }

    mgr->priv->loading = FALSE;
}


static void
moo_bookmark_mgr_save (MooBookmarkMgr *mgr)
{
    MooMarkupNode *xml;
    MooMarkupNode *root;

    xml = moo_prefs_get_markup (MOO_PREFS_RC);
    g_return_if_fail (xml != NULL);

    root = moo_markup_get_element (xml, BOOKMARKS_ROOT);

    if (root)
        moo_markup_delete_node (root);

    gtk::ListStore& model = *mgr->priv->store;

    GtkTreeIter iter;
    if (!model.get_iter_first (iter))
        return;

    root = moo_markup_create_element (xml, BOOKMARKS_ROOT);
    g_return_if_fail (root != NULL);

    do
    {
        objp<MooBookmark> bookmark;
        MooMarkupNode *elm;

        model.get (iter, COLUMN_BOOKMARK, bookmark);

        if (!bookmark)
        {
            moo_markup_create_element (root, ELEMENT_SEPARATOR);
            continue;
        }

        /* XXX validate bookmark */
        elm = moo_markup_create_text_element (root, ELEMENT_BOOKMARK,
                                              bookmark->display_path);
        moo_markup_set_prop (elm, PROP_LABEL, bookmark->label);
        if (!bookmark->icon_stock_id.empty())
            moo_markup_set_prop (elm, PROP_ICON, bookmark->icon_stock_id);
    }
    while (model.iter_next (iter));
}


/***************************************************************************/
/* Bookmarks menu
 */

static UserInfoPtr
user_info_new (GObject             *user,
               MooActionCollection *actions,
               MooUiXml            *xml,
               const char          *path,
               guint                user_id)
{
    g_return_val_if_fail (G_IS_OBJECT (user), NULL);
    g_return_val_if_fail (MOO_IS_ACTION_COLLECTION (actions), NULL);
    g_return_val_if_fail (MOO_IS_UI_XML (xml), NULL);
    g_return_val_if_fail (path, NULL);
    g_return_val_if_fail (user_id > 0, NULL);

    UserInfoPtr info = std::make_shared<UserInfo> ();

    info->user = user;
    info->actions = actions;
    info->xml = xml;
    info->path.set (path);
    info->user_id = user_id;

    return info;
}


static void
item_activated (GtkAction      *action,
                MooBookmarkMgr *mgr)
{
    MooBookmark *bookmark;
    gpointer user;

    g_return_if_fail (GTK_IS_ACTION (action));
    g_return_if_fail (MOO_IS_BOOKMARK_MGR (mgr));

    bookmark = reinterpret_cast<MooBookmark*> (g_object_get_data (G_OBJECT (action), "moo-bookmark"));
    user = g_object_get_data (G_OBJECT (action), "moo-bookmark-user");

    g_return_if_fail (bookmark != NULL && user != NULL);

    g_signal_emit (mgr, signals[ACTIVATE], 0, bookmark, user);
}


static void
make_menu (MooBookmarkMgr *mgr,
           UserInfo       &info)
{
    gtk::TreeModel& model = *mgr->priv->store;
    GtkTreeIter iter;
    GtkActionGroup *group;

    if (!model.get_iter_first (iter))
        return;

    info.merge_id = moo_ui_xml_new_merge_id (info.xml);
    
    strbuilder markup;

    group = moo_action_collection_get_group (info.actions, NULL);

    do
    {
        objp<MooBookmark> bookmark;

        model.get (iter, COLUMN_BOOKMARK, bookmark);

        if (!bookmark)
        {
            markup.append ("<separator/>");
            continue;
        }

        gstr action_id = gstr::printf ("MooBookmarkAction-%p", bookmark.get ());

        gtk::ActionPtr action = wrap(
            moo_action_group_add_action (group, action_id,
                                         "label", bookmark->label.empty () ? bookmark->label.get () : bookmark->display_path.get (),
                                         "stock-id", bookmark->icon_stock_id.get (),
                                         "tooltip", bookmark->display_path.get (),
                                         "no-accel", TRUE,
                                         nullptr));
        action->set_data ("moo-bookmark",
                          bookmark.release (),
                          (GDestroyNotify) _moo_bookmark_free);
        action->set_data ("moo-bookmark-user", info.user);
        action->connect ("activate", G_CALLBACK (item_activated), mgr);

        info.bm_actions.push_back (action);

        markup.append_printf ("<item action=\"%s\"/>", action_id);
    }
    while (model.iter_next (iter));

    moo_ui_xml_insert_markup (info.xml, info.merge_id,
                              info.path, -1, markup.get ());
}


static void
destroy_menu (UserInfo& info)
{
    for (const auto& action : info.bm_actions)
        moo_action_collection_remove_action (info.actions, action.gobj ());

    info.bm_actions.clear ();

    if (info.merge_id > 0)
    {
        moo_ui_xml_remove_ui (info.xml, info.merge_id);
        info.merge_id = 0;
    }
}


static gboolean
mgr_update_menus (MooBookmarkMgr *mgr)
{
    mgr->priv->update_idle = 0;

    GtkTreeIter first;
    bool empty = !mgr->priv->store->get_iter_first (first);

    for (const auto& u : mgr->priv->users)
    {
        destroy_menu (*u);

        if (!empty)
            make_menu (mgr, *u);
    }

    return FALSE;
}


void
_moo_bookmark_mgr_add_user (MooBookmarkMgr *mgr,
                            gpointer        user,
                            MooActionCollection *actions,
                            MooUiXml       *xml,
                            const char     *path)
{
    g_return_if_fail (MOO_IS_BOOKMARK_MGR (mgr));
    g_return_if_fail (G_IS_OBJECT (user));
    g_return_if_fail (MOO_IS_ACTION_COLLECTION (actions));
    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (path != NULL);

    UserInfoPtr info = user_info_new (G_OBJECT (user), actions, xml, path,
                                      ++mgr->priv->last_user_id);
    g_return_if_fail (info != nullptr);
    mgr->priv->users.push_back (info);

    make_menu (mgr, *info);
}


static void
mgr_remove_user (MooBookmarkMgr *mgr,
                 UserInfo       &info)
{
    destroy_menu (info);
    remove_if (mgr->priv->users, [&info] (const UserInfoPtr& p) { return p.get () == &info; });
}


void
_moo_bookmark_mgr_remove_user (MooBookmarkMgr *mgr,
                               gpointer        user)
{
    g_return_if_fail (MOO_IS_BOOKMARK_MGR (mgr));

    std::vector<UserInfoPtr> to_remove;

    for (const auto& u : mgr->priv->users)
    {
        if (u->user == user)
            to_remove.push_back (u);
    }

    for (const auto& u : to_remove)
    {
        mgr_remove_user (mgr, *u);
    }
}


#if 0
// static void     create_menu_items           (MooBookmarkMgr *mgr,
//                                              GtkMenuShell   *menu,
//                                              int             position,
//                                              MooBookmarkFunc func,
//                                              gpointer        data)
// {
//     GtkTreeIter iter;
//     GtkTreeModel *model = GTK_TREE_MODEL (mgr->priv->store);
//
//     if (!gtk_tree_model_get_iter_first (model, &iter))
//         return;
//
//     do
//     {
//         MooBookmark *bookmark = NULL;
//         GtkWidget *item, *icon;
//
//         model.get (&iter, COLUMN_BOOKMARK, &bookmark, -1);
//
//         if (!bookmark)
//         {
//             item = gtk_separator_menu_item_new ();
//         }
//         else
//         {
//             if (bookmark->label)
//                 item = gtk_image_menu_item_new_with_label (bookmark->label);
//             else
//                 item = gtk_image_menu_item_new ();
//
//             if (bookmark->pixbuf)
//                 icon = gtk_image_new_from_pixbuf (bookmark->pixbuf);
//             else if (bookmark->icon_stock_id)
//                 icon = gtk_image_new_from_stock (bookmark->icon_stock_id,
//                     GTK_ICON_SIZE_MENU);
//             else
//                 icon = NULL;
//
//             if (icon)
//             {
//                 gtk_widget_show (icon);
//                 gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), icon);
//             }
//
//             g_signal_connect (item, "activate",
//                               G_CALLBACK (menu_item_activated), mgr);
//         }
//
//         g_object_set_data_full (G_OBJECT (item), "moo-bookmark-mgr",
//                                 g_object_ref (mgr), g_object_unref);
//         g_object_set_data_full (G_OBJECT (item), "moo-bookmark",
//                                 bookmark, (GDestroyNotify) _moo_bookmark_free);
//         g_object_set_data (G_OBJECT (item), "moo-bookmark-func", func);
//         g_object_set_data (G_OBJECT (item), "moo-bookmark-data", data);
//
//         gtk_widget_show (item);
//         gtk_menu_shell_insert (menu, item, position);
//         if (position >= 0)
//             position++;
//     }
//     while (gtk_tree_model_iter_next (model, &iter));
// }


static void moo_bookmark_mgr_update_menu(GtkMenuShell   *menu,
                                         MooBookmarkMgr *mgr)
{
    MooBookmarkFunc func;
    gpointer data;
    GList *items;
    int position = 0;

    g_return_if_fail (MOO_IS_BOOKMARK_MGR (mgr));
    g_return_if_fail (GTK_IS_MENU_SHELL (menu));

    g_return_if_fail (g_object_get_data (G_OBJECT (menu), "moo-bookmark-mgr") == mgr);

    func = g_object_get_data (G_OBJECT (menu), "moo-bookmark-func");
    data = g_object_get_data (G_OBJECT (menu), "moo-bookmark-data");

    items = gtk_container_get_children (GTK_CONTAINER (menu));

    if (items)
    {
        GList *l;
        GList *my_items = NULL;

        for (l = items; l != NULL; l = l->next)
        {
            if (g_object_get_data (G_OBJECT (l->data), "moo-bookmark-mgr") == mgr)
            {
                my_items = g_list_prepend (my_items, l->data);
                if (position < 0)
                    position = g_list_position (items, l);
            }
        }

        for (l = my_items; l != NULL; l = l->next)
            gtk_container_remove (GTK_CONTAINER (menu), l->data);

        g_list_free (items);
        g_list_free (my_items);
    }

    create_menu_items (mgr, menu, position, func, data);
}
#endif


/***************************************************************************/
/* Bookmark editor
 */

static gtk::TreeModelPtr copy_bookmarks (gtk::ListStore& store);
static void          copy_bookmarks_back (gtk::ListStore store,
                                          gtk::TreeModel model);
static void          init_editor_dialog (BkEditorXml    *xml);
static void          dialog_response (GtkWidget      *dialog,
                                      int             response,
                                      MooBookmarkMgr *mgr);
static void          dialog_show (GtkWidget      *dialog,
                                  MooBookmarkMgr *mgr);

GtkWidget *
_moo_bookmark_mgr_get_editor (MooBookmarkMgr *mgr)
{
    GtkWidget *dialog;
    BkEditorXml *xml;

    if (mgr->priv->editor)
        return mgr->priv->editor.gobj ();

    xml = bk_editor_xml_new ();
    dialog = GTK_WIDGET (xml->BkEditor);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    init_editor_dialog (xml);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (dialog_response), mgr);
    g_signal_connect (dialog, "delete-event",
                      G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    g_signal_connect (dialog, "show",
                      G_CALLBACK (dialog_show), mgr);

    mgr->priv->editor.set (dialog);

    return dialog;
}


static void
dialog_show (GtkWidget      *dialog,
             MooBookmarkMgr *mgr)
{
    BkEditorXml *xml = bk_editor_xml_get (dialog);
    g_return_if_fail (xml != NULL);
    auto model = copy_bookmarks (*mgr->priv->store);
    gtk_tree_view_set_model (xml->treeview, model.gobj ());
}


static void
dialog_response (GtkWidget      *dialog,
                 int             response,
                 MooBookmarkMgr *mgr)
{
    BkEditorXml *xml = bk_editor_xml_get (dialog);
    g_return_if_fail (xml != NULL);

    if (response != GTK_RESPONSE_OK)
    {
        gtk_widget_hide (dialog);
        return;
    }

    gtk::TreeModelPtr model = wrap (xml->treeview)->get_model ();
    copy_bookmarks_back (*mgr->priv->store, *model);
    gtk_widget_hide (dialog);
}


static bool
copy_value (gtk::TreeModel& src,
            const GtkTreeIter& iter,
            gtk::ListStore& dest)
{
    objp<MooBookmark> bookmark;
    src.get (iter, COLUMN_BOOKMARK, bookmark);

    GtkTreeIter dest_iter;
    dest.append (dest_iter);
    dest.set (dest_iter, COLUMN_BOOKMARK, bookmark);

    return false;
}

static gtk::TreeModelPtr
copy_bookmarks (gtk::ListStore& store)
{
    auto copy = gtk::ListStore::create ({ MOO_TYPE_BOOKMARK });

    store.foreach ([&] (const GtkTreePath&, const GtkTreeIter& iter)
    {
        return copy_value (store, iter, *copy);
    });

    return copy;
}

static void
copy_bookmarks_back (gtk::ListStore store, gtk::TreeModel model)
{
    store.clear ();
    model.foreach ([&] (const GtkTreePath&, const GtkTreeIter& iter)
    {
        return copy_value (model, iter, store);
    });
}


static void     icon_data_func      (GtkTreeViewColumn  *column,
                                     GtkCellRenderer    *cell,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter);
static void     label_data_func     (GtkTreeViewColumn  *column,
                                     GtkCellRenderer    *cell,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter);
static void     path_data_func      (GtkTreeViewColumn  *column,
                                     GtkCellRenderer    *cell,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter);

static void     selection_changed   (GtkTreeSelection   *selection,
                                     BkEditorXml        *xml);
static void     new_clicked         (BkEditorXml        *xml);
static void     delete_clicked      (BkEditorXml        *xml);
static void     separator_clicked   (BkEditorXml        *xml);

static void     label_edited        (GtkCellRenderer    *cell,
                                     char               *path,
                                     char               *text,
                                     BkEditorXml        *xml);
static void     path_edited         (GtkCellRenderer    *cell,
                                     char               *path,
                                     char               *text,
                                     BkEditorXml        *xml);
static void     path_editing_started(GtkCellRenderer    *cell,
                                     GtkCellEditable    *editable);

static void     init_icon_combo     (GtkComboBox        *combo,
                                     BkEditorXml        *xml);
static void     combo_update_icon   (GtkComboBox        *combo,
                                     BkEditorXml        *xml);


static void
init_editor_dialog (BkEditorXml *xml)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    GtkTreeSelection *selection;
    MooFileEntryCompletion *completion;

    init_icon_combo (xml->icon_combo, xml);

    selection = gtk_tree_view_get_selection (xml->treeview);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
    g_signal_connect (selection, "changed",
                      G_CALLBACK (selection_changed), xml);
    selection_changed (selection, xml);

    g_signal_connect_swapped (xml->delete_button, "clicked",
                              G_CALLBACK (delete_clicked), xml);
    g_signal_connect_swapped (xml->new_button, "clicked",
                              G_CALLBACK (new_clicked), xml);
    g_signal_connect_swapped (xml->separator_button, "clicked",
                              G_CALLBACK (separator_clicked), xml);

    /* Icon */
    cell = gtk_cell_renderer_pixbuf_new ();
    /* Column label in file selector bookmark editor */
    column = gtk_tree_view_column_new_with_attributes (C_("fileview-bookmark-editor", "Icon"), cell, NULL);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) icon_data_func,
                                             NULL, NULL);
    gtk_tree_view_append_column (xml->treeview, column);
    g_object_set_data (G_OBJECT (xml->treeview),
                       "moo-bookmarks-icon-column",
                       column);

    /* Label */
    cell = gtk_cell_renderer_text_new ();
    g_object_set (cell, "editable", TRUE, NULL);
    g_signal_connect (cell, "edited",
                      G_CALLBACK (label_edited), xml);

    /* Column label in file selector bookmark editor */
    column = gtk_tree_view_column_new_with_attributes (C_("fileview-bookmark-editor", "Label"), cell, NULL);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) label_data_func,
                                             NULL, NULL);
    gtk_tree_view_append_column (xml->treeview, column);
    g_object_set_data (G_OBJECT (xml->treeview),
                       "moo-bookmarks-label-column",
                       column);

    /* Path */
    cell = gtk_cell_renderer_text_new ();
    g_object_set (cell, "editable", TRUE, NULL);
    g_signal_connect (cell, "edited",
                      G_CALLBACK (path_edited), xml);
    g_signal_connect (cell, "editing-started",
                      G_CALLBACK (path_editing_started), NULL);

    /* Column label in file selector bookmark editor */
    column = gtk_tree_view_column_new_with_attributes (C_("fileview-bookmark-editor", "Path"), cell, NULL);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) path_data_func,
                                             NULL, NULL);
    gtk_tree_view_append_column (xml->treeview, column);
    g_object_set_data (G_OBJECT (xml->treeview),
                       "moo-bookmarks-path-column",
                       column);

    completion = MOO_FILE_ENTRY_COMPLETION (
        g_object_new (MOO_TYPE_FILE_ENTRY_COMPLETION,
                      "directories-only", TRUE,
                      "show-hidden", FALSE,
                      (const char*) NULL));
    g_object_set_data_full (G_OBJECT (cell), "moo-file-entry-completion",
                            completion, g_object_unref);
}


static objp<MooBookmark>
get_bookmark (GtkTreeModel       *model,
              GtkTreeIter        *iter)
{
    objp<MooBookmark> bookmark;
    gtk_tree_model_get (model, iter, COLUMN_BOOKMARK, bookmark.pp (), -1);
    return bookmark;
}


static void
set_bookmark (GtkListStore*     store,
              GtkTreeIter*      iter,
              objp<MooBookmark> bookmark)
{
    gtk_list_store_set (store, iter, COLUMN_BOOKMARK, bookmark.release(), -1);
}


static void
icon_data_func (G_GNUC_UNUSED GtkTreeViewColumn *column,
                GtkCellRenderer    *cell,
                GtkTreeModel       *model,
                GtkTreeIter        *iter)
{
    objp<MooBookmark> bookmark = get_bookmark (model, iter);

    if (!bookmark)
        g_object_set (cell,
                      "pixbuf", NULL,
                      "stock-id", NULL,
                      NULL);
    else
        g_object_set (cell,
                      "pixbuf", bookmark->pixbuf,
                      "stock-id", bookmark->icon_stock_id,
                      "stock-size", GTK_ICON_SIZE_MENU,
                      NULL);
}


static void
label_data_func (G_GNUC_UNUSED GtkTreeViewColumn *column,
                 GtkCellRenderer    *cell,
                 GtkTreeModel       *model,
                 GtkTreeIter        *iter)
{
    objp<MooBookmark> bookmark = get_bookmark (model, iter);

    if (!bookmark)
        g_object_set (cell,
                      "text", "-------",
                      "editable", FALSE,
                      NULL);
    else
        g_object_set (cell,
                      "text", bookmark->label,
                      "editable", TRUE,
                      NULL);
}


static void
path_data_func (G_GNUC_UNUSED GtkTreeViewColumn *column,
                GtkCellRenderer    *cell,
                GtkTreeModel       *model,
                GtkTreeIter        *iter)
{
    objp<MooBookmark> bookmark = get_bookmark (model, iter);

    if (!bookmark)
        g_object_set (cell,
                      "text", NULL,
                      "editable", FALSE,
                      NULL);
    else
        g_object_set (cell,
                      "text", bookmark->display_path,
                      "editable", TRUE,
                      NULL);
}


static void
selection_changed (GtkTreeSelection *selection,
                   BkEditorXml      *xml)
{
    GtkWidget *selected_hbox;
    int selected;

    selected_hbox = GTK_WIDGET (xml->selected_hbox);
    selected = gtk_tree_selection_count_selected_rows (selection);
    gtk_widget_set_sensitive (GTK_WIDGET (xml->delete_button), selected);

    if (selected == 1)
    {
        GtkTreeIter iter;
        GtkTreeModel *model;
        GList *rows = gtk_tree_selection_get_selected_rows (selection, &model);
        g_return_if_fail (rows != NULL);
        gtk_tree_model_get_iter (model, &iter, reinterpret_cast<GtkTreePath*> (rows->data));
        objp<MooBookmark> bookmark = get_bookmark (model, &iter);
        if (bookmark)
        {
            gtk_widget_set_sensitive (selected_hbox, TRUE);
            combo_update_icon (xml->icon_combo, xml);
        }
        else
        {
            gtk_widget_set_sensitive (selected_hbox, FALSE);
        }
    }
    else
    {
        gtk_widget_set_sensitive (selected_hbox, FALSE);
    }
}


static void
new_clicked (BkEditorXml *xml)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    GtkListStore *store;

    store = GTK_LIST_STORE (gtk_tree_view_get_model (xml->treeview));

    {
        auto bookmark = objp<MooBookmark>::make ("New bookmark", nullptr, MOO_STOCK_FOLDER);
        gtk_list_store_append (store, &iter);
        set_bookmark (store, &iter, std::move (bookmark));
    }

    column = GTK_TREE_VIEW_COLUMN (g_object_get_data (G_OBJECT (xml->treeview),
                                                      "moo-bookmarks-label-column"));
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
    gtk_tree_view_set_cursor (xml->treeview, path, column, TRUE);

    g_object_set_data (G_OBJECT (store),
                       "moo-bookmarks-modified",
                       GINT_TO_POINTER (TRUE));

    gtk_tree_path_free (path);
}


static void
delete_clicked (BkEditorXml *xml)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeSelection *selection;
    GtkListStore *store;
    GList *paths, *rows = NULL, *l;

    store = GTK_LIST_STORE (gtk_tree_view_get_model (xml->treeview));

    selection = gtk_tree_view_get_selection (xml->treeview);
    paths = gtk_tree_selection_get_selected_rows (selection, NULL);

    if (!paths)
        return;

    for (l = paths; l != NULL; l = l->next)
        rows = g_list_prepend (rows,
                               gtk_tree_row_reference_new (GTK_TREE_MODEL (store),
                                                           reinterpret_cast<GtkTreePath*> (l->data)));

    for (l = rows; l != NULL; l = l->next)
    {
        if (gtk_tree_row_reference_valid (reinterpret_cast<GtkTreeRowReference*> (l->data)))
        {
            path = gtk_tree_row_reference_get_path (reinterpret_cast<GtkTreeRowReference*> (l->data));
            gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
            gtk_list_store_remove (store, &iter);
            gtk_tree_path_free (path);
        }
    }

    g_object_set_data (G_OBJECT (store),
                       "moo-bookmarks-modified",
                       GINT_TO_POINTER (TRUE));

    g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
    g_list_foreach (rows, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free (paths);
    g_list_free (rows);
}


static void
separator_clicked (BkEditorXml *xml)
{
    GtkTreeIter iter;
    GtkListStore *store;
    store = GTK_LIST_STORE (gtk_tree_view_get_model (xml->treeview));
    gtk_list_store_append (store, &iter);
}


static void
label_edited (G_GNUC_UNUSED GtkCellRenderer *cell,
              char        *path_string,
              char        *text,
              BkEditorXml *xml)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkListStore *store;

    store = GTK_LIST_STORE (gtk_tree_view_get_model (xml->treeview));

    path = gtk_tree_path_new_from_string (path_string);

    if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
    {
        gtk_tree_path_free (path);
        return;
    }

    {
        objp<MooBookmark> bookmark = get_bookmark (GTK_TREE_MODEL (store), &iter);
        g_return_if_fail (bookmark != nullptr);

        if (bookmark->label.empty () || bookmark->label == text)
        {
            bookmark->label.set (text);
            set_bookmark (store, &iter, std::move (bookmark));
            g_object_set_data (G_OBJECT (store),
                               "moo-bookmarks-modified",
                               GINT_TO_POINTER (TRUE));
        }
    }

    gtk_tree_path_free (path);
}


static void
path_edited (G_GNUC_UNUSED GtkCellRenderer *cell,
             char        *path_string,
             char        *text,
             BkEditorXml *xml)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkListStore *store;
    MooFileEntryCompletion *cmpl;

    store = GTK_LIST_STORE (gtk_tree_view_get_model (xml->treeview));

    path = gtk_tree_path_new_from_string (path_string);

    if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
    {
        gtk_tree_path_free (path);
        return;
    }

    {
        objp<MooBookmark> bookmark = get_bookmark (GTK_TREE_MODEL (store), &iter);
        g_return_if_fail (bookmark != nullptr);

        if (bookmark->display_path != text)
        {
            _moo_bookmark_set_display_path (bookmark.get (), text);
            set_bookmark (store, &iter, std::move (bookmark));
            g_object_set_data (G_OBJECT (store),
                               "moo-bookmarks-modified",
                               GINT_TO_POINTER (TRUE));
        }
    }

    gtk_tree_path_free (path);

    cmpl = MOO_FILE_ENTRY_COMPLETION (g_object_get_data (G_OBJECT (cell), "moo-file-entry-completion"));
    g_return_if_fail (cmpl != NULL);
    g_object_set (cmpl, "entry", NULL, NULL);
}


static void
path_editing_started (GtkCellRenderer    *cell,
                      GtkCellEditable    *editable)
{
    MooFileEntryCompletion *cmpl =
            MOO_FILE_ENTRY_COMPLETION (g_object_get_data (G_OBJECT (cell), "moo-file-entry-completion"));

    g_return_if_fail (cmpl != NULL);
    g_return_if_fail (GTK_IS_ENTRY (editable));

    g_object_set (cmpl, "entry", editable, NULL);

#if 0
    if (!g_object_get_data (G_OBJECT (editable), "moo-stupid-entry-workaround"))
    {
        g_signal_connect (editable, "realize",
                          G_CALLBACK (path_entry_realize), NULL);
        g_signal_connect (editable, "unrealize",
                          G_CALLBACK (path_entry_unrealize), NULL);
        g_object_set_data (G_OBJECT (editable), "moo-stupid-entry-workaround",
                           GINT_TO_POINTER (TRUE));
    }
#endif
}


#if 0
static void
path_entry_realize (GtkWidget *entry)
{
    GtkSettings *settings;
    gboolean value;

    g_return_if_fail (gtk_widget_has_screen (entry));
    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (entry));
    g_return_if_fail (settings != NULL);

    g_object_get (settings, "gtk-entry-select-on-focus", &value, NULL);
    g_object_set (settings, "gtk-entry-select-on-focus", FALSE, NULL);
    g_object_set_data (G_OBJECT (settings),
                       "moo-stupid-entry-workaround",
                       GINT_TO_POINTER (TRUE));
    g_object_set_data (G_OBJECT (settings),
                       "moo-stupid-entry-workaround-value",
                       GINT_TO_POINTER (value));

    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
}


static void
path_entry_unrealize (GtkWidget *entry)
{
    GtkSettings *settings;
    gboolean value;

    g_return_if_fail (gtk_widget_has_screen (entry));
    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (entry));
    g_return_if_fail (settings != NULL);

    if (g_object_get_data (G_OBJECT (settings), "moo-stupid-entry-workaround"))
    {
        value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (settings),
                                 "moo-stupid-entry-workaround-value"));
        g_object_set (settings, "gtk-entry-select-on-focus", value, NULL);
        g_object_set_data (G_OBJECT (settings), "moo-stupid-entry-workaround", NULL);
    }

    g_signal_handlers_disconnect_by_func (entry,
                                          (gpointer) path_entry_realize,
                                          NULL);
    g_signal_handlers_disconnect_by_func (entry,
                                          (gpointer) path_entry_unrealize,
                                          NULL);
    g_object_set_data (G_OBJECT (entry), "moo-stupid-entry-workaround", NULL);
}
#endif


static void combo_icon_data_func    (GtkCellLayout      *cell_layout,
                                     GtkCellRenderer    *cell,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter);
static void combo_label_data_func   (GtkCellLayout      *cell_layout,
                                     GtkCellRenderer    *cell,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter);
static void fill_icon_store         (GtkListStore       *store,
                                     GtkStyle           *style);
static void icon_store_find_pixbuf  (GtkListStore       *store,
                                     GtkTreeIter        *iter,
                                     gobj_ref<GdkPixbuf> pixbuf);
static void icon_store_find_stock   (GtkListStore       *store,
                                     GtkTreeIter        *iter,
                                     const char         *stock);
static void icon_store_find_empty   (GtkListStore       *store,
                                     GtkTreeIter        *iter);
static void icon_combo_changed      (GtkComboBox        *combo,
                                     BkEditorXml        *xml);

static void
init_icon_combo (GtkComboBox *combo,
                 BkEditorXml *xml)
{
    static GtkListStore *icon_store;
    GtkCellRenderer *cell;

    if (!icon_store)
    {
        GtkWidget *dialog = GTK_WIDGET (xml->BkEditor);
        gtk_widget_ensure_style (dialog);
        icon_store = gtk_list_store_new (3, GDK_TYPE_PIXBUF,
                                         G_TYPE_STRING, G_TYPE_STRING);
        fill_icon_store (icon_store, dialog->style);
    }

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (icon_store));
    gtk_combo_box_set_wrap_width (combo, 3);

    cell = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), cell,
                                        (GtkCellLayoutDataFunc) combo_icon_data_func,
                                        NULL, NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), cell,
                                        (GtkCellLayoutDataFunc) combo_label_data_func,
                                        NULL, NULL);

    g_signal_connect (combo, "changed",
                      G_CALLBACK (icon_combo_changed), xml);
}


static void
combo_update_icon (GtkComboBox *combo,
                   BkEditorXml *xml)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GList *rows;
    GtkListStore *icon_store;

    selection = gtk_tree_view_get_selection (xml->treeview);
    rows = gtk_tree_selection_get_selected_rows (selection, &model);
    g_return_if_fail (rows != NULL && rows->next == NULL);

    gtk_tree_model_get_iter (model, &iter, reinterpret_cast<GtkTreePath*> (rows->data));
    objp<MooBookmark> bookmark = get_bookmark (model, &iter);
    g_return_if_fail (bookmark != nullptr);

    icon_store = GTK_LIST_STORE (gtk_combo_box_get_model (combo));

    if (bookmark->pixbuf)
        icon_store_find_pixbuf (icon_store, &iter, *bookmark->pixbuf);
    else if (!bookmark->icon_stock_id.empty())
        icon_store_find_stock (icon_store, &iter, bookmark->icon_stock_id);
    else
        icon_store_find_empty (icon_store, &iter);

    g_signal_handlers_block_by_func (combo, (gpointer) icon_combo_changed, xml);
    gtk_combo_box_set_active_iter (combo, &iter);
    g_signal_handlers_unblock_by_func (combo, (gpointer) icon_combo_changed, xml);

    gtk_tree_path_free (reinterpret_cast<GtkTreePath*> (rows->data));
    g_list_free (rows);
}


enum {
    ICON_COLUMN_PIXBUF = 0,
    ICON_COLUMN_STOCK  = 1,
    ICON_COLUMN_LABEL  = 2
};

static void
combo_icon_data_func (G_GNUC_UNUSED GtkCellLayout *cell_layout,
                      GtkCellRenderer    *cell,
                      GtkTreeModel       *model,
                      GtkTreeIter        *iter)
{
    char *stock = NULL;
    GdkPixbuf *pixbuf = NULL;

    gtk_tree_model_get (model, iter, ICON_COLUMN_PIXBUF, &pixbuf, -1);
    g_object_set (cell, "pixbuf", pixbuf, NULL);
    if (pixbuf)
    {
        g_object_unref (pixbuf);
        return;
    }

    gtk_tree_model_get (model, iter, ICON_COLUMN_STOCK, &stock, -1);
    g_object_set (cell, "stock-id", stock,
                  "stock-size", GTK_ICON_SIZE_MENU, NULL);
    g_free (stock);
}


static void
combo_label_data_func (G_GNUC_UNUSED GtkCellLayout *cell_layout,
                       GtkCellRenderer    *cell,
                       GtkTreeModel       *model,
                       GtkTreeIter        *iter)
{
    char *label = NULL;
    gtk_tree_model_get (model, iter, ICON_COLUMN_LABEL, &label, -1);
    g_object_set (cell, "text", label, NULL);
    g_free (label);
}


static void
fill_icon_store (GtkListStore       *store,
                 GtkStyle           *style)
{
    GtkTreeIter iter;
    GSList *stock_ids, *l;

    stock_ids = gtk_stock_list_ids ();

    for (l = stock_ids; l != NULL; l = l->next)
    {
        GtkStockItem item;

        if (!gtk_style_lookup_icon_set (style, reinterpret_cast<char*> (l->data)))
            continue;

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, ICON_COLUMN_STOCK,
                            l->data, -1);

        if (gtk_stock_lookup (reinterpret_cast<char*> (l->data), &item))
        {
            char *label = g_strdup (item.label);
            char *und = strchr (label, '_');

            if (und)
            {
                if (und[1] == 0)
                    *und = 0;
                else
                    memmove (und, und + 1, strlen (label) - (und - label));
            }

            gtk_list_store_set (store, &iter, ICON_COLUMN_LABEL,
                                label, -1);
            g_free (label);
        }
        else
        {
            gtk_list_store_set (store, &iter, ICON_COLUMN_LABEL,
                                l->data, -1);
        }
    }

    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                          ICON_COLUMN_LABEL,
                                          GTK_SORT_ASCENDING);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                          GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
                                          GTK_SORT_ASCENDING);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, ICON_COLUMN_LABEL, "None", -1);

    g_slist_foreach (stock_ids, (GFunc) extern_g_free, NULL);
    g_slist_free (stock_ids);
}


static void
icon_combo_changed (GtkComboBox *combo,
                    BkEditorXml *xml)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter, icon_iter;
    GList *rows;
    GtkTreeModel *icon_model;

    selection = gtk_tree_view_get_selection (xml->treeview);
    rows = gtk_tree_selection_get_selected_rows (selection, &model);
    g_return_if_fail (rows != NULL && rows->next == NULL);

    gtk_tree_model_get_iter (model, &iter, reinterpret_cast<GtkTreePath*> (rows->data));
    objp<MooBookmark> bookmark = get_bookmark (model, &iter);
    g_return_if_fail (bookmark != nullptr);

    gtk_combo_box_get_active_iter (combo, &icon_iter);
    icon_model = gtk_combo_box_get_model (combo);

    gobj_ptr<GdkPixbuf> pixbuf;
    gtk_tree_model_get (icon_model, &icon_iter, ICON_COLUMN_PIXBUF, pixbuf.pp (), -1);

    bookmark->pixbuf = pixbuf;
    bookmark->icon_stock_id.clear ();

    if (!pixbuf)
    {
        gstrp stock;
        gtk_tree_model_get (icon_model, &icon_iter, ICON_COLUMN_STOCK, stock.pp(), -1);
        bookmark->icon_stock_id = std::move (stock);
    }

    set_bookmark (GTK_LIST_STORE (model), &iter, std::move (bookmark));

    gtk_tree_path_free (reinterpret_cast<GtkTreePath*> (rows->data));
    g_list_free (rows);
}


static void
icon_store_find_pixbuf (GtkListStore       *store,
                        GtkTreeIter        *iter,
                        gobj_ref<GdkPixbuf> pixbuf)
{
    GtkTreeModel *model = GTK_TREE_MODEL (store);

    if (gtk_tree_model_get_iter_first (model, iter)) do
    {
        gobj_ptr<GdkPixbuf> pix;
        gtk_tree_model_get (model, iter, ICON_COLUMN_PIXBUF, pix.pp (), -1);

        if (pix == pixbuf.gobj())
            return;
    }
    while (gtk_tree_model_iter_next (model, iter));

    gtk_list_store_append (store, iter);
    gtk_list_store_set (store, iter, ICON_COLUMN_PIXBUF, pixbuf.gobj(), -1);
}


static void
icon_store_find_stock (GtkListStore       *store,
                       GtkTreeIter        *iter,
                       const char         *stock)
{
    GtkTreeModel *model = GTK_TREE_MODEL (store);

    g_return_if_fail (stock != NULL);

    if (gtk_tree_model_get_iter_first (model, iter)) do
    {
        char *id = NULL;
        gtk_tree_model_get (model, iter, ICON_COLUMN_STOCK, &id, -1);

        if (id && !strcmp (id, stock))
        {
            g_free (id);
            return;
        }

        g_free (id);
    }
    while (gtk_tree_model_iter_next (model, iter));

    gtk_list_store_append (store, iter);
    gtk_list_store_set (store, iter, ICON_COLUMN_STOCK, stock, -1);
}


static void
icon_store_find_empty (GtkListStore       *store,
                       GtkTreeIter        *iter)
{
    GtkTreeModel *model = GTK_TREE_MODEL (store);

    if (gtk_tree_model_get_iter_first (model, iter)) do
    {
        char *id = NULL;
        GdkPixbuf *pixbuf = NULL;

        gtk_tree_model_get (model, iter, ICON_COLUMN_STOCK, &id,
                            ICON_COLUMN_PIXBUF, &pixbuf, -1);

        if (!id && !pixbuf)
            return;

        g_free (id);
        if (pixbuf)
            g_object_unref (pixbuf);
    }
    while (gtk_tree_model_iter_next (model, iter));

    gtk_list_store_append (store, iter);
    gtk_list_store_set (store, iter, ICON_COLUMN_LABEL, "None", -1);
}