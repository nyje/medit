/*
 *   mooui/moouixml.c
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

#include "mooui/moouixml.h"
#include "mooutils/moocompat.h"
#include "mooutils/moomarshals.h"
#include <gtk/gtk.h>
#include <string.h>


/* XXX weak ref actions */

typedef MooUINodeType NodeType;
typedef MooUINode Node;
typedef MooUIWidgetNode Widget;
typedef MooUIItemNode Item;
typedef MooUISeparatorNode Separator;
typedef MooUIPlaceholderNode Placeholder;

#define CONTAINER MOO_UI_NODE_CONTAINER
#define WIDGET MOO_UI_NODE_WIDGET
#define TOOLBAR MOO_UI_NODE_TOOLBAR
#define ITEM MOO_UI_NODE_ITEM
#define SEPARATOR MOO_UI_NODE_SEPARATOR
#define PLACEHOLDER MOO_UI_NODE_PLACEHOLDER

static const char *NODE_TYPE_NAME[] = {
    NULL,
    "MOO_UI_NODE_CONTAINER",
    "MOO_UI_NODE_WIDGET",
    "MOO_UI_NODE_ITEM",
    "MOO_UI_NODE_SEPARATOR",
    "MOO_UI_NODE_PLACEHOLDER"
};

struct _MooUIXMLPrivate {
    Node *ui;
    GSList *toplevels;  /* Toplevel* */
    guint last_merge_id;
    GSList *merged_ui; /* Merge* */
};

typedef struct {
    Node *node;
    GtkWidget *widget;
    GHashTable *children; /* Node* -> GtkWidget* */
    MooActionGroup *actions;
    GtkAccelGroup *accel_group;
} Toplevel;

typedef struct {
    guint id;
    GSList *nodes;
} Merge;

typedef enum {
    UPDATE_ADD_NODE,
    UPDATE_REMOVE_NODE,
    UPDATE_CHANGE_NODE
} UpdateType;

#define TOPLEVEL_QUARK (toplevel_quark ())
#define NODE_QUARK (node_quark ())

#define SLIST_FOREACH(list,var)                     \
G_STMT_START {                                      \
    GSList *var;                                    \
    for (var = list; var != NULL; var = var->next)  \

#define SLIST_FOREACH_END                           \
} G_STMT_END


/* walking nodes stops when func returns FALSE */
typedef gboolean (*NodeForeachFunc) (Node       *node,
                                     gpointer    data);

static void     moo_ui_xml_finalize     (GObject        *object);

static void     xml_add_markup          (MooUIXML       *xml,
                                         MooMarkupNode  *mnode);

static void     update_widgets          (MooUIXML       *xml,
                                         UpdateType      type,
                                         Node           *node);

static Node    *parse_markup            (MooMarkupNode  *mnode);
static Node    *parse_object            (MooMarkupNode  *mnode);
static Node    *parse_widget            (MooMarkupNode  *mnode);
static Node    *parse_placeholder       (MooMarkupNode  *mnode);
static Node    *parse_item              (MooMarkupNode  *mnode);
static Node    *parse_separator         (MooMarkupNode  *mnode);
static gboolean node_check              (Node           *node);
static gboolean placeholder_check       (Node           *node);
static gboolean item_check              (Node           *node);
static gboolean widget_check            (Node           *node);
static gboolean container_check         (Node           *node);

static Merge   *lookup_merge            (MooUIXML       *xml,
                                         guint           id);

static Node    *node_new                (gsize           size,
                                         NodeType        type,
                                         const char     *name);
static Item    *item_new                (const char     *name,
                                         const char     *action,
                                         const char     *stock_id,
                                         const char     *label,
                                         const char     *icon_stock_id);
static gboolean node_is_ancestor        (Node           *node,
                                         Node           *ancestor);
static gboolean node_is_empty           (Node           *node);
static GSList  *node_list_children      (Node           *ndoe);
static GSList  *node_list_all_children  (Node           *ndoe);
static void     node_free               (Node           *node);
static void     node_foreach            (Node           *node,
                                         NodeForeachFunc func,
                                         gpointer        data);

static void     merge_add_node          (Merge          *merge,
                                         Node           *node);
static void     merge_remove_node       (Merge          *merge,
                                         Node           *node);

static Toplevel *toplevel_new           (Node           *node,
                                         MooActionGroup *actions,
                                         GtkAccelGroup  *accel_group);
static void     toplevel_free           (Toplevel       *toplevel);
static GtkWidget *toplevel_get_widget   (Toplevel       *toplevel,
                                         Node           *node);

static GQuark   toplevel_quark          (void);
static GQuark   node_quark              (void);

static void     xml_add_item_widget     (MooUIXML       *xml,
                                         GtkWidget      *widget);
static void     xml_add_widget          (MooUIXML       *xml,
                                         GtkWidget      *widget,
                                         Toplevel       *toplevel,
                                         Node           *node);
static void     xml_remove_widget       (MooUIXML       *xml,
                                         GtkWidget      *widget);
static void     xml_delete_toplevel     (MooUIXML       *xml,
                                         Toplevel       *toplevel);
static void     xml_connect_toplevel    (MooUIXML       *xml,
                                         Toplevel       *toplevel);

static gboolean create_menu_separator   (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         GtkMenuShell   *menu,
                                         Node           *node,
                                         int             index);
static void     create_menu_item        (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         GtkMenuShell   *menu,
                                         Node           *node,
                                         int             index);
static gboolean create_menu_shell       (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         MooUIWidgetType type);
static gboolean fill_menu_shell         (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         Node           *menu_node,
                                         GtkMenuShell   *menu);
static void     update_separators       (Node           *parent,
                                         Toplevel       *toplevel);
static gboolean create_tool_separator   (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         GtkToolbar     *toolbar,
                                         Node           *node,
                                         int             index);
static gboolean create_tool_item        (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         GtkToolbar     *toolbar,
                                         Node           *node,
                                         int             index);
static gboolean fill_toolbar            (MooUIXML       *xml,
                                         Toplevel       *toplevel,
                                         Node           *toolbar_node,
                                         GtkToolbar     *toolbar);
static gboolean create_toolbar          (MooUIXML       *xml,
                                         Toplevel       *toplevel);


/* MOO_TYPE_UI_XML */
G_DEFINE_TYPE (MooUIXML, moo_ui_xml, G_TYPE_OBJECT)


static void
moo_ui_xml_class_init (MooUIXMLClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = moo_ui_xml_finalize;
}


static Node*
node_new (gsize size, NodeType type, const char *name)
{
    Node *node = g_malloc0 (size);
    node->type = type;
    node->name = name ? g_strdup (name) : g_strdup ("");
    return node;
}

#define NODE_NEW(typename, type, name) (node_new (sizeof(typename), type, name))


static void
moo_ui_xml_init (MooUIXML *xml)
{
    xml->priv = g_new0 (MooUIXMLPrivate, 1);
    xml->priv->ui = NODE_NEW (Node, CONTAINER, "ui");
}


MooUIXML*
moo_ui_xml_new (void)
{
    return g_object_new (MOO_TYPE_UI_XML, NULL);
}


static Node*
parse_object (MooMarkupNode *mnode)
{
    Node *node;
    const char *name;
    MooMarkupNode *mchild;

    name = moo_markup_get_prop (mnode, "name");

    if (!name || !name[0])
    {
        g_warning ("%s: object name missing", G_STRLOC);
        return NULL;
    }

    node = NODE_NEW (Node, CONTAINER, name);

    for (mchild = mnode->children; mchild != NULL; mchild = mchild->next)
    {
        if (MOO_MARKUP_IS_ELEMENT (mchild))
        {
            Node *child = parse_markup (mchild);

            if (!child)
            {
                node_free (node);
                return NULL;
            }
            else
            {
                child->parent = node;
                node->children = g_slist_append (node->children, child);
            }
        }
    }

    return node;
}


static Node*
parse_widget (MooMarkupNode *mnode)
{
    Node *node;
    const char *name;
    MooMarkupNode *mchild;

    name = moo_markup_get_prop (mnode, "name");

    if (!name || !name[0])
    {
        g_warning ("%s: widget name missing", G_STRLOC);
        return NULL;
    }

    node = NODE_NEW (Widget, WIDGET, name);

    for (mchild = mnode->children; mchild != NULL; mchild = mchild->next)
    {
        if (MOO_MARKUP_IS_ELEMENT (mchild))
        {
            Node *child = parse_markup (mchild);

            if (!child)
            {
                node_free (node);
                return NULL;
            }
            else
            {
                child->parent = node;
                node->children = g_slist_append (node->children, child);
            }
        }
    }

    return node;
}


static Node*
parse_placeholder (MooMarkupNode *mnode)
{
    Node *node;
    const char *name;
    MooMarkupNode *mchild;

    name = moo_markup_get_prop (mnode, "name");

    if (!name || !name[0])
    {
        g_warning ("%s: placeholder name missing", G_STRLOC);
        return NULL;
    }

    node = NODE_NEW (Placeholder, PLACEHOLDER, name);

    for (mchild = mnode->children; mchild != NULL; mchild = mchild->next)
    {
        if (MOO_MARKUP_IS_ELEMENT (mchild))
        {
            Node *child = parse_markup (mchild);

            if (!child)
            {
                node_free (node);
                return NULL;
            }
            else
            {
                child->parent = node;
                node->children = g_slist_append (node->children, child);
            }
        }
    }

    return node;
}


static Item*
item_new (const char *name,
          const char *action,
          const char *stock_id,
          const char *label,
          const char *icon_stock_id)
{
    Item *item;

    item = (Item*) NODE_NEW (Item, ITEM, name);

    item->action = g_strdup (action);
    item->stock_id = g_strdup (stock_id);
    item->label = g_strdup (label);
    item->icon_stock_id = g_strdup (icon_stock_id);

    return item;
}


static Node*
parse_item (MooMarkupNode *mnode)
{
    Item *item;
    const char *name;
    MooMarkupNode *mchild;

    name = moo_markup_get_prop (mnode, "name");

    if (!name)
        name = moo_markup_get_prop (mnode, "action");

    item = item_new (name,
                     moo_markup_get_prop (mnode, "action"),
                     moo_markup_get_prop (mnode, "stock_id"),
                     moo_markup_get_prop (mnode, "label"),
                     moo_markup_get_prop (mnode, "icon_stock_id"));

    for (mchild = mnode->children; mchild != NULL; mchild = mchild->next)
    {
        if (MOO_MARKUP_IS_ELEMENT (mchild))
        {
            Node *child = parse_markup (mchild);

            if (!child)
            {
                node_free ((Node*) item);
                return NULL;
            }
            else
            {
                child->parent = (Node*) item;
                item->children = g_slist_append (item->children, child);
            }
        }
    }

    return (Node*) item;
}


static Node*
parse_separator (G_GNUC_UNUSED MooMarkupNode *mnode)
{
    return NODE_NEW (Separator, SEPARATOR, NULL);
}


static Node*
parse_markup (MooMarkupNode *mnode)
{
    g_return_val_if_fail (MOO_MARKUP_IS_ELEMENT (mnode), NULL);
    g_return_val_if_fail (mnode->name != NULL, NULL);

    if (!strcmp (mnode->name, "object"))
        return parse_object (mnode);
    else if (!strcmp (mnode->name, "widget"))
        return parse_widget (mnode);
    else if (!strcmp (mnode->name, "item"))
        return parse_item (mnode);
    else if (!strcmp (mnode->name, "separator"))
        return parse_separator (mnode);
    else if (!strcmp (mnode->name, "placeholder"))
        return parse_placeholder (mnode);

    g_warning ("%s: unknown element '%s'", G_STRLOC, mnode->name);
    return NULL;
}


static void
container_free (G_GNUC_UNUSED Node *node)
{
}

static void
widget_free (G_GNUC_UNUSED Node *node)
{
}

static void
separator_free (G_GNUC_UNUSED Node *node)
{
}

static void
placeholder_free (G_GNUC_UNUSED Node *node)
{
}

static void
item_free (Item *item)
{
    g_free (item->action);
    g_free (item->stock_id);
    g_free (item->label);
    g_free (item->icon_stock_id);
}

static void
node_free (Node *node)
{
    if (node)
    {
        g_slist_foreach (node->children, (GFunc) node_free, NULL);
        g_slist_free (node->children);
        node->children = NULL;

        switch (node->type)
        {
            case CONTAINER:
                container_free (node);
                break;
            case WIDGET:
                widget_free (node);
                break;
            case ITEM:
                item_free ((Item*) node);
                break;
            case SEPARATOR:
                separator_free (node);
                break;
            case PLACEHOLDER:
                placeholder_free (node);
                break;
        }

        g_free (node->name);
        g_free (node);
    }
}


void
moo_ui_xml_add_ui_from_string (MooUIXML       *xml,
                               const char     *buffer,
                               gssize          length)
{
    MooMarkupDoc *doc;
    MooMarkupNode *ui_node, *child;
    GError *error = NULL;

    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (buffer != NULL);

    doc = moo_markup_parse_memory (buffer, length, &error);

    if (!doc)
    {
        g_critical ("%s: could not parse markup", G_STRLOC);
        if (error)
        {
            g_critical ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
        return;
    }

    ui_node = moo_markup_get_root_element (doc, "ui");

    if (!ui_node)
        ui_node = MOO_MARKUP_NODE (doc);

    for (child = ui_node->children; child != NULL; child = child->next)
        if (MOO_MARKUP_IS_ELEMENT (child))
            xml_add_markup (xml, child);

    moo_markup_doc_unref (doc);
}


static gboolean
node_check (Node *node)
{
    g_return_val_if_fail (node != NULL, FALSE);

    switch (node->type)
    {
        case CONTAINER:
            return container_check (node);
        case WIDGET:
            return widget_check (node);
        case ITEM:
            return item_check (node);
        case SEPARATOR:
            return TRUE;
        case PLACEHOLDER:
            return placeholder_check (node);
    }

    g_return_val_if_reached (FALSE);
}


static gboolean
placeholder_check (Node *node)
{
    g_return_val_if_fail (node != NULL, FALSE);
    g_return_val_if_fail (node->type == PLACEHOLDER, FALSE);
    g_return_val_if_fail (node->name && node->name[0], FALSE);

    SLIST_FOREACH (node->children, l)
    {
        Node *child = l->data;

        switch (child->type)
        {
            case SEPARATOR:
            case ITEM:
            case PLACEHOLDER:
                if (!node_check (child))
                    return FALSE;
                break;

            default:
                g_warning ("%s: invalid placeholder child type %s",
                           G_STRLOC, NODE_TYPE_NAME[child->type]);
                return FALSE;
        }
    }
    SLIST_FOREACH_END;

    return TRUE;
}


static gboolean
item_check (Node *node)
{
    g_return_val_if_fail (node != NULL, FALSE);
    g_return_val_if_fail (node->type == ITEM, FALSE);
    g_return_val_if_fail (node->name != NULL, FALSE);

    SLIST_FOREACH (node->children, l)
    {
        Node *child = l->data;

        switch (child->type)
        {
            case SEPARATOR:
            case ITEM:
            case PLACEHOLDER:
                if (!node_check (child))
                    return FALSE;
                break;

            default:
                g_warning ("%s: invalid item child type %s",
                           G_STRLOC, NODE_TYPE_NAME[child->type]);
                return FALSE;
        }
    }
    SLIST_FOREACH_END;

    return TRUE;
}


static gboolean
widget_check (Node *node)
{
    g_return_val_if_fail (node != NULL, FALSE);
    g_return_val_if_fail (node->type == WIDGET, FALSE);
    g_return_val_if_fail (node->name && node->name[0], FALSE);

    SLIST_FOREACH (node->children, l)
    {
        Node *child = l->data;

        switch (child->type)
        {
            case SEPARATOR:
            case ITEM:
            case PLACEHOLDER:
                if (!node_check (child))
                    return FALSE;
                break;

            default:
                g_warning ("%s: invalid widget child type %s",
                           G_STRLOC, NODE_TYPE_NAME[child->type]);
                return FALSE;
        }
    }
    SLIST_FOREACH_END;

    return TRUE;
}


static gboolean
container_check (Node *node)
{
    g_return_val_if_fail (node != NULL, FALSE);
    g_return_val_if_fail (node->type == CONTAINER, FALSE);
    g_return_val_if_fail (node->name && node->name[0], FALSE);

    SLIST_FOREACH (node->children, l)
    {
        Node *child = l->data;

        switch (child->type)
        {
            case CONTAINER:
            case WIDGET:
                if (!node_check (child))
                    return FALSE;
                break;

            default:
                g_warning ("%s: invalid widget child type %s",
                           G_STRLOC, NODE_TYPE_NAME[child->type]);
                return FALSE;
        }
    }
    SLIST_FOREACH_END;

    return TRUE;
}


static void
xml_add_markup (MooUIXML       *xml,
                MooMarkupNode  *mnode)
{
    Node *node = parse_markup (mnode);

    if (!node)
        return;

    switch (node->type)
    {
        case CONTAINER:
            if (!container_check (node))
            {
                node_free (node);
                return;
            }
            break;

        case WIDGET:
            if (!widget_check (node))
            {
                node_free (node);
                return;
            }
            break;

        case ITEM:
        case SEPARATOR:
        case PLACEHOLDER:
            g_warning ("%s: invalid toplevel type %s",
                       G_STRLOC, NODE_TYPE_NAME[node->type]);
            node_free (node);
            return;
    }

    if (moo_ui_xml_get_node (xml, node->name))
    {
        g_warning ("%s: implement me?", G_STRLOC);
        node_free (node);
        return;
    }

    node->parent = xml->priv->ui;
    xml->priv->ui->children = g_slist_append (xml->priv->ui->children, node);
}


guint
moo_ui_xml_new_merge_id (MooUIXML *xml)
{
    Merge *merge;

    g_return_val_if_fail (MOO_IS_UI_XML (xml), 0);

    xml->priv->last_merge_id++;
    merge = g_new0 (Merge, 1);
    merge->id = xml->priv->last_merge_id;
    merge->nodes = NULL;

    xml->priv->merged_ui = g_slist_prepend (xml->priv->merged_ui, merge);

    return merge->id;
}


static Merge*
lookup_merge (MooUIXML *xml,
              guint     merge_id)
{
    GSList *l;

    for (l = xml->priv->merged_ui; l != NULL; l = l->next)
    {
        Merge *merge = l->data;
        if (merge->id == merge_id)
            return merge;
    }

    return NULL;
}


MooUINode*
moo_ui_xml_add_item (MooUIXML       *xml,
                     guint           merge_id,
                     const char     *parent_path,
                     const char     *name,
                     const char     *action,
                     int             position)
{
    Merge *merge;
    MooUINode *parent;
    Item *item;

    g_return_val_if_fail (MOO_IS_UI_XML (xml), NULL);
    g_return_val_if_fail (parent_path != NULL, NULL);

    if (!name || !name[0])
        name = action;

    merge = lookup_merge (xml, merge_id);
    g_return_val_if_fail (merge != NULL, NULL);

    parent = moo_ui_xml_get_node (xml, parent_path);
    g_return_val_if_fail (parent != NULL, NULL);

    switch (parent->type)
    {
        case MOO_UI_NODE_WIDGET:
        case MOO_UI_NODE_ITEM:
        case MOO_UI_NODE_PLACEHOLDER:
            break;

        case MOO_UI_NODE_CONTAINER:
        case MOO_UI_NODE_SEPARATOR:
            g_warning ("%s: can't add item to node of type %s",
                       G_STRLOC, NODE_TYPE_NAME[parent->type]);
    }

    item = item_new (name, action, NULL, NULL, NULL);
    item->parent = parent;
    parent->children = g_slist_insert (parent->children, item, position);

    merge_add_node (merge, (Node*) item);
    update_widgets (xml, UPDATE_ADD_NODE, (Node*) item);

    return (Node*) item;
}


void
moo_ui_xml_insert (MooUIXML       *xml,
                   guint           merge_id,
                   MooUINode      *parent,
                   int             position,
                   const char     *markup)
{
    Merge *merge;
    GError *error = NULL;
    MooMarkupDoc *doc;
    MooMarkupNode *mchild;
    int children_length;

    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (markup != NULL);
    g_return_if_fail (!parent || node_is_ancestor (parent, xml->priv->ui));

    merge = lookup_merge (xml, merge_id);
    g_return_if_fail (merge != NULL);

    if (!parent)
        parent = xml->priv->ui;

    if (parent->type == MOO_UI_NODE_SEPARATOR)
    {
        g_warning ("%s: can't add stuff to node of type %s",
                   G_STRLOC, NODE_TYPE_NAME[parent->type]);
        return;
    }

    doc = moo_markup_parse_memory (markup, -1, &error);

    if (!doc)
    {
        g_warning ("%s: could not parse markup", G_STRLOC);
        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
        return;
    }

    children_length = g_slist_length (parent->children);
    if (position < 0 || position > children_length)
        position = children_length;

    for (mchild = doc->last; mchild != NULL; mchild = mchild->prev)
    {
        Node *node;

        if (!MOO_MARKUP_IS_ELEMENT (mchild))
            continue;

        node = parse_markup (mchild);

        if (!node)
            continue;

        if (!node_check (node))
        {
            node_free (node);
            continue;
        }

        switch (node->type)
        {
            case WIDGET:
            case CONTAINER:
                if (parent->type != CONTAINER)
                {
                    g_warning ("%s: can not add node of type %s to node of type %s",
                               G_STRLOC, NODE_TYPE_NAME[node->type],
                               NODE_TYPE_NAME[parent->type]);
                    node_free (node);
                    continue;
                }
                break;

            case ITEM:
            case PLACEHOLDER:
            case SEPARATOR:
                if (parent->type == SEPARATOR || parent->type == CONTAINER)
                {
                    g_warning ("%s: can not add node of type %s to node of type %s",
                               G_STRLOC, NODE_TYPE_NAME[node->type],
                               NODE_TYPE_NAME[parent->type]);
                    node_free (node);
                    continue;
                }
                break;
        }

        /* XXX check names? */
        node->parent = parent;
        parent->children = g_slist_insert (parent->children, node, position);

        merge_add_node (merge, node);
        update_widgets (xml, UPDATE_ADD_NODE, node);
    }

    moo_markup_doc_unref (doc);
}


void
moo_ui_xml_insert_after (MooUIXML       *xml,
                         guint           merge_id,
                         MooUINode      *parent,
                         MooUINode      *after,
                         const char     *markup)
{
    int position;

    g_return_if_fail (MOO_IS_UI_XML (xml));

    if (!parent)
        parent = xml->priv->ui;

    g_return_if_fail (!after || after->parent == parent);

    if (!after)
        position = 0;
    else
        position = g_slist_index (parent->children, after) + 1;

    moo_ui_xml_insert (xml, merge_id, parent, position, markup);
}


void
moo_ui_xml_insert_before (MooUIXML       *xml,
                          guint           merge_id,
                          MooUINode      *parent,
                          MooUINode      *before,
                          const char     *markup)
{
    int position;

    g_return_if_fail (MOO_IS_UI_XML (xml));

    if (!parent)
        parent = xml->priv->ui;

    g_return_if_fail (!before || before->parent == parent);

    if (!before)
        position = g_slist_length (parent->children);
    else
        position = g_slist_index (parent->children, before);

    moo_ui_xml_insert (xml, merge_id, parent, position, markup);
}


void
moo_ui_xml_insert_markup_after (MooUIXML       *xml,
                                guint           merge_id,
                                const char     *parent_path,
                                const char     *after_name,
                                const char     *markup)
{
    Node *parent = NULL, *after = NULL;

    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (markup != NULL);

    if (parent_path)
    {
        parent = moo_ui_xml_get_node (xml, parent_path);
        g_return_if_fail (parent != NULL);
    }
    else
    {
        parent = xml->priv->ui;
    }

    if (after_name)
    {
        after = moo_ui_node_get_child (parent, after_name);
        g_return_if_fail (after != NULL);
    }

    moo_ui_xml_insert_after (xml, merge_id, parent, after, markup);
}


void
moo_ui_xml_insert_markup_before (MooUIXML       *xml,
                                 guint           merge_id,
                                 const char     *parent_path,
                                 const char     *before_name,
                                 const char     *markup)
{
    Node *parent = NULL, *before = NULL;

    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (markup != NULL);

    if (parent_path)
    {
        parent = moo_ui_xml_get_node (xml, parent_path);
        g_return_if_fail (parent != NULL);
    }
    else
    {
        parent = xml->priv->ui;
    }

    if (before_name)
    {
        before = moo_ui_node_get_child (parent, before_name);
        g_return_if_fail (before != NULL);
    }

    moo_ui_xml_insert_after (xml, merge_id, parent, before, markup);
}


void
moo_ui_xml_insert_markup (MooUIXML       *xml,
                          guint           merge_id,
                          const char     *parent_path,
                          int             position,
                          const char     *markup)
{
    Node *parent = NULL;

    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (markup != NULL);

    if (parent_path)
    {
        parent = moo_ui_xml_get_node (xml, parent_path);
        g_return_if_fail (parent != NULL);
    }
    else
    {
        parent = xml->priv->ui;
    }

    moo_ui_xml_insert (xml, merge_id, parent, position, markup);
}


static gboolean
node_is_ancestor (Node           *node,
                  Node           *ancestor)
{
    Node *n;

    g_return_val_if_fail (node != NULL, FALSE);
    g_return_val_if_fail (ancestor != NULL, FALSE);

    for (n = node; n != NULL; n = n->parent)
        if (n == ancestor)
            return TRUE;

    return FALSE;
}


void
moo_ui_xml_remove_ui (MooUIXML       *xml,
                      guint           merge_id)
{
    Merge *merge;
    GSList *nodes;

    g_return_if_fail (MOO_IS_UI_XML (xml));

    merge = lookup_merge (xml, merge_id);
    g_return_if_fail (merge != NULL);

    nodes = g_slist_copy (merge->nodes);

    SLIST_FOREACH (nodes, l)
    {
        moo_ui_xml_remove_node (xml, l->data);
    }
    SLIST_FOREACH_END;

    g_return_if_fail (merge->nodes == NULL);
    xml->priv->merged_ui = g_slist_remove (xml->priv->merged_ui, merge);
    g_free (merge);
}


void
moo_ui_xml_remove_node (MooUIXML       *xml,
                        MooUINode      *node)
{
    Node *parent;

    g_return_if_fail (MOO_IS_UI_XML (xml));
    g_return_if_fail (node != NULL);
    g_return_if_fail (node_is_ancestor (node, xml->priv->ui));

    SLIST_FOREACH (xml->priv->merged_ui, l)
    {
        Merge *merge = l->data;
        GSList *merge_nodes = g_slist_copy (merge->nodes);

        SLIST_FOREACH (merge_nodes, n)
        {
            Node *merge_node = n->data;
            if (node_is_ancestor (merge_node, node))
                merge_remove_node (merge, merge_node);
        }
        SLIST_FOREACH_END;

        g_slist_free (merge_nodes);
    }
    SLIST_FOREACH_END;

    update_widgets (xml, UPDATE_REMOVE_NODE, node);

    parent = node->parent;
    parent->children = g_slist_remove (parent->children, node);
    node->parent = NULL;

    SLIST_FOREACH (xml->priv->toplevels, l)
    {
        Toplevel *toplevel = l->data;
        if (node_is_ancestor (parent, toplevel->node))
            update_separators (parent, toplevel);
    }
    SLIST_FOREACH_END;

    node_free (node);
}


static void
merge_add_node (Merge *merge,
                Node  *added)
{
    g_return_if_fail (merge != NULL);
    g_return_if_fail (added != NULL);

    SLIST_FOREACH (merge->nodes, l)
    {
        Node *node = l->data;

        if (node_is_ancestor (added, node))
            return;
    }
    SLIST_FOREACH_END;

    merge->nodes = g_slist_prepend (merge->nodes, added);
}


static void
merge_remove_node (Merge          *merge,
                   Node           *removed)
{
    g_return_if_fail (merge != NULL);
    g_return_if_fail (removed != NULL);

    merge->nodes = g_slist_remove (merge->nodes, removed);
}


MooUINode*
moo_ui_xml_get_node (MooUIXML       *xml,
                     const char     *path)
{
    g_return_val_if_fail (MOO_IS_UI_XML (xml), NULL);
    g_return_val_if_fail (path != NULL, NULL);

    return moo_ui_node_get_child (xml->priv->ui, path);
}


static gboolean
find_placeholder_func (Node    *node,
                       gpointer user_data)
{
    struct {
        Node *found;
        const char *name;
    } *data = user_data;

    if (node->type != PLACEHOLDER)
        return TRUE;

    if (!strcmp (node->name, data->name))
    {
        data->found = node;
        return FALSE;
    }

    return TRUE;
}

MooUINode*
moo_ui_xml_find_placeholder (MooUIXML       *xml,
                             const char     *name)
{
    struct {
        Node *found;
        const char *name;
    } data;

    g_return_val_if_fail (MOO_IS_UI_XML (xml), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    data.found = NULL;
    data.name = name;
    node_foreach (xml->priv->ui, find_placeholder_func, &data);

    return data.found;
}


char*
moo_ui_node_get_path (MooUINode *node)
{
    GString *path;

    g_return_val_if_fail (node != NULL, NULL);

    path = g_string_new (node->name);

    while (node->parent && node->parent->parent)
    {
        node = node->parent;
        g_string_prepend_c (path, '/');
        g_string_prepend (path, node->name);
    }

    g_print ("path: %s\n", path->str);
    return g_string_free (path, FALSE);
}


MooUINode*
moo_ui_node_get_child (MooUINode      *node,
                       const char     *path)
{
    char **pieces, **p;

    g_return_val_if_fail (node != NULL, NULL);
    g_return_val_if_fail (path != NULL, NULL);

    pieces = g_strsplit (path, "/", 0);

    if (!pieces)
        return node;

    for (p = pieces; *p != NULL; ++p)
    {
        Node *child = NULL;

        if (!**p)
            continue;

        SLIST_FOREACH (node->children, l)
        {
            child = l->data;
            if (!strcmp (child->name, *p))
                break;
            else
                child = NULL;
        }
        SLIST_FOREACH_END;

        if (child)
        {
            node = child;
        }
        else
        {
            node = NULL;
            goto out;
        }
    }

out:
    g_strfreev (pieces);
    return node;
}


static Toplevel*
toplevel_new (Node           *node,
              MooActionGroup *actions,
              GtkAccelGroup  *accel_group)
{
    Toplevel *top;

    g_return_val_if_fail (node != NULL, NULL);

    top = g_new0 (Toplevel, 1);
    top->node = node;
    top->widget = NULL;
    top->children = g_hash_table_new (g_direct_hash, g_direct_equal);
    top->actions = actions;
    top->accel_group = accel_group;

    return top;
}


static void
toplevel_free (Toplevel *toplevel)
{
    if (toplevel)
    {
        g_hash_table_destroy (toplevel->children);
        g_free (toplevel);
    }
}


static GQuark toplevel_quark (void)
{
    static GQuark q = 0;
    if (!q)
        q = g_quark_from_static_string ("moo-ui-xml-toplevel");
    return q;
}

static GQuark node_quark (void)
{
    static GQuark q = 0;
    if (!q)
        q = g_quark_from_static_string ("moo-ui-xml-node");
    return q;
}


static void
visibility_notify (GtkWidget *widget,
                   G_GNUC_UNUSED gpointer whatever,
                   MooUIXML  *xml)
{
    Toplevel *toplevel;
    Node *node, *parent;

    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (MOO_IS_UI_XML (xml));

    toplevel = g_object_get_qdata (G_OBJECT (widget), TOPLEVEL_QUARK);
    g_return_if_fail (toplevel != NULL);

    node = g_object_get_qdata (G_OBJECT (widget), NODE_QUARK);
    g_return_if_fail (node != NULL && node->parent != NULL);
    g_return_if_fail (node->type == ITEM);

    parent = node->parent;
    while (parent->type == PLACEHOLDER)
        parent = parent->parent;

    /* XXX submenu */
    update_separators (parent, toplevel);
}

static void
xml_add_item_widget (MooUIXML       *xml,
                     GtkWidget      *widget)
{
    g_signal_connect (widget, "notify::visibility",
                      G_CALLBACK (visibility_notify), xml);
}


static void
widget_destroyed (GtkWidget *widget,
                  MooUIXML  *xml)
{
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (MOO_IS_UI_XML (xml));
    xml_remove_widget (xml, widget);
}


static void
xml_remove_widget (MooUIXML  *xml,
                   GtkWidget *widget)
{
    Toplevel *toplevel;
    Node *node;

    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (MOO_IS_UI_XML (xml));

    toplevel = g_object_get_qdata (G_OBJECT (widget), TOPLEVEL_QUARK);
    g_return_if_fail (toplevel != NULL);
    g_return_if_fail (toplevel->widget != widget);

    node = g_object_get_qdata (G_OBJECT (widget), NODE_QUARK);
    g_hash_table_remove (toplevel->children, node);

    g_object_set_qdata (G_OBJECT (widget), NODE_QUARK, NULL);
    g_object_set_qdata (G_OBJECT (widget), TOPLEVEL_QUARK, NULL);
    g_signal_handlers_disconnect_by_func (widget,
                                          (gpointer) widget_destroyed,
                                          xml);
    g_signal_handlers_disconnect_by_func (widget,
                                          (gpointer) visibility_notify,
                                          xml);
}


static void
xml_add_widget (MooUIXML  *xml,
                GtkWidget *widget,
                Toplevel  *toplevel,
                Node      *node)
{
    g_signal_connect (widget, "destroy",
                      G_CALLBACK (widget_destroyed), xml);
    g_object_set_qdata (G_OBJECT (widget), TOPLEVEL_QUARK, toplevel);
    g_object_set_qdata (G_OBJECT (widget), NODE_QUARK, node);
    g_hash_table_insert (toplevel->children, node, widget);
}


static void
prepend_value (G_GNUC_UNUSED gpointer key,
               gpointer value,
               GSList **list)
{
    *list = g_slist_prepend (*list, value);
}

static GSList*
hash_table_list_values (GHashTable *hash_table)
{
    GSList *list = NULL;
    g_hash_table_foreach (hash_table, (GHFunc) prepend_value, &list);
    return list;
}


static void
toplevel_destroyed (GtkWidget *widget,
                    MooUIXML  *xml)
{
    Toplevel *toplevel;

    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (MOO_IS_UI_XML (xml));

    toplevel = g_object_get_qdata (G_OBJECT (widget), TOPLEVEL_QUARK);
    g_return_if_fail (toplevel != NULL);
    g_return_if_fail (toplevel->widget == widget);

    xml_delete_toplevel (xml, toplevel);
}


static void
xml_delete_toplevel (MooUIXML *xml,
                     Toplevel *toplevel)
{
    GSList *children, *l;

    children = hash_table_list_values (toplevel->children);

    for (l = children; l != NULL; l = l->next)
    {
        GtkWidget *child = GTK_WIDGET (l->data);
        if (child != toplevel->widget)
            xml_remove_widget (xml, child);
    }

    g_signal_handlers_disconnect_by_func (toplevel->widget,
                                          (gpointer) toplevel_destroyed,
                                          xml);
    g_object_set_qdata (G_OBJECT (toplevel->widget), TOPLEVEL_QUARK, NULL);
    g_object_set_qdata (G_OBJECT (toplevel->widget), NODE_QUARK, NULL);
    xml->priv->toplevels = g_slist_remove (xml->priv->toplevels, toplevel);

    g_slist_free (children);
    toplevel_free (toplevel);
}


static void
xml_connect_toplevel (MooUIXML  *xml,
                      Toplevel  *toplevel)
{
    g_signal_connect (toplevel->widget, "destroy",
                      G_CALLBACK (toplevel_destroyed), xml);
    g_object_set_qdata (G_OBJECT (toplevel->widget), TOPLEVEL_QUARK, toplevel);
    g_object_set_qdata (G_OBJECT (toplevel->widget), NODE_QUARK, toplevel->node);
    g_hash_table_insert (toplevel->children, toplevel->node, toplevel->widget);
}


static gboolean
create_menu_separator (MooUIXML       *xml,
                       Toplevel       *toplevel,
                       GtkMenuShell   *menu,
                       Node           *node,
                       int             index)
{
    GtkWidget *item = gtk_separator_menu_item_new ();
    gtk_menu_shell_insert (menu, item, index);
    xml_add_widget (xml, item, toplevel, node);
    return TRUE;
}


static gboolean node_is_empty (Node *node)
{
    SLIST_FOREACH (node->children, l)
    {
        Node *child = l->data;

        if (child->type == SEPARATOR)
            continue;

        if (child->type == PLACEHOLDER)
        {
            if (!node_is_empty (child))
                return FALSE;
        }
        else
        {
            return FALSE;
        }
    }
    SLIST_FOREACH_END;

    return TRUE;
}


static void
create_menu_item (MooUIXML       *xml,
                  Toplevel       *toplevel,
                  GtkMenuShell   *menu,
                  Node           *node,
                  int             index)
{
    GtkWidget *menu_item = NULL;
    Item *item;

    g_return_if_fail (node != NULL && node->type == ITEM);

    item = (Item*) node;

    if (item->action)
    {
        MooAction *action;

        g_return_if_fail (toplevel->actions != NULL);

        action = moo_action_group_get_action (toplevel->actions, item->action);

        if (!action)
        {
            g_critical ("%s: could not find action '%s'",
                        G_STRLOC, item->action);
            return;
        }

        if (action->dead)
            return;

        menu_item = moo_action_create_menu_item (action);
    }
    else
    {
        if (item->stock_id)
        {
            menu_item = gtk_image_menu_item_new_from_stock (item->stock_id, NULL);
        }
        else if (item->label)
        {
            if (item->icon_stock_id)
            {
                GtkWidget *icon = gtk_image_new_from_stock (item->icon_stock_id,
                                                            GTK_ICON_SIZE_MENU);
                menu_item = gtk_image_menu_item_new_with_mnemonic (item->label);
                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), icon);
            }
            else
            {
                menu_item = gtk_menu_item_new_with_mnemonic (item->label);
            }
        }

        if (menu_item)
            gtk_widget_show (menu_item);
    }

    g_return_if_fail (menu_item != NULL);

    gtk_menu_shell_insert (menu, menu_item, index);
    xml_add_widget (xml, menu_item, toplevel, node);
    xml_add_item_widget (xml, menu_item);

    if (!node_is_empty (node))
    {
        GtkWidget *submenu = gtk_menu_new ();
        /* XXX empty menu */
        gtk_widget_show (submenu);
        gtk_menu_set_accel_group (GTK_MENU (submenu), toplevel->accel_group);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);
        fill_menu_shell (xml, toplevel, node,
                         GTK_MENU_SHELL (submenu));
    }
}


static GSList*
node_list_children (Node *parent)
{
    GSList *list = NULL, *l;

    for (l = parent->children; l != NULL; l = l->next)
    {
        GSList *tmp, *t;
        Node *node = l->data;

        switch (node->type)
        {
            case MOO_UI_NODE_ITEM:
            case MOO_UI_NODE_SEPARATOR:
                list = g_slist_prepend (list, node);
                break;
            case MOO_UI_NODE_PLACEHOLDER:
                tmp = node_list_children (node);
                for (t = tmp; t != NULL; t = t->next)
                    list = g_slist_prepend (list, t->data);
                g_slist_free (tmp);
                break;
            default:
                g_return_val_if_reached (g_slist_reverse (list));
        }
    }

    return g_slist_reverse (list);
}


static void
real_foreach (Node    *node,
              gpointer data)
{
    GSList *l;
    struct {
        NodeForeachFunc func;
        gpointer func_data;
        gboolean stop;
    } *foreach_data = data;

    if (foreach_data->stop)
        return;

    if (foreach_data->func (node, foreach_data->func_data))
    {
        foreach_data->stop = TRUE;
        return;
    }

    for (l = node->children; l != NULL; l = l->next)
    {
        Node *child = l->data;

        real_foreach (child, data);

        if (foreach_data->stop)
            return;
    }
}

static void
node_foreach (Node           *node,
              NodeForeachFunc func,
              gpointer        data)
{
    struct {
        NodeForeachFunc func;
        gpointer func_data;
        gboolean stop;
    } foreach_data = {func, data, FALSE};

    g_return_if_fail (node != NULL);
    g_return_if_fail (func != NULL);

    real_foreach (node, &foreach_data);
}


static GtkWidget*
toplevel_get_widget (Toplevel  *toplevel,
                     Node      *node)
{
    return g_hash_table_lookup (toplevel->children, node);
}


static void
update_separators (Node           *parent,
                   Toplevel       *toplevel)
{
    GSList *children, *l;
    Node *separator = NULL;
    gboolean first = TRUE;
    GtkWidget *widget;

    children = node_list_children (parent);

    for (l = children; l != NULL; l = l->next)
    {
        Node *node = l->data;

        switch (node->type)
        {
            case MOO_UI_NODE_ITEM:
                widget = toplevel_get_widget (toplevel, node);

                if (!widget || !GTK_WIDGET_VISIBLE (widget))
                    continue;

                if (!first)
                {
                    if (separator)
                    {
                        GtkWidget *sep_widget = toplevel_get_widget (toplevel, separator);
                        g_return_if_fail (sep_widget != NULL);
                        gtk_widget_show (sep_widget);
                    }
                }
                else
                {
                    first = FALSE;
                    separator = NULL;
                }
                break;

            case MOO_UI_NODE_SEPARATOR:
                widget = toplevel_get_widget (toplevel, node);
                g_return_if_fail (widget != NULL);
                gtk_widget_hide (widget);
                if (!first)
                    separator = node;
                break;

            default:
                g_return_if_reached ();
        }
    }

    g_slist_free (children);
}


static gboolean
fill_menu_shell (MooUIXML       *xml,
                 Toplevel       *toplevel,
                 Node           *menu_node,
                 GtkMenuShell   *menu)
{
    GSList *children;

    children = node_list_children (menu_node);

    SLIST_FOREACH (children, l)
    {
        Node *node = l->data;

        switch (node->type)
        {
            case MOO_UI_NODE_ITEM:
                create_menu_item (xml, toplevel, menu, node, -1);
                break;
            case MOO_UI_NODE_SEPARATOR:
                create_menu_separator (xml, toplevel, menu, node, -1);
                break;

            default:
                g_warning ("%s: invalid menu item type %s",
                           G_STRLOC, NODE_TYPE_NAME[node->type]);
                return FALSE;
        }
    }
    SLIST_FOREACH_END;

    update_separators (menu_node, toplevel);

    return TRUE;
}


static gboolean
create_menu_shell (MooUIXML       *xml,
                   Toplevel       *toplevel,
                   MooUIWidgetType type)
{
    g_return_val_if_fail (toplevel != NULL, FALSE);
    g_return_val_if_fail (toplevel->widget == NULL, FALSE);
    g_return_val_if_fail (toplevel->node != NULL, FALSE);

    if (type == MOO_UI_MENUBAR)
    {
        toplevel->widget = gtk_menu_bar_new ();
    }
    else
    {
        toplevel->widget = gtk_menu_new ();
        gtk_menu_set_accel_group (GTK_MENU (toplevel->widget),
                                  toplevel->accel_group);
    }

    xml_connect_toplevel (xml, toplevel);

    return fill_menu_shell (xml, toplevel,
                            toplevel->node,
                            GTK_MENU_SHELL (toplevel->widget));
}


static gboolean
create_tool_separator (MooUIXML       *xml,
                       Toplevel       *toplevel,
                       GtkToolbar     *toolbar,
                       Node           *node,
                       int             index)
{
    GtkToolItem *item = gtk_separator_tool_item_new ();
    gtk_toolbar_insert (toolbar, item, index);
    xml_add_widget (xml, GTK_WIDGET (item), toplevel, node);
    return TRUE;
}


static gboolean
create_tool_item (MooUIXML       *xml,
                  Toplevel       *toplevel,
                  GtkToolbar     *toolbar,
                  Node           *node,
                  int             index)
{
    GtkWidget *tool_item = NULL;
    Item *item;

    g_return_val_if_fail (node != NULL && node->type == ITEM, FALSE);

    item = (Item*) node;

    if (item->action)
    {
        MooAction *action;

        g_return_val_if_fail (toplevel->actions != NULL, FALSE);

        action = moo_action_group_get_action (toplevel->actions, item->action);
        g_return_val_if_fail (action != NULL, FALSE);

        if (action->dead)
            return TRUE;

        tool_item = moo_action_create_tool_item (action, toolbar, index);
    }
    else
    {
        g_warning ("%s: implement me", G_STRLOC);
        return FALSE;
    }

    g_return_val_if_fail (tool_item != NULL, FALSE);

    xml_add_widget (xml, tool_item, toplevel, node);
    xml_add_item_widget (xml, tool_item);

    return TRUE;
}


static gboolean
fill_toolbar (MooUIXML       *xml,
              Toplevel       *toplevel,
              Node           *toolbar_node,
              GtkToolbar     *toolbar)
{
    gboolean result = TRUE;
    GSList *children;

    children = node_list_children (toolbar_node);

    SLIST_FOREACH (children, l)
    {
        Node *node = l->data;

        switch (node->type)
        {
            case MOO_UI_NODE_ITEM:
                result = create_tool_item (xml, toplevel, toolbar, node, -1);
                break;
            case MOO_UI_NODE_SEPARATOR:
                create_tool_separator (xml, toplevel, toolbar, node, -1);
                break;

            default:
                g_warning ("%s: invalid tool item type %s",
                           G_STRLOC, NODE_TYPE_NAME[node->type]);
                return FALSE;
        }

        if (!result)
            return FALSE;
    }
    SLIST_FOREACH_END;

    update_separators (toolbar_node, toplevel);

    return TRUE;
}


static gboolean
create_toolbar (MooUIXML       *xml,
                Toplevel       *toplevel)
{
    g_return_val_if_fail (toplevel != NULL, FALSE);
    g_return_val_if_fail (toplevel->widget == NULL, FALSE);
    g_return_val_if_fail (toplevel->node != NULL, FALSE);

    toplevel->widget = gtk_toolbar_new ();
    xml_connect_toplevel (xml, toplevel);

    return fill_toolbar (xml, toplevel,
                         toplevel->node,
                         GTK_TOOLBAR (toplevel->widget));
}


GtkWidget*
moo_ui_xml_create_widget (MooUIXML       *xml,
                          MooUIWidgetType type,
                          const char     *path,
                          MooActionGroup *actions,
                          GtkAccelGroup  *accel_group)
{
    Node *node;
    Toplevel *toplevel;
    gboolean result;

    g_return_val_if_fail (MOO_IS_UI_XML (xml), NULL);
    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (!actions || MOO_IS_ACTION_GROUP (actions), NULL);

    node = moo_ui_xml_get_node (xml, path);
    g_return_val_if_fail (node != NULL, NULL);

    if (node->type != WIDGET)
    {
        g_warning ("%s: can create widgets only for nodes of type %s",
                   G_STRLOC, NODE_TYPE_NAME[WIDGET]);
        return NULL;
    }

    if (type < 1 || type > 3)
    {
        g_warning ("%s: invalid widget type %d", G_STRLOC, type);
        return NULL;
    }

    toplevel = toplevel_new (node, actions, accel_group);
    xml->priv->toplevels = g_slist_append (xml->priv->toplevels, toplevel);

    switch (type)
    {
        case MOO_UI_MENUBAR:
            result = create_menu_shell (xml, toplevel, MOO_UI_MENUBAR);
            break;
        case MOO_UI_MENU:
            result = create_menu_shell (xml, toplevel, MOO_UI_MENU);
            break;
        case MOO_UI_TOOLBAR:
            result = create_toolbar (xml, toplevel);
            break;
    }

    if (!result)
    {
        xml_delete_toplevel (xml, toplevel);
        return NULL;
    }

    return toplevel->widget;
}


static Node*
effective_parent (Node *node)
{
    Node *parent;
    g_return_val_if_fail (node != NULL, NULL);
    parent = node->parent;
    while (parent && parent->type == PLACEHOLDER)
        parent = parent->parent;
    return parent;
}


static int
effective_index (Node *parent,
                 Node *node)
{
    GSList *children;
    int index;
    g_return_val_if_fail (effective_parent (node) == parent, -1);
    children = node_list_children (parent);
    index = g_slist_index (children, node);
    g_slist_free (children);
    g_return_val_if_fail (index >= 0, -1);
    return index;
}


static void
toplevel_add_node (MooUIXML *xml,
                   Toplevel *toplevel,
                   Node     *node)
{
    g_return_if_fail (GTK_IS_WIDGET (toplevel->widget));
    g_return_if_fail (node->type == ITEM);
    g_return_if_fail (node_is_ancestor (node, toplevel->node));

    if (GTK_IS_TOOLBAR (toplevel->widget))
    {
        g_return_if_fail (effective_parent (node) == toplevel->node);
        create_tool_item (xml, toplevel,
                          GTK_TOOLBAR (toplevel->widget),
                          node, effective_index (toplevel->node, node));
        update_separators (toplevel->node, toplevel);
    }
    else if (GTK_IS_MENU_SHELL (toplevel->widget))
    {
        GtkWidget *parent_widget, *menu_shell;
        Node *parent = effective_parent (node);

        g_return_if_fail (parent != NULL);
        parent_widget = toplevel_get_widget (toplevel, parent);
        g_return_if_fail (parent_widget != NULL);

        if (GTK_IS_MENU_SHELL (parent_widget))
        {
            menu_shell = parent_widget;
        }
        else
        {
            g_return_if_fail (GTK_IS_MENU_ITEM (parent_widget));
            menu_shell = gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent_widget));
            if (!menu_shell)
            {
                menu_shell = gtk_menu_new ();
                gtk_widget_show (menu_shell);
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (parent_widget), menu_shell);
            }
        }

        create_menu_item (xml, toplevel, GTK_MENU_SHELL (menu_shell),
                          node, effective_index (parent, node));
        update_separators (parent, toplevel);
    }
    else
    {
        g_return_if_reached ();
    }
}


static GSList*
node_list_all_children (Node *node)
{
    GSList *list, *l;

    g_return_val_if_fail (node != NULL, NULL);

    list = g_slist_append (NULL, node);

    for (l = node->children; l != NULL; l = l->next)
        list = g_slist_append (list, node_list_all_children (l->data));

    return list;
}


static void
toplevel_remove_node (G_GNUC_UNUSED MooUIXML *xml,
                      Toplevel *toplevel,
                      Node     *node)
{
    GSList *children, *l;

    g_return_if_fail (node != toplevel->node);
    g_return_if_fail (node_is_ancestor (node, toplevel->node));

    children = node_list_all_children (node);

    for (l = children; l != NULL; l = l->next)
    {
        GtkWidget *widget = g_hash_table_lookup (toplevel->children, node);

        if (widget)
            gtk_widget_destroy (widget);
    }

    g_slist_free (children);
}


static void
update_widgets (MooUIXML       *xml,
                UpdateType      type,
                Node           *node)
{
    switch (type)
    {
        case UPDATE_ADD_NODE:
            SLIST_FOREACH (xml->priv->toplevels, l)
            {
                Toplevel *toplevel = l->data;

                if (node_is_ancestor (node, toplevel->node))
                    toplevel_add_node (xml, toplevel, node);
            }
            SLIST_FOREACH_END;
            break;

        case UPDATE_REMOVE_NODE:
            SLIST_FOREACH (xml->priv->toplevels, l)
            {
                Toplevel *toplevel = l->data;

                if (node_is_ancestor (toplevel->node, node))
                    xml_delete_toplevel (xml, toplevel);
                else if (node_is_ancestor (node, toplevel->node))
                    toplevel_remove_node (xml, toplevel, node);
            }
            SLIST_FOREACH_END;
            break;

        case UPDATE_CHANGE_NODE:
            g_warning ("%s: implement me", G_STRLOC);
            break;

        default:
            g_return_if_reached ();
    }
}


static void
moo_ui_xml_finalize (GObject *object)
{
    MooUIXML *xml = MOO_UI_XML (object);

    SLIST_FOREACH (xml->priv->toplevels, t)
    {
        Toplevel *toplevel = t->data;
        GSList *widgets = hash_table_list_values (toplevel->children);

        SLIST_FOREACH (widgets, w)
        {
            GObject *widget = G_OBJECT (w->data);

            g_object_set_qdata (widget, NODE_QUARK, NULL);
            g_object_set_qdata (widget, TOPLEVEL_QUARK, NULL);
            g_signal_handlers_disconnect_by_func (widget, (gpointer) widget_destroyed, xml);
            g_signal_handlers_disconnect_by_func (widget, (gpointer) visibility_notify, xml);
        }
        SLIST_FOREACH_END;

        g_slist_free (widgets);
        toplevel_free (toplevel);
    }
    SLIST_FOREACH_END;

    SLIST_FOREACH (xml->priv->merged_ui, m)
    {
        Merge *merge = m->data;
        g_slist_free (merge->nodes);
        g_free (merge);
    }
    SLIST_FOREACH_END;

    g_slist_free (xml->priv->toplevels);
    g_slist_free (xml->priv->merged_ui);
    node_free (xml->priv->ui);

    g_free (xml->priv);
    xml->priv = NULL;

    G_OBJECT_CLASS(moo_ui_xml_parent_class)->finalize (object);
}
