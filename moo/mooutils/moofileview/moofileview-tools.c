/*
 *   moofileview-tools.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "moofileview/moofileview-tools.h"
#include "moofileview/moofile.h"
#include "mooutils/mooprefs.h"
#if MOO_USE_XDGMIME
#include "xdgmime/xdgmime.h"
#endif
#include <string.h>


typedef struct {
    guint merge_id;
    GSList *actions;
} ToolsInfo;

typedef struct {
    GtkAction parent;
    MooFileView *fileview;
    GSList *extensions;
    GSList *mimetypes;
    char *command;
} ToolAction;

typedef GtkActionClass ToolActionClass;

GType _moo_file_view_tool_action_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (ToolAction, _moo_file_view_tool_action, GTK_TYPE_ACTION)


static void
_moo_file_view_tool_action_init (G_GNUC_UNUSED ToolAction *action)
{
}


static void
moo_file_view_tool_action_finalize (GObject *object)
{
    ToolAction *action = (ToolAction*) object;

    g_slist_foreach (action->extensions, (GFunc) g_free, NULL);
    g_slist_free (action->extensions);
    g_slist_foreach (action->mimetypes, (GFunc) g_free, NULL);
    g_slist_free (action->mimetypes);
    g_free (action->command);

    G_OBJECT_CLASS (_moo_file_view_tool_action_parent_class)->finalize (object);
}


static void
moo_file_view_tool_action_activate (GtkAction *_action)
{
    ToolAction *action = (ToolAction*) _action;
    GList *files;
    const char *p;
    GString *command;
    GError *error = NULL;

    g_return_if_fail (MOO_IS_FILE_VIEW (action->fileview));
    g_return_if_fail (action->command && action->command[0]);

    p = strstr (action->command, "%f");
    g_return_if_fail (p != NULL);

    files = _moo_file_view_get_filenames (action->fileview);
    g_return_if_fail (files != NULL);

    command = g_string_new_len (action->command, p - action->command);

    while (files)
    {
        g_string_append_printf (command, " \"%s\"", (char*) files->data);
        g_free (files->data);
        files = g_list_delete_link (files, files);
    }

    g_string_append (command, p + 2);

    if (!g_spawn_command_line_async (command->str, &error))
    {
        g_warning ("%s: %s", G_STRLOC, error->message);
        g_error_free (error);
    }

    g_string_free (command, TRUE);
}


static void
_moo_file_view_tool_action_class_init (ToolActionClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = moo_file_view_tool_action_finalize;
    klass->activate = moo_file_view_tool_action_activate;
}


static void
remove_old_tools (MooFileView    *fileview,
                  MooUIXML       *xml,
                  GtkActionGroup *group)
{
    ToolsInfo *info;

    info = g_object_get_data (G_OBJECT (fileview), "moo-file-view-tools-info");

    if (info)
    {
        g_object_set_data (G_OBJECT (fileview), "moo-file-view-tools-info", NULL);

        moo_ui_xml_remove_ui (xml, info->merge_id);

        while (info->actions)
        {
            GtkAction *a = info->actions->data;
            gtk_action_group_remove_action (group, a);
            info->actions = g_slist_delete_link (info->actions,
                    info->actions);
        }

        g_free (info);
    }
}


static GtkAction *
tool_action_new (MooFileView *fileview,
                 const char  *label,
                 const char  *extensions,
                 const char  *mimetypes,
                 const char  *command)
{
    ToolAction *action;
    char *name;
    char **set, **p;

    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (command && strstr (command, "%f"), NULL);

    name = g_strdup_printf ("moofileview-action-%08x", g_random_int ());
    action = g_object_new (_moo_file_view_tool_action_get_type (),
                           "label", label, "name", name, NULL);
    action->fileview = fileview;
    action->command = g_strdup (command);

    if (extensions && extensions[0])
    {
        set = g_strsplit_set (extensions, ";,", 0);

        for (p = set; p && *p; ++p)
        {
            g_strstrip (*p);

            if (**p)
                action->extensions = g_slist_prepend (action->extensions,
                                                      g_strdup (*p));
        }

        action->extensions = g_slist_reverse (action->extensions);
        g_strfreev (set);
    }

    if (mimetypes && mimetypes[0])
    {
        set = g_strsplit_set (mimetypes, ";,", 0);

        for (p = set; p && *p; ++p)
        {
            g_strstrip (*p);

            if (**p)
                action->mimetypes = g_slist_prepend (action->mimetypes,
                                                     g_strdup (*p));
        }

        action->mimetypes = g_slist_reverse (action->mimetypes);
        g_strfreev (set);
    }

    g_free (name);
    return GTK_ACTION (action);
}


void
_moo_file_view_tools_load (MooFileView *fileview)
{
    ToolsInfo *info;
    MooMarkupDoc *doc;
    MooMarkupNode *root, *child;
    MooUIXML *xml;
    GtkActionGroup *group;
    MooUINode *ph;
    GSList *l;

    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    xml = _moo_file_view_get_ui_xml (fileview);
    group = _moo_file_view_get_actions (fileview);
    remove_old_tools (fileview, xml, group);

    doc = moo_prefs_get_markup ();
    g_return_if_fail (doc != NULL);

    root = moo_markup_get_element (MOO_MARKUP_NODE (doc), "MooFileView/Tools");

    if (!root)
        return;

    ph = moo_ui_xml_find_placeholder (xml, "OpenWith");
    g_return_if_fail (ph != NULL);

    info = g_new0 (ToolsInfo, 1);

    for (child = root->children; child != NULL; child = child->next)
    {
        GtkAction *action;
        const char *label, *extensions, *mimetypes;
        const char *command;

        if (!MOO_MARKUP_IS_ELEMENT (child))
            continue;

        if (strcmp (child->name, "tool"))
        {
            g_warning ("%s: unknown node '%s'", G_STRLOC, child->name);
            continue;
        }

        label = moo_markup_get_prop (child, "label");
        extensions = moo_markup_get_prop (child, "extensions");
        mimetypes = moo_markup_get_prop (child, "mimetypes");
        command = moo_markup_get_content (child);

        if (!label)
        {
            g_warning ("%s: label missing", G_STRLOC);
            continue;
        }

        if (!command)
        {
            g_warning ("%s: command missing", G_STRLOC);
            continue;
        }

        action = tool_action_new (fileview, label, extensions, mimetypes, command);

        if (!action)
            continue;

        info->actions = g_slist_prepend (info->actions, action);
    }

    if (!info->actions)
    {
        g_free (info);
        return;
    }

    info->actions = g_slist_reverse (info->actions);
    info->merge_id = moo_ui_xml_new_merge_id (xml);
    g_object_set_data (G_OBJECT (fileview), "moo-file-view-tools-info", info);

    for (l = info->actions; l != NULL; l = l->next)
    {
        GtkAction *action = l->data;
        char *markup;

        gtk_action_group_add_action (group, action);

        markup = g_markup_printf_escaped ("<item action=\"%s\"/>",
                                          gtk_action_get_name (action));
        moo_ui_xml_insert (xml, info->merge_id, ph, -1, markup);
        g_free (markup);
    }
}


static gboolean
action_check_one (ToolAction *action,
                  MooFile    *file)
{
    GSList *l;
    const char *mime;

    g_return_val_if_fail (file != NULL, FALSE);

    for (l = action->extensions; l != NULL; l = l->next)
        if (g_pattern_match_simple (l->data, _moo_file_display_name (file)))
            return TRUE;

#if MOO_USE_XDGMIME
    mime = _moo_file_get_mime_type (file);
    g_return_val_if_fail (mime != NULL, FALSE);

    if (mime == xdg_mime_type_unknown)
        return FALSE;

    for (l = action->mimetypes; l != NULL; l = l->next)
        if (xdg_mime_mime_type_subclass (mime, l->data))
            return TRUE;
#endif

    return FALSE;
}


static void
action_check (ToolAction *action,
              GList      *files)
{
    gboolean visible = TRUE;

    if (!action->extensions && !action->mimetypes)
    {
        g_object_set (action, "visible", TRUE, NULL);
        return;
    }

    while (files)
    {
        if (!action_check_one (action, files->data))
        {
            visible = FALSE;
            break;
        }

        files = files->next;
    }

    g_object_set (action, "visible", visible, NULL);
}


void
_moo_file_view_tools_check (MooFileView *fileview)
{
    GList *files;
    ToolsInfo *info;
    GSList *l;

    info = g_object_get_data (G_OBJECT (fileview), "moo-file-view-tools-info");

    if (!info)
        return;

    files = _moo_file_view_get_files (fileview);

    if (!files)
    {
        for (l = info->actions; l != NULL; l = l->next)
            g_object_set (l->data, "visible", FALSE, NULL);
        return;
    }

    for (l = info->actions; l != NULL; l = l->next)
        action_check (l->data, files);

    g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
    g_list_free (files);
}
