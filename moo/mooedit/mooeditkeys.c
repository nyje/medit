/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *   mooeditkeys.c
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
#include "mooedit/mootextiter.h"
#include "mooutils/moocompat.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>


inline G_GNUC_CONST static GtkWidgetClass *parent_class (void)
{
    static gpointer sourceview_class = NULL;
    if (!sourceview_class)
        sourceview_class = GTK_WIDGET_CLASS (gtk_type_class (GTK_TYPE_SOURCE_VIEW));
    return GTK_WIDGET_CLASS (sourceview_class);
}


/* these two functions are taken from gtk/gtktextview.c */
static void text_view_obscure_mouse_cursor (GtkTextView *text_view);
static void set_invisible_cursor (GdkWindow *window);

static gboolean handle_tab          (MooEdit        *edit,
                                     GdkEventKey    *event);
static gboolean handle_backspace    (MooEdit        *edit,
                                     GdkEventKey    *event);
static gboolean handle_enter        (MooEdit        *edit,
                                     GdkEventKey    *event);
static gboolean handle_shift_tab    (MooEdit        *edit,
                                     GdkEventKey    *event);
static gboolean handle_ctrl_up      (MooEdit        *edit,
                                     GdkEventKey    *event,
                                     gboolean        up);
static gboolean handle_ctrl_pgup    (MooEdit        *edit,
                                     GdkEventKey    *event,
                                     gboolean        up);


int         _moo_edit_key_press_event       (GtkWidget          *widget,
                                             GdkEventKey        *event)
{
    MooEdit *edit;
    GtkTextView *text_view;
    gboolean obscure = TRUE;
    gboolean handled = FALSE;
    int keyval = event->keyval;
    GdkModifierType mods =
            event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);

    edit = MOO_EDIT (widget);
    text_view = GTK_TEXT_VIEW (widget);

    if (!mods)
    {
        switch (keyval)
        {
            case GDK_Tab:
            case GDK_KP_Tab:
                handled = handle_tab (edit, event);
                break;
            case GDK_BackSpace:
                handled = handle_backspace (edit, event);
                break;
            case GDK_KP_Enter:
            case GDK_Return:
                handled = handle_enter (edit, event);
                break;
        }
    }
    else if (mods == GDK_SHIFT_MASK)
    {
        switch (keyval)
        {
            /* TODO TODO stupid X and gtk !!! */
            case GDK_ISO_Left_Tab:
            case GDK_KP_Tab:
                handled = handle_shift_tab (edit, event);
                break;
        }
    }
    else if (mods == GDK_CONTROL_MASK)
    {
        switch (keyval)
        {
            case GDK_Up:
            case GDK_KP_Up:
                handled = handle_ctrl_up (edit, event, TRUE);
                /* if we scroll, let mouse cursor stay */
                obscure = FALSE;
                break;
            case GDK_Down:
            case GDK_KP_Down:
                handled = handle_ctrl_up (edit, event, FALSE);
                obscure = FALSE;
                break;
            case GDK_Page_Up:
            case GDK_KP_Page_Up:
                handled = handle_ctrl_pgup (edit, event, TRUE);
                obscure = FALSE;
                break;
            case GDK_Page_Down:
            case GDK_KP_Page_Down:
                handled = handle_ctrl_pgup (edit, event, FALSE);
                obscure = FALSE;
                break;
        }
    }
    else
    {
        obscure = FALSE;
    }

    if (obscure && handled)
        text_view_obscure_mouse_cursor (text_view);

    if (handled)
        return TRUE;

    edit->priv->in_key_press = TRUE;
    handled = parent_class()->key_press_event (widget, event);
    edit->priv->in_key_press = FALSE;

    return handled;
}


static void text_view_obscure_mouse_cursor (GtkTextView *text_view)
{
    if (!text_view->mouse_cursor_obscured)
    {
        GdkWindow *window =
                gtk_text_view_get_window (text_view,
                                          GTK_TEXT_WINDOW_TEXT);
        set_invisible_cursor (window);
        text_view->mouse_cursor_obscured = TRUE;
    }
}


static void set_invisible_cursor (GdkWindow *window)
{
    GdkBitmap *empty_bitmap;
    GdkCursor *cursor;
    GdkColor useless;
    char invisible_cursor_bits[] = { 0x0 };

    useless.red = useless.green = useless.blue = 0;
    useless.pixel = 0;

    empty_bitmap =
            gdk_bitmap_create_from_data (window,
                                         invisible_cursor_bits,
                                         1, 1);

    cursor = gdk_cursor_new_from_pixmap (empty_bitmap,
                                         empty_bitmap,
                                         &useless,
                                         &useless, 0, 0);

    gdk_window_set_cursor (window, cursor);

    gdk_cursor_unref (cursor);
    g_object_unref (empty_bitmap);
}


static gboolean handle_tab          (MooEdit        *edit,
                                     G_GNUC_UNUSED GdkEventKey    *event)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));
    GtkTextIter insert, bound, start, end;
    gboolean starts_line, insert_last;
    int first_line, last_line;

    if (!edit->priv->indenter || !edit->priv->tab_indents)
        return FALSE;

    if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
        gtk_text_buffer_begin_user_action (buffer);
        moo_indenter_tab (edit->priv->indenter, buffer);
        gtk_text_buffer_end_user_action (buffer);
        return TRUE;
    }

    gtk_text_buffer_get_iter_at_mark (buffer, &insert,
                                      gtk_text_buffer_get_insert (buffer));
    gtk_text_buffer_get_iter_at_mark (buffer, &bound,
                                      gtk_text_buffer_get_selection_bound (buffer));

    insert_last = (gtk_text_iter_compare (&insert, &bound) > 0);
    starts_line = gtk_text_iter_starts_line (&start);

    first_line = gtk_text_iter_get_line (&start);
    last_line = gtk_text_iter_get_line (&end);
    if (gtk_text_iter_starts_line (&end))
        last_line -= 1;

    gtk_text_buffer_begin_user_action (buffer);

    moo_indenter_shift_lines (edit->priv->indenter, buffer, first_line, last_line, 1);

    if (starts_line)
    {
        if (insert_last)
        {
            gtk_text_buffer_get_iter_at_mark (buffer, &insert,
                                              gtk_text_buffer_get_selection_bound (buffer));
            gtk_text_iter_set_line_offset (&insert, 0);
            gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &insert);
        }
        else
        {
            gtk_text_buffer_get_iter_at_mark (buffer, &insert,
                                              gtk_text_buffer_get_insert (buffer));
            gtk_text_iter_set_line_offset (&insert, 0);
            gtk_text_buffer_move_mark_by_name (buffer, "insert", &insert);
        }
    }

    gtk_text_buffer_end_user_action (buffer);

    return TRUE;
}


static gboolean handle_shift_tab    (MooEdit        *edit,
                                     G_GNUC_UNUSED GdkEventKey    *event)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));
    GtkTextIter start, end;
    int first_line, last_line;

    if (!edit->priv->shift_tab_unindents || !edit->priv->indenter)
        return FALSE;

    gtk_text_buffer_get_selection_bounds (buffer, &start, &end);

    first_line = gtk_text_iter_get_line (&start);
    last_line = gtk_text_iter_get_line (&end);

    if (gtk_text_iter_starts_line (&end) && first_line != last_line)
        last_line -= 1;

    gtk_text_buffer_begin_user_action (buffer);
    moo_indenter_shift_lines (edit->priv->indenter, buffer, first_line, last_line, -1);
    gtk_text_buffer_end_user_action (buffer);

    return TRUE;
}


static gboolean handle_backspace    (MooEdit        *edit,
                                     G_GNUC_UNUSED GdkEventKey    *event)
{
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    gboolean result;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    if (!edit->priv->backspace_indents || !edit->priv->indenter)
        return FALSE;
    if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL))
        return FALSE;

    gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                      gtk_text_buffer_get_insert (buffer));

    if (gtk_text_iter_starts_line (&iter))
        return FALSE;

    gtk_text_buffer_begin_user_action (buffer);
    result = moo_indenter_backspace (edit->priv->indenter, buffer);
    gtk_text_buffer_begin_user_action (buffer);

    return result;
}


static gboolean handle_enter        (MooEdit        *edit,
                                     G_GNUC_UNUSED GdkEventKey    *event)
{
    GtkTextBuffer *buffer;
    GtkTextIter start, end;
    gboolean has_selection;

    if (!edit->priv->enter_indents || !edit->priv->indenter)
        return FALSE;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit));

    has_selection =
            gtk_text_buffer_get_selection_bounds (buffer, &start, &end);

    gtk_text_buffer_begin_user_action (buffer);

    if (has_selection)
        gtk_text_buffer_delete (buffer, &start, &end);

    /* XXX insert "\r\n" on windows? */
    gtk_text_buffer_insert (buffer, &start, "\n", 1);
    moo_indenter_character (edit->priv->indenter, buffer, '\n', &start);

    gtk_text_buffer_end_user_action (buffer);

    gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (edit),
                                        gtk_text_buffer_get_insert (buffer));
    return TRUE;
}


static gboolean handle_ctrl_up      (MooEdit        *edit,
                                     G_GNUC_UNUSED GdkEventKey    *event,
                                     gboolean        up)
{
    GtkTextView *text_view;
    GtkAdjustment *adjustment;
    double value;

    if (!edit->priv->ctrl_up_down_scrolls)
        return FALSE;

    text_view = GTK_TEXT_VIEW (edit);
    adjustment = text_view->vadjustment;

    if (!adjustment)
        return FALSE;

    if (up)
    {
        value = adjustment->value - adjustment->step_increment;
        if (value < adjustment->lower)
            value = adjustment->lower;
        gtk_adjustment_set_value (adjustment, value);
    }
    else
    {
        value = adjustment->value + adjustment->step_increment;
        if (value > adjustment->upper - adjustment->page_size)
            value = adjustment->upper - adjustment->page_size;
        gtk_adjustment_set_value (adjustment, value);
    }

    return TRUE;
}


static gboolean handle_ctrl_pgup    (MooEdit        *edit,
                                     G_GNUC_UNUSED GdkEventKey    *event,
                                     gboolean        up)
{
    GtkTextView *text_view;
    GtkAdjustment *adjustment;
    double value;

    if (!edit->priv->ctrl_page_up_down_scrolls)
        return FALSE;

    text_view = GTK_TEXT_VIEW (edit);
    adjustment = text_view->vadjustment;

    if (!adjustment)
        return FALSE;

    if (up)
    {
        value = adjustment->value - adjustment->page_increment;
        if (value < adjustment->lower)
            value = adjustment->lower;
        gtk_adjustment_set_value (adjustment, value);
    }
    else
    {
        value = adjustment->value + adjustment->page_increment;
        if (value > adjustment->upper - adjustment->page_size)
            value = adjustment->upper - adjustment->page_size;
        gtk_adjustment_set_value (adjustment, value);
    }

    return TRUE;
}
