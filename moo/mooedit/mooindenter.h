/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *   mooindenter.h
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

#ifndef __MOO_INDENTER_H__
#define __MOO_INDENTER_H__

#include <gtk/gtktextbuffer.h>

G_BEGIN_DECLS


#define MOO_TYPE_INDENTER            (moo_indenter_get_type ())
#define MOO_INDENTER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_INDENTER, MooIndenter))
#define MOO_INDENTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_INDENTER, MooIndenterClass))
#define MOO_IS_INDENTER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_INDENTER))
#define MOO_IS_INDENTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_INDENTER))
#define MOO_INDENTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_INDENTER, MooIndenterClass))


typedef struct _MooIndenter         MooIndenter;
typedef struct _MooIndenterPrivate  MooIndenterPrivate;
typedef struct _MooIndenterClass    MooIndenterClass;

struct _MooIndenter
{
    GObject parent;

    gboolean use_tabs;
    guint tab_width;
    guint indent;
};

struct _MooIndenterClass
{
    GObjectClass parent_class;

    void        (*set_value)            (MooIndenter    *indenter,
                                         const char     *var,
                                         const char     *value);

    void        (*character)            (MooIndenter    *indenter,
                                         GtkTextBuffer  *buffer,
                                         gunichar        inserted_char,
                                         GtkTextIter    *where);
    void        (*tab)                  (MooIndenter    *indenter,
                                         GtkTextBuffer  *buffer);
    gboolean    (*backspace)            (MooIndenter    *indenter,
                                         GtkTextBuffer  *buffer);

    void        (*shift_lines)          (MooIndenter    *indenter,
                                         GtkTextBuffer  *buffer,
                                         guint           first_line,
                                         guint           last_line,
                                         int             direction);
};


GType        moo_indenter_get_type              (void) G_GNUC_CONST;

MooIndenter *moo_indenter_default_new           (void);

MooIndenter *moo_indenter_get_for_lang          (const char     *lang);
MooIndenter *moo_indenter_get_for_mode          (const char     *mode);

void         moo_indenter_set_value             (MooIndenter    *indenter,
                                                 const char     *var,
                                                 const char     *value);

char        *moo_indenter_make_space            (MooIndenter    *indenter,
                                                 guint           len,
                                                 guint           start);

void         moo_indenter_character             (MooIndenter    *indenter,
                                                 GtkTextBuffer  *buffer,
                                                 gunichar        inserted_char,
                                                 GtkTextIter    *where);
void         moo_indenter_tab                   (MooIndenter    *indenter,
                                                 GtkTextBuffer  *buffer);
gboolean     moo_indenter_backspace             (MooIndenter    *indenter,
                                                 GtkTextBuffer  *buffer);
void         moo_indenter_shift_lines           (MooIndenter    *indenter,
                                                 GtkTextBuffer  *buffer,
                                                 guint           first_line,
                                                 guint           last_line,
                                                 int             direction);


G_END_DECLS

#endif /* __MOO_INDENTER_H__ */
