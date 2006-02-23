/*
 *   mooaction.c
 *
 *   Copyright (C) 2004-2006 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#include "mooutils/mooaction.h"
#include "mooutils/mooactiongroup.h"
#include "mooutils/mooaccel.h"
#include "mooutils/moomarshals.h"
#include "mooutils/moocompat.h"
#include "mooutils/mooutils-gobject.h"
#include <gtk/gtk.h>
#include <string.h>


static void moo_action_class_init       (MooActionClass *klass);

static void moo_action_init             (MooAction      *action);
static void moo_action_finalize         (GObject        *object);
static GObject *moo_action_constructor  (GType           type,
                                         guint           n_props,
                                         GObjectConstructParam *props);

static void moo_action_set_property     (GObject        *object,
                                         guint           prop_id,
                                         const GValue   *value,
                                         GParamSpec     *pspec);
static void moo_action_get_property     (GObject        *object,
                                         guint           prop_id,
                                         GValue         *value,
                                         GParamSpec     *pspec);

static void moo_action_activate_real    (MooAction      *action);

static void moo_action_set_closure      (MooAction      *action,
                                         MooClosure     *closure);
static void moo_action_set_label        (MooAction      *action,
                                         const char     *label);
static void moo_action_set_tooltip      (MooAction      *action,
                                         const char     *tooltip);
static void moo_action_set_icon_stock_id(MooAction      *action,
                                         const char     *stock_id);
static void moo_action_set_icon         (MooAction      *action,
                                         GdkPixbuf      *icon);

static void moo_action_set_sensitive_real (MooAction    *action,
                                         gboolean        sensitive);
static void moo_action_set_visible_real (MooAction      *action,
                                         gboolean        visible);

static GtkWidget *moo_action_create_menu_item_real (MooAction      *action);
static GtkWidget *moo_action_create_tool_item_real (MooAction      *action,
                                                    GtkWidget      *toolbar,
                                                    int             position,
                                                    MooToolItemFlags flags);

static void moo_action_add_proxy        (MooAction      *action,
                                         GtkWidget      *proxy);

#if GTK_MINOR_VERSION < 4
static void tool_item_callback (G_GNUC_UNUSED GtkWidget *widget,
                                MooAction *action)
{
    moo_action_activate (action);
}
#endif /* GTK_MINOR_VERSION < 4 */


enum {
    PROP_0,

    PROP_ID,
    PROP_NAME,
    PROP_GROUP_NAME,

    PROP_CLOSURE,
    PROP_CLOSURE_OBJECT,
    PROP_CLOSURE_SIGNAL,
    PROP_CLOSURE_CALLBACK,
    PROP_CLOSURE_PROXY_FUNC,

    PROP_STOCK_ID,
    PROP_LABEL,
    PROP_TOOLTIP,

    PROP_NO_ACCEL,
    PROP_ACCEL,
    PROP_FORCE_ACCEL_LABEL,

    PROP_ICON,
    PROP_ICON_STOCK_ID,

    PROP_DEAD,
    PROP_SENSITIVE,
    PROP_VISIBLE,
    PROP_IS_IMPORTANT
};

enum {
    ACTIVATE,
    SET_SENSITIVE,
    SET_VISIBLE,
    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL] = {0};


/* MOO_TYPE_ACTION */
G_DEFINE_TYPE (MooAction, moo_action, G_TYPE_OBJECT)


static void moo_action_class_init (MooActionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructor = moo_action_constructor;
    gobject_class->finalize = moo_action_finalize;
    gobject_class->set_property = moo_action_set_property;
    gobject_class->get_property = moo_action_get_property;

    klass->activate = moo_action_activate_real;
    klass->add_proxy = moo_action_add_proxy;
    klass->set_sensitive = moo_action_set_sensitive_real;
    klass->set_visible = moo_action_set_visible_real;
    klass->create_menu_item = moo_action_create_menu_item_real;
    klass->create_tool_item = moo_action_create_tool_item_real;

    g_object_class_install_property (gobject_class,
                                     PROP_ID,
                                     g_param_spec_string ("id",
                                             "id",
                                             "id",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_GROUP_NAME,
                                     g_param_spec_string ("group-name",
                                             "group-name",
                                             "group-name",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                             "name",
                                             "name",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_CLOSURE,
                                     g_param_spec_boxed ("closure",
                                             "closure",
                                             "closure",
                                             MOO_TYPE_CLOSURE,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STOCK_ID,
                                     g_param_spec_string ("stock-id",
                                             "stock-id",
                                             "stock-id",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_LABEL,
                                     g_param_spec_string ("label",
                                             "label",
                                             "label",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TOOLTIP,
                                     g_param_spec_string ("tooltip",
                                             "tooltip",
                                             "tooltip",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NO_ACCEL,
                                     g_param_spec_boolean ("no-accel",
                                             "no-accel",
                                             "no-accel",
                                             FALSE,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_FORCE_ACCEL_LABEL,
                                     g_param_spec_boolean ("force-accel-label",
                                             "force-accel-label",
                                             "force-accel-label",
                                             FALSE,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACCEL,
                                     g_param_spec_string ("accel",
                                             "accel",
                                             "accel",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SENSITIVE,
                                     g_param_spec_boolean ("sensitive",
                                             "sensitive",
                                             "sensitive",
                                             TRUE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_VISIBLE,
                                     g_param_spec_boolean ("visible",
                                             "visible",
                                             "visible",
                                             TRUE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_IS_IMPORTANT,
                                     g_param_spec_boolean ("is-important",
                                             "is-important",
                                             "is-important",
                                             FALSE,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DEAD,
                                     g_param_spec_boolean ("dead",
                                             "dead",
                                             "dead",
                                             FALSE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON_STOCK_ID,
                                     g_param_spec_string ("icon-stock-id",
                                             "icon-stock-id",
                                             "icon-stock-id",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON,
                                     g_param_spec_object ("icon",
                                             "icon",
                                             "icon",
                                             GDK_TYPE_PIXBUF,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_CLOSURE_OBJECT,
                                     g_param_spec_object ("closure-object",
                                             "closure-object",
                                             "closure-object",
                                             G_TYPE_OBJECT,
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class,
                                     PROP_CLOSURE_SIGNAL,
                                     g_param_spec_string ("closure-signal",
                                             "closure-signal",
                                             "closure-signal",
                                             NULL,
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class,
                                     PROP_CLOSURE_CALLBACK,
                                     g_param_spec_pointer ("closure-callback",
                                             "closure-callback",
                                             "closure-callback",
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class,
                                     PROP_CLOSURE_PROXY_FUNC,
                                     g_param_spec_pointer ("closure-proxy-func",
                                             "closure-proxy-func",
                                             "closure-proxy-func",
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    signals[ACTIVATE] =
        g_signal_new ("activate",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (MooActionClass, activate),
                      NULL, NULL,
                      _moo_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[SET_SENSITIVE] =
        g_signal_new ("set-sensitive",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (MooActionClass, set_sensitive),
                      NULL, NULL,
                      _moo_marshal_VOID__BOOL,
                      G_TYPE_NONE, 1,
                      G_TYPE_BOOLEAN);

    signals[SET_VISIBLE] =
        g_signal_new ("set-visible",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (MooActionClass, set_visible),
                      NULL, NULL,
                      _moo_marshal_VOID__BOOL,
                      G_TYPE_NONE, 1,
                      G_TYPE_BOOLEAN);
}


static void moo_action_init (MooAction *action)
{
    action->visible = TRUE;
    action->sensitive = TRUE;
}


static void moo_action_finalize       (GObject      *object)
{
    MooAction *action = MOO_ACTION (object);

    if (action->closure)
        moo_closure_unref (action->closure);
    g_free (action->id);
    g_free (action->name);
    g_free (action->group_name);
    g_free (action->stock_id);
    g_free (action->label);
    g_free (action->tooltip);
    g_free (action->icon_stock_id);
    g_free (action->default_accel);
    if (action->icon)
        g_object_unref (action->icon);

    G_OBJECT_CLASS (moo_action_parent_class)->finalize (object);
}


static GObject*
moo_action_constructor (GType           type,
                        guint           n_props,
                        GObjectConstructParam *props)
{
    GObject *object;
    MooAction *action;
    guint i;

    MooClosure *closure = NULL;
    gpointer closure_object = NULL;
    const char *closure_signal = NULL;
    GCallback closure_callback = NULL;
    GCallback closure_proxy_func = NULL;

    object = G_OBJECT_CLASS(moo_action_parent_class)->constructor (type, n_props, props);
    action = MOO_ACTION (object);

    if (action->dead)
        return object;

    for (i = 0; i < n_props; ++i)
    {
        const char *name = props[i].pspec->name;
        GValue *value = props[i].value;

        if (!strcmp (name, "closure-callback"))
            closure_callback = g_value_get_pointer (value);
        else if (!strcmp (name, "closure-signal"))
            closure_signal = g_value_get_string (value);
        else if (!strcmp (name, "closure-proxy-func"))
            closure_proxy_func = g_value_get_pointer (value);
        else if (!strcmp (name, "closure-object"))
            closure_object = g_value_get_object (value);
    }

    if (closure_callback || closure_signal)
    {
        if (closure_object)
            closure = moo_closure_new_simple (closure_object, closure_signal,
                                              closure_callback, closure_proxy_func);
        else
            g_warning ("%s: invalid closure params", G_STRLOC);
    }

    if (closure)
        moo_action_set_closure (action, closure);

    return object;
}


static void
moo_action_activate_real (MooAction *action)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    if (action->closure)
        moo_closure_invoke (action->closure);
}


static void
moo_action_get_property (GObject        *object,
                         guint           prop_id,
                         GValue         *value,
                         GParamSpec     *pspec)
{
    MooAction *action = MOO_ACTION (object);

    switch (prop_id)
    {
        case PROP_ID:
            g_value_set_string (value, action->id);
            break;
        case PROP_GROUP_NAME:
            g_value_set_string (value, action->group_name);
            break;
        case PROP_NAME:
            g_value_set_string (value, action->name);
            break;
        case PROP_SENSITIVE:
            g_value_set_boolean (value, action->sensitive != 0);
            break;
        case PROP_VISIBLE:
            g_value_set_boolean (value, action->visible != 0);
            break;
        case PROP_DEAD:
            g_value_set_boolean (value, action->dead != 0);
            break;
        case PROP_CLOSURE:
            g_value_set_boxed (value, action->closure);
            break;
        case PROP_STOCK_ID:
            g_value_set_string (value, action->stock_id);
            break;
        case PROP_LABEL:
            g_value_set_string (value, action->label);
            break;
        case PROP_TOOLTIP:
            g_value_set_string (value, action->tooltip);
            break;
        case PROP_NO_ACCEL:
            g_value_set_boolean (value, action->no_accel != 0);
            break;
        case PROP_FORCE_ACCEL_LABEL:
            g_value_set_boolean (value, action->force_accel_label != 0);
            break;
        case PROP_IS_IMPORTANT:
            g_value_set_boolean (value, action->is_important != 0);
            break;
        case PROP_ACCEL:
            g_value_set_string (value, action->default_accel);
            break;
        case PROP_ICON_STOCK_ID:
            g_value_set_string (value, action->icon_stock_id);
            break;
        case PROP_ICON:
            g_value_set_object (value, action->icon);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
moo_action_set_property (GObject        *object,
                         guint           prop_id,
                         const GValue   *value,
                         GParamSpec     *pspec)
{
    MooAction *action = MOO_ACTION (object);

    switch (prop_id)
    {
        case PROP_ID:
            g_free (action->id);
            action->id = g_strdup (g_value_get_string (value));
            g_object_notify (object, "id");
            break;
        case PROP_GROUP_NAME:
            g_free (action->group_name);
            action->group_name = g_strdup (g_value_get_string (value));
            g_object_notify (object, "group-name");
            break;
        case PROP_NAME:
            g_free (action->name);
            action->name = g_strdup (g_value_get_string (value));
            g_object_notify (object, "name");
            break;
        case PROP_SENSITIVE:
            moo_action_set_sensitive (action,
                                      g_value_get_boolean (value) != 0);
            break;
        case PROP_VISIBLE:
            moo_action_set_visible (action,
                                    g_value_get_boolean (value) != 0);
            break;
        case PROP_DEAD:
            action->dead = g_value_get_boolean (value) != 0;
            g_object_notify (object, "dead");
            break;
        case PROP_IS_IMPORTANT:
            action->is_important = g_value_get_boolean (value) != 0;
            g_object_notify (object, "is_important");
            break;
        case PROP_CLOSURE:
            moo_action_set_closure (action, g_value_get_boxed (value));
            break;
        case PROP_STOCK_ID:
            g_free (action->stock_id);
            action->stock_id = g_strdup (g_value_get_string (value));
            g_object_notify (G_OBJECT (action), "stock_id");
            break;
        case PROP_LABEL:
            moo_action_set_label (action,
                                  g_value_get_string (value));
            break;
        case PROP_TOOLTIP:
            moo_action_set_tooltip (action,
                                    g_value_get_string (value));
            break;
        case PROP_NO_ACCEL:
            action->no_accel = g_value_get_boolean (value) != 0;
            g_object_notify (object, "no-accel");
            break;
        case PROP_FORCE_ACCEL_LABEL:
            action->force_accel_label = g_value_get_boolean (value) != 0;
            g_object_notify (object, "force-accel-label");
            break;
        case PROP_ACCEL:
            g_free (action->default_accel);
            action->default_accel = g_strdup (g_value_get_string (value));
            g_object_notify (object, "accel");
            break;
        case PROP_ICON_STOCK_ID:
            moo_action_set_icon_stock_id (action,
                                          g_value_get_string (value));
            break;
        case PROP_ICON:
            moo_action_set_icon (action, GDK_PIXBUF (g_value_get_object (value)));
            break;

        case PROP_CLOSURE_OBJECT:
        case PROP_CLOSURE_SIGNAL:
        case PROP_CLOSURE_CALLBACK:
        case PROP_CLOSURE_PROXY_FUNC:
            /* constructor handles these */
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
moo_action_set_closure (MooAction      *action,
                        MooClosure     *closure)
{
    if (action->closure == closure)
        return;

    if (action->closure)
        moo_closure_unref (action->closure);

    action->closure = closure;

    if (action->closure)
    {
        moo_closure_ref (action->closure);
        moo_closure_sink (action->closure);
    }
}


static void
moo_action_set_label (MooAction      *action,
                      const char     *label)
{
    g_free (action->label);
    action->label = g_strdup (label);
}


static void
moo_action_set_tooltip (MooAction      *action,
                        const char     *tooltip)
{
    g_free (action->tooltip);
    action->tooltip = g_strdup (tooltip);
}


void
moo_action_set_no_accel (MooAction      *action,
                         gboolean        no_accel)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    action->no_accel = no_accel ? TRUE : FALSE;
}

gboolean
moo_action_get_no_accel (MooAction *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), TRUE);
    return action->no_accel ? TRUE : FALSE;
}


GtkWidget*
moo_action_create_menu_item (MooAction *action)
{
    MooActionClass *klass;

    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);
    g_return_val_if_fail (!action->dead, NULL);

    klass = MOO_ACTION_GET_CLASS (action);
    g_return_val_if_fail (klass != NULL && klass->create_menu_item != NULL, NULL);
    return klass->create_menu_item (action);
}


GtkWidget*
moo_action_create_tool_item (MooAction      *action,
                             GtkWidget      *toolbar,
                             int             position,
                             MooToolItemFlags flags)
{
    MooActionClass *klass;

    g_return_val_if_fail (MOO_IS_ACTION (action), FALSE);
    g_return_val_if_fail (!action->dead, FALSE);

    klass = MOO_ACTION_GET_CLASS (action);
    g_return_val_if_fail (klass != NULL && klass->create_tool_item != NULL, FALSE);
    return klass->create_tool_item (action, toolbar, position, flags);
}


const char*
moo_action_get_id (MooAction *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);
    return action->id;
}

const char*
moo_action_get_group_name (MooAction *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);
    return action->group_name;
}

const char*
moo_action_get_name (MooAction *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);
    return action->name;
}


const char*
moo_action_make_accel_path (const char     *group_id,
                            const char     *action_id)
{
    static GString *path = NULL;

    g_return_val_if_fail (action_id != NULL && action_id[0] != 0, NULL);

    if (!path)
        path = g_string_new (NULL);

    if (group_id && group_id[0])
        g_string_printf (path, "<MooAction>/%s/%s", group_id, action_id);
    else
        g_string_printf (path, "<MooAction>/%s", action_id);

    return path->str;
}


static GtkWidget*
moo_action_create_menu_item_real (MooAction *action)
{
    GtkWidget *item = NULL;

    if (action->dead)
    {
        return NULL;
    }
    else if (action->stock_id)
    {
        item = gtk_image_menu_item_new_from_stock (action->stock_id, NULL);
    }
    else
    {
        GtkWidget *icon = NULL;

        if (action->icon_stock_id)
        {
            icon = gtk_image_new_from_stock (action->icon_stock_id, GTK_ICON_SIZE_MENU);
            if (!icon) g_warning ("could not create stock icon '%s'",
                                action->icon_stock_id);
            else gtk_widget_show (icon);
        }

        item = gtk_image_menu_item_new_with_mnemonic (action->label);
        if (icon)
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), icon);
    }

    if (!action->no_accel)
        gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item),
                                      _moo_action_get_accel_path (action));

    if (action->force_accel_label)
    {
        GtkWidget *accel_label = gtk_bin_get_child (GTK_BIN (item));

        if (GTK_IS_ACCEL_LABEL (accel_label))
            moo_accel_label_set_action (accel_label, action);
        else
            g_critical ("%s: oops", G_STRLOC);
    }

    moo_action_add_proxy (action, item);

    return item;
}


static GtkWidget*
moo_action_create_tool_item_real (MooAction      *action,
                                  GtkWidget      *toolbar,
                                  int             position,
                                  MooToolItemFlags flags)
{
#if GTK_CHECK_VERSION(2,4,0)
    GtkToolItem *item = NULL;

    if (action->stock_id)
    {
#if GTK_CHECK_VERSION(2,6,0)
        if (flags & MOO_TOOL_ITEM_MENU)
            item = gtk_menu_tool_button_new_from_stock (action->stock_id);
        else
#endif
            item = gtk_tool_button_new_from_stock (action->stock_id);
    }
    else
    {
        GtkWidget *icon = NULL;

        if (action->icon_stock_id)
        {
            icon = gtk_image_new_from_stock (action->icon_stock_id,
                                             gtk_toolbar_get_icon_size (GTK_TOOLBAR (toolbar)));
            if (!icon)
                g_warning ("could not create stock icon '%s'",
                           action->icon_stock_id);
            else
                gtk_widget_show (icon);
        }

#if GTK_CHECK_VERSION(2,6,0)
        if (flags & MOO_TOOL_ITEM_MENU)
            item = gtk_menu_tool_button_new (icon, action->label);
        else
#endif
            item = gtk_tool_button_new (icon, action->label);

        gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
    }

    if (action->tooltip)
    {
        GtkTooltips *tooltips = gtk_tooltips_new ();
        gtk_object_sink (g_object_ref (tooltips));
        gtk_tool_item_set_tooltip (item,
                                   tooltips,
                                   action->tooltip,
                                   action->tooltip);
        g_object_set_data_full (G_OBJECT (item), "moo-tooltips",
                                tooltips, g_object_unref);
    }

    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, position);
    gtk_container_child_set (GTK_CONTAINER (toolbar), GTK_WIDGET (item),
                             "homogeneous", FALSE, NULL);

#else /*  !GTK_CHECK_VERSION(2,4,0) */

    GtkWidget *item = NULL;

    if (action->stock_id)
    {
        item = gtk_toolbar_insert_stock (toolbar,
                                         action->stock_id,
                                         action->tooltip,
                                         action->tooltip,
                                         (GtkSignalFunc)tool_item_callback,
                                         action,
                                         position);
    }
    else
    {
        GtkWidget *icon = NULL;

        if (action->icon_stock_id)
        {
            icon = gtk_image_new_from_stock (action->icon_stock_id,
                                             gtk_toolbar_get_icon_size (toolbar));
            if (!icon)
                g_warning ("could not create stock icon '%s'",
                           action->icon_stock_id);
            else
                gtk_widget_show (icon);
        }

        item = gtk_toolbar_insert_item (toolbar,
                                        action->label,
                                        action->tooltip,
                                        action->tooltip,
                                        icon,
                                        NULL,
                                        NULL,
                                        position);

        gtk_button_set_use_underline (GTK_BUTTON (item), TRUE);
    }

#endif /* !GTK_CHECK_VERSION(2,4,0) */

    moo_action_add_proxy (action, GTK_WIDGET (item));

    return GTK_WIDGET (item);
}


void
moo_action_activate (MooAction *action)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    g_signal_emit (action, signals[ACTIVATE], 0);
}


void
moo_action_set_sensitive (MooAction      *action,
                          gboolean        sensitive)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    g_signal_emit (action, signals[SET_SENSITIVE], 0, sensitive ? TRUE : FALSE);
}

void
moo_action_set_visible (MooAction      *action,
                        gboolean        visible)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    g_signal_emit (action, signals[SET_VISIBLE], 0, visible ? TRUE : FALSE);
}


static void
moo_action_set_sensitive_real (MooAction      *action,
                               gboolean        sensitive)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    action->sensitive = sensitive ? TRUE : FALSE;
    g_object_notify (G_OBJECT (action), "sensitive");
}

static void
moo_action_set_visible_real (MooAction      *action,
                             gboolean        visible)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    action->visible = visible ? TRUE : FALSE;
    g_object_notify (G_OBJECT (action), "visible");
}


static void
moo_action_set_icon_stock_id (MooAction      *action,
                              const char     *stock_id)
{
    if (action->icon_stock_id != stock_id)
    {
        g_free (action->icon_stock_id);
        action->icon_stock_id = g_strdup (stock_id);
        g_object_notify (G_OBJECT (action), "icon-stock-id");
    }
}


static void
moo_action_set_icon (G_GNUC_UNUSED MooAction      *action,
                     GdkPixbuf      *icon)
{
    if (icon)
        g_warning ("%s: implement me", G_STRLOC);
}


static void moo_action_add_proxy        (MooAction      *action,
                                         GtkWidget      *proxy)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    g_return_if_fail (G_IS_OBJECT (proxy));

    if (GTK_IS_MENU_ITEM (proxy))
    {
        moo_bind_signal (action, "activate", proxy, "activate");
    }
#if GTK_MINOR_VERSION >= 4
    else if (GTK_IS_TOOL_BUTTON (proxy))
    {
        moo_bind_signal (action, "activate", proxy, "clicked");
        moo_bind_bool_property (proxy, "is-important", action, "is-important", FALSE);
    }
#endif /* GTK_MINOR_VERSION >= 4 */
    else if (GTK_IS_BUTTON (proxy))
    {
        moo_bind_signal (action, "activate", proxy, "clicked");
    }
    else
    {
        g_warning ("%s: unknown proxy type '%s'", G_STRLOC,
                   g_type_name (G_OBJECT_TYPE (proxy)));
    }

    moo_bind_bool_property (proxy, "sensitive", action, "sensitive", FALSE);
    moo_bind_bool_property (proxy, "visible", action, "visible", FALSE);
}


const char*
moo_action_get_default_accel (MooAction      *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), "");

    if (!action->default_accel)
        action->default_accel = g_strdup ("");

    return action->default_accel;
}


void
_moo_action_set_accel_path (MooAction      *action,
                            const char     *accel_path)
{
    g_return_if_fail (MOO_IS_ACTION (action));
    g_free (action->accel_path);
    action->accel_path = g_strdup (accel_path);
}


const char*
_moo_action_get_accel_path (MooAction      *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);
    return action->accel_path;
}


const char*
_moo_action_get_accel (MooAction *action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), "");

    if (action->accel_path)
        return moo_get_accel (action->accel_path);
    else if (action->default_accel)
        return action->default_accel;
    else
        return "";
}


GType
moo_tool_item_flags_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        static const GFlagsValue values[] = {
            { MOO_TOOL_ITEM_MENU, (char*) "MOO_TOOL_ITEM_MENU", (char*) "menu" },
            { 0, NULL, NULL }
        };

        type = g_flags_register_static ("MooToolItemFlags", values);
    }

    return type;
}
