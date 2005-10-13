/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   mooeditwindow.h
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

#ifndef __MOO_EDIT_WINDOW_H__
#define __MOO_EDIT_WINDOW_H__

#include <mooedit/mooedit.h>
#include <mooutils/moowindow.h>
#include <mooutils/moobigpaned.h>

G_BEGIN_DECLS


#define MOO_TYPE_EDIT_WINDOW            (moo_edit_window_get_type ())
#define MOO_EDIT_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOO_TYPE_EDIT_WINDOW, MooEditWindow))
#define MOO_EDIT_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_EDIT_WINDOW, MooEditWindowClass))
#define MOO_IS_EDIT_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOO_TYPE_EDIT_WINDOW))
#define MOO_IS_EDIT_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_EDIT_WINDOW))
#define MOO_EDIT_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_EDIT_WINDOW, MooEditWindowClass))


typedef struct _MooEditWindow           MooEditWindow;
typedef struct _MooEditWindowPrivate    MooEditWindowPrivate;
typedef struct _MooEditWindowClass      MooEditWindowClass;

struct _MooEditWindow
{
    MooWindow               parent;
    MooBigPaned            *paned;
    MooEditWindowPrivate   *priv;
};

struct _MooEditWindowClass
{
    MooWindowClass          parent_class;

    /* these do not open or close document */
    void (*new_doc)         (MooEditWindow  *window,
                             MooEdit        *doc);
    void (*close_doc)       (MooEditWindow  *window,
                             MooEdit        *doc);
    void (*close_doc_after) (MooEditWindow  *window);
};


GType        moo_edit_window_get_type           (void) G_GNUC_CONST;

MooEdit     *moo_edit_window_get_active_doc     (MooEditWindow  *window);
void         moo_edit_window_set_active_doc     (MooEditWindow  *window,
                                                 MooEdit        *edit);

GSList      *moo_edit_window_list_docs          (MooEditWindow  *window);
guint        moo_edit_window_num_docs           (MooEditWindow  *window);

void         moo_edit_window_set_title_prefix   (MooEditWindow  *window,
                                                 const char     *prefix);

/* sinks widget and frees label */
gboolean     moo_edit_window_add_pane           (MooEditWindow  *window,
                                                 const char     *user_id,
                                                 GtkWidget      *widget,
                                                 MooPaneLabel   *label,
                                                 MooPanePosition position);
gboolean     moo_edit_window_remove_pane        (MooEditWindow  *window,
                                                 const char     *user_id);
GtkWidget   *moo_edit_window_get_pane           (MooEditWindow  *window,
                                                 const char     *user_id);


G_END_DECLS

#endif /* __MOO_EDIT_WINDOW_H__ */
