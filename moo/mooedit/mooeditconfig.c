/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   mooeditconfig.c
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

#include "mooedit/mooeditconfig.h"
#include "mooutils/mooutils-gobject.h"
#include <gobject/gvaluecollector.h>
#include <string.h>


#define VALUE(c_,i_)  (&(c_)->priv->values[i_])
#define GVALUE(c_,i_) (&VALUE(c_,i_)->gval)


typedef struct _Variable Variable;
typedef struct _VarArray VarArray;
typedef struct _Value    Value;


struct _MooEditConfigPrivate {
    Value *values;
    guint n_values;
    guint n_values_allocd__;
};

struct _Value {
    GValue gval;
    guint source : 16;
};

struct _Variable {
    GParamSpec *pspec;
};

struct _VarArray {
    Variable *data;
    guint len;
};


static MooEditConfig *global;
static GSList *instances;
static VarArray *vars;
static GQuark prop_id_quark;
static GHashTable *aliases;


static void moo_edit_config_finalize        (GObject        *object);

static void moo_edit_config_set_property    (GObject        *object,
                                             guint           prop_id,
                                             const GValue   *value,
                                             GParamSpec     *pspec);
static void moo_edit_config_get_property    (GObject        *object,
                                             guint           prop_id,
                                             GValue         *value,
                                             GParamSpec     *pspec);

static void global_changed                  (GObject        *object,
                                             GParamSpec     *pspec);


/* MOO_TYPE_EDIT_CONFIG */
G_DEFINE_TYPE (MooEditConfig, moo_edit_config, G_TYPE_OBJECT)


static void
moo_edit_config_class_init (MooEditConfigClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = moo_edit_config_set_property;
    gobject_class->get_property = moo_edit_config_get_property;
    gobject_class->finalize = moo_edit_config_finalize;

    prop_id_quark = g_quark_from_static_string ("MooEditConfigPropId");
    aliases = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    vars = (VarArray*) g_array_new (FALSE, TRUE, sizeof (Variable));
    g_array_set_size ((GArray*) vars, 1);
}


static void
moo_edit_config_init (MooEditConfig *config)
{
    guint i;

    config->priv = g_new0 (MooEditConfigPrivate, 1);
    config->priv->values = g_new0 (Value, vars->len);
    config->priv->n_values = config->priv->n_values_allocd__ = vars->len;

    if (global)
    {
        instances = g_slist_prepend (instances, config);

        for (i = 1; i < vars->len; ++i)
        {
            g_value_init (GVALUE (config, i), G_VALUE_TYPE (GVALUE (global, i)));
            g_value_copy (GVALUE (global, i), GVALUE (config, i));
            VALUE(config, i)->source = VALUE(global, i)->source;
        }
    }
}


static void
moo_edit_config_finalize (GObject *object)
{
    guint i;
    MooEditConfig *config = MOO_EDIT_CONFIG (object);

    instances = g_slist_remove (instances, config);

    for (i = 1; i < config->priv->n_values; ++i)
        g_value_unset (GVALUE (config, i));

    g_free (config->priv->values);
    g_free (config->priv);

    G_OBJECT_CLASS (moo_edit_config_parent_class)->finalize (object);
}


static void
global_init (void)
{
    if (!global)
    {
        guint i;

        global = g_object_new (MOO_TYPE_EDIT_CONFIG, NULL);

        for (i = 1; i < vars->len; ++i)
        {
            g_value_init (GVALUE (global, i), G_PARAM_SPEC_VALUE_TYPE (vars->data[i].pspec));
            g_param_value_set_default (vars->data[i].pspec, GVALUE (global, i));
            VALUE(global, i)->source = MOO_EDIT_CONFIG_SOURCE_AUTO;
        }

        g_signal_connect (global, "notify",
                          G_CALLBACK (global_changed), NULL);
        /* XXX read preferences here */
    }
}


MooEditConfig *
moo_edit_config_new (void)
{
    global_init ();
    return g_object_new (MOO_TYPE_EDIT_CONFIG, NULL);
}


static void
moo_edit_config_set_property (GObject        *object,
                              guint           prop_id,
                              const GValue   *value,
                              GParamSpec     *pspec)
{
    MooEditConfig *config = MOO_EDIT_CONFIG (object);

    if (prop_id == 0 || prop_id >= vars->len)
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        return;
    }

    g_assert (config->priv->n_values == vars->len);

    g_value_copy (value, GVALUE (config, prop_id));
    VALUE (config, prop_id)->source = MOO_EDIT_CONFIG_SOURCE_USER;
    g_object_notify (object, pspec->name);
}


static void
moo_edit_config_get_property (GObject        *object,
                              guint           prop_id,
                              GValue         *value,
                              GParamSpec     *pspec)
{
    MooEditConfig *config = MOO_EDIT_CONFIG (object);

    if (prop_id == 0 || prop_id >= vars->len)
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        return;
    }

    g_assert (config->priv->n_values == vars->len);

    g_value_copy (GVALUE (config, prop_id), value);
}


static void
object_notify (MooEditConfig *config,
               guint          prop_id)
{
    g_object_notify (G_OBJECT (config),
                     vars->data[prop_id].pspec->name);
}


static void
update_prop_from_global (MooEditConfig *config,
                         gpointer       data)
{
    guint prop_id = GPOINTER_TO_UINT (data);

    g_assert (prop_id > 0 && prop_id < vars->len);
    g_assert (prop_id <= config->priv->n_values);

    if (prop_id == config->priv->n_values)
    {
        if (config->priv->n_values_allocd__ == prop_id)
        {
            Value *tmp;
            config->priv->n_values_allocd__ = MAX (config->priv->n_values_allocd__ * 1.2,
                                                   config->priv->n_values_allocd__ + 2);
            tmp = g_new0 (Value, config->priv->n_values_allocd__);
            memcpy (tmp, config->priv->values, config->priv->n_values * sizeof (Value));
            g_free (config->priv->values);
            config->priv->values = tmp;
        }

        config->priv->n_values += 1;
        g_value_init (GVALUE (config, prop_id), G_VALUE_TYPE (GVALUE (global, prop_id)));
        VALUE(config, prop_id)->source = VALUE(global, prop_id)->source;
    }

    if (VALUE(global, prop_id)->source <= VALUE(config, prop_id)->source)
    {
        g_value_copy (GVALUE(global, prop_id), GVALUE(config, prop_id));
        VALUE(config, prop_id)->source = VALUE(global, prop_id)->source;
        object_notify (config, prop_id);
    }
}

static void
global_changed (G_GNUC_UNUSED GObject *object,
                GParamSpec     *pspec)
{
    gpointer prop_id;

    g_assert (MOO_EDIT_CONFIG (object) == global);

    prop_id = g_param_spec_get_qdata (pspec, prop_id_quark);
    g_return_if_fail (prop_id != NULL);

    g_slist_foreach (instances, (GFunc) g_object_freeze_notify, NULL);
    g_slist_foreach (instances, (GFunc) update_prop_from_global, prop_id);
    g_slist_foreach (instances, (GFunc) g_object_thaw_notify, NULL);

    /* XXX write to preferences here */
}


static void
global_add_prop (GParamSpec *pspec,
                 guint       prop_id)
{
    g_assert (global->priv->n_values == prop_id);

    if (global->priv->n_values_allocd__ == prop_id)
    {
        Value *tmp;
        global->priv->n_values_allocd__ = MAX (global->priv->n_values_allocd__ * 1.2,
                                               global->priv->n_values_allocd__ + 2);
        tmp = g_new0 (Value, global->priv->n_values_allocd__);
        memcpy (tmp, global->priv->values, global->priv->n_values * sizeof (Value));
        g_free (global->priv->values);
        global->priv->values = tmp;
    }

    global->priv->n_values += 1;
    g_value_init (GVALUE (global, prop_id), G_PARAM_SPEC_VALUE_TYPE (pspec));
    g_param_value_set_default (pspec, GVALUE (global, prop_id));
    VALUE(global, prop_id)->source = MOO_EDIT_CONFIG_SOURCE_AUTO;
    /* XXX prefs */
}


guint
moo_edit_config_install_setting (GParamSpec *pspec)
{
    GObjectClass *klass;
    guint prop_id;

    g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), 0);

    global_init ();
    klass = g_type_class_ref (MOO_TYPE_EDIT_CONFIG);

    if (g_object_class_find_property (klass, pspec->name))
    {
        g_warning ("%s: setting with name '%s' already exists",
                   G_STRLOC, pspec->name);
        return 0;
    }

    prop_id = vars->len;
    g_array_set_size ((GArray*) vars, prop_id + 1);

    vars->data[prop_id].pspec = pspec;
    g_param_spec_set_qdata (pspec, prop_id_quark, GUINT_TO_POINTER (prop_id));
    g_object_class_install_property (klass, prop_id, pspec);

    global_add_prop (pspec, prop_id);

    g_slist_foreach (instances, (GFunc) g_object_freeze_notify, NULL);
    g_slist_foreach (instances, (GFunc) update_prop_from_global, GUINT_TO_POINTER (prop_id));
    g_slist_foreach (instances, (GFunc) g_object_thaw_notify, NULL);

    g_type_class_unref (klass);
    return prop_id;
}


GParamSpec *
moo_edit_config_get_spec (guint id)
{
    g_return_val_if_fail (id > 0 && id < vars->len, NULL);
    return vars->data[id].pspec;
}


GParamSpec *
moo_edit_config_lookup_spec (const char     *name,
                             guint          *id,
                             gboolean        try_alias)
{
    GParamSpec *pspec;
    GObjectClass *klass;
    const char *real_name;

    g_return_val_if_fail (name != NULL, NULL);

    global_init ();

    klass = g_type_class_ref (MOO_TYPE_EDIT_CONFIG);

    real_name = name;

    if (try_alias)
        real_name = g_hash_table_lookup (aliases, name);
    if (!real_name)
        real_name = name;

    pspec = g_object_class_find_property (klass, real_name);

    if (id)
    {
        if (pspec)
            *id = GPOINTER_TO_UINT (g_param_spec_get_qdata (pspec, prop_id_quark));
        else
            *id = 0;
    }

    g_type_class_unref (klass);
    return pspec;
}


static const GValue *
config_get_value (MooEditConfig  *config,
                  const char     *setting)
{
    guint prop_id;

    g_return_val_if_fail (MOO_IS_EDIT_CONFIG (config), NULL);
    g_return_val_if_fail (setting != NULL, NULL);

    moo_edit_config_lookup_spec (setting, &prop_id, FALSE);
    g_return_val_if_fail (prop_id != 0, NULL);

    return GVALUE (config, prop_id);
}


const char *
moo_edit_config_get_string (MooEditConfig  *config,
                            const char     *setting)
{
    const GValue *val;

    g_return_val_if_fail (MOO_IS_EDIT_CONFIG (config), NULL);
    g_return_val_if_fail (setting != NULL, NULL);

    val = config_get_value (config, setting);
    g_return_val_if_fail (val && G_VALUE_HOLDS_STRING (val), NULL);

    return g_value_get_string (val);
}


guint
moo_edit_config_get_uint (MooEditConfig  *config,
                          const char     *setting)
{
    const GValue *val;

    g_return_val_if_fail (MOO_IS_EDIT_CONFIG (config), 0);
    g_return_val_if_fail (setting != NULL, 0);

    val = config_get_value (config, setting);
    g_return_val_if_fail (val && G_VALUE_HOLDS_UINT (val), 0);

    return g_value_get_uint (val);
}


gboolean
moo_edit_config_get_bool (MooEditConfig  *config,
                          const char     *setting)
{
    const GValue *val;

    g_return_val_if_fail (MOO_IS_EDIT_CONFIG (config), FALSE);
    g_return_val_if_fail (setting != NULL, FALSE);

    val = config_get_value (config, setting);
    g_return_val_if_fail (val && G_VALUE_HOLDS_BOOLEAN (val), FALSE);

    return g_value_get_boolean (val);
}


static void
moo_edit_config_set_value (MooEditConfig  *config,
                           guint           prop_id,
                           MooEditConfigSource source,
                           const GValue   *value)
{
    Value *val;

    g_return_if_fail (MOO_IS_EDIT_CONFIG (config));
    g_return_if_fail (prop_id > 0 && prop_id < vars->len);
    g_return_if_fail (G_IS_VALUE (value));

    g_assert (prop_id < config->priv->n_values);

    val = VALUE (config, prop_id);

    if (val->source >= source &&
        (val->source != source ||
         g_param_values_cmp (vars->data[prop_id].pspec, &val->gval, value)))
    {
        g_value_copy (value, &val->gval);
        val->source = source;
        object_notify (config, prop_id);
    }
}


static void
moo_edit_config_set_valist (MooEditConfig  *config,
                            const char     *first_setting,
                            MooEditConfigSource source,
                            va_list         var_args)
{
    const gchar *name;

    g_return_if_fail (MOO_IS_EDIT_CONFIG (config));
    g_return_if_fail (first_setting != NULL);

    g_object_ref (config);
    g_object_freeze_notify (G_OBJECT (config));

    name = first_setting;

    while (name)
    {
        GValue value;
        GParamSpec *pspec;
        gchar *error = NULL;
        guint prop_id;

        value.g_type = 0;
        pspec = moo_edit_config_lookup_spec (name, &prop_id, FALSE);

        if (!pspec)
        {
            g_warning ("%s: there is no setting named `%s'",
                       G_STRLOC, name);
            break;
        }

        g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
        G_VALUE_COLLECT (&value, var_args, 0, &error);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error);
            g_free (error);
            g_value_unset (&value);
            break;
        }

        moo_edit_config_set_value (config, prop_id, source, &value);
        g_value_unset (&value);

        name = va_arg (var_args, gchar*);

        if (name)
            source = va_arg (var_args, guint);
    }

    g_object_thaw_notify (G_OBJECT (config));
    g_object_unref (config);
}


void
moo_edit_config_set (MooEditConfig  *config,
                     const char     *first_setting,
                     MooEditConfigSource source,
                     ...)
{
    va_list var_args;

    g_return_if_fail (MOO_IS_EDIT_CONFIG (config));
    g_return_if_fail (first_setting != NULL);

    va_start (var_args, source);
    moo_edit_config_set_valist (config, first_setting, source, var_args);
    va_end (var_args);
}


void
moo_edit_config_get (MooEditConfig  *config,
                     const char     *first_setting,
                     ...)
{
    va_list var_args;

    g_return_if_fail (MOO_IS_EDIT_CONFIG (config));

    va_start (var_args, first_setting);
    g_object_get_valist (G_OBJECT (config), first_setting, var_args);
    va_end (var_args);
}


void
moo_edit_config_set_global (const char     *first_setting,
                            MooEditConfigSource source,
                            ...)
{
    va_list var_args;

    g_return_if_fail (first_setting != NULL);

    global_init ();

    va_start (var_args, source);
    moo_edit_config_set_valist (global, first_setting, source, var_args);
    va_end (var_args);
}


void
moo_edit_config_get_global (const char     *first_setting,
                            ...)
{
    va_list var_args;

    global_init ();

    va_start (var_args, first_setting);
    g_object_get_valist (G_OBJECT (global), first_setting, var_args);
    va_end (var_args);
}


void
moo_edit_config_unset_by_source (MooEditConfig  *config,
                                 MooEditConfigSource source)
{
    guint i;

    g_return_if_fail (MOO_IS_EDIT_CONFIG (config));

    g_object_ref (config);
    g_object_freeze_notify (G_OBJECT (config));

    for (i = 1; i < vars->len; ++i)
    {
        Value *val = VALUE (config, i);

        if (val->source < source)
            continue;

        g_param_value_set_default (vars->data[i].pspec, &val->gval);
        val->source = MOO_EDIT_CONFIG_SOURCE_AUTO;

        if (config != global)
            update_prop_from_global (config, GUINT_TO_POINTER (i));

        object_notify (config, i);
    }

    g_object_thaw_notify (G_OBJECT (config));
    g_object_unref (config);
}


guint
moo_edit_config_get_setting_id (GParamSpec *pspec)
{
    g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), 0);
    return GPOINTER_TO_UINT (g_param_spec_get_qdata (pspec, prop_id_quark));
}


gboolean
moo_edit_config_parse_bool (const char *string,
                            gboolean   *value)
{
    g_return_val_if_fail (string != NULL, FALSE);

    if (!g_ascii_strcasecmp (string, "true") ||
         !g_ascii_strcasecmp (string, "on") ||
         !g_ascii_strcasecmp (string, "yes") ||
         !g_ascii_strcasecmp (string, "1"))
    {
        if (value)
            *value = TRUE;
    }
    else if (!g_ascii_strcasecmp (string, "false") ||
              !g_ascii_strcasecmp (string, "off") ||
              !g_ascii_strcasecmp (string, "no") ||
              !g_ascii_strcasecmp (string, "0"))
    {
        if (value)
            *value = FALSE;
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}


static gboolean
parse_boolean (const char *value,
               GValue     *gval)
{
    gboolean bval;

    if (moo_edit_config_parse_bool (value, &bval))
    {
        g_value_set_boolean (gval, bval);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


void
moo_edit_config_parse (MooEditConfig  *config,
                       const char     *name,
                       const char     *value,
                       MooEditConfigSource source)
{
    GValue gval;
    gboolean result = FALSE;
    GParamSpec *pspec;
    guint prop_id;

    g_return_if_fail (MOO_IS_EDIT_CONFIG (config));
    g_return_if_fail (name != NULL);
    g_return_if_fail (value != NULL);

    pspec = moo_edit_config_lookup_spec (name, &prop_id, TRUE);

    if (!pspec)
    {
        g_warning ("%s: no setting named '%s'", G_STRLOC, name);
        return;
    }

    gval.g_type = 0;
    g_value_init (&gval, G_PARAM_SPEC_VALUE_TYPE (pspec));

    if (G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN)
        result = parse_boolean (value, &gval);
    else
        result = moo_value_convert_from_string (value, &gval);

    if (result)
        moo_edit_config_set_value (config, prop_id, source, &gval);
    else
        g_warning ("%s: could not convert '%s' to type '%s'",
                   G_STRLOC, value, g_type_name (G_VALUE_TYPE (&gval)));

    g_value_unset (&gval);
}


void
moo_edit_config_install_alias (const char     *name,
                               const char     *alias)
{
    char *s1, *s2;
    GParamSpec *pspec;

    g_return_if_fail (name != NULL);
    g_return_if_fail (alias != NULL);

    pspec = moo_edit_config_lookup_spec (name, NULL, FALSE);

    if (!pspec)
    {
        g_warning ("%s: no setting named '%s'", G_STRLOC, name);
        return;
    }

    if (moo_edit_config_lookup_spec (alias, NULL, TRUE))
    {
        g_warning ("%s: setting named '%s' already exists", G_STRLOC, alias);
        return;
    }

    if (g_hash_table_lookup (aliases, alias))
    {
        g_warning ("%s: alias '%s' already exists", G_STRLOC, alias);
        return;
    }

    s1 = g_strdup (alias);
    g_strdelimit (s1, "_", '-');
    s2 = g_strdup (alias);
    g_strdelimit (s2, "-", '_');

    g_hash_table_insert (aliases, s1, g_strdup (pspec->name));
    g_hash_table_insert (aliases, s2, g_strdup (pspec->name));
}
