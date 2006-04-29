/*
 *   mooeditfileops.h
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

#ifndef MOOEDIT_COMPILATION
#error "This file may not be included"
#endif

#ifndef __MOO_EDIT_FILE_OPS_H__
#define __MOO_EDIT_FILE_OPS_H__

#include "mooedit/mooedit.h"

G_BEGIN_DECLS


typedef struct _MooEditLoader MooEditLoader;
typedef struct _MooEditSaver  MooEditSaver;

typedef enum {
    MOO_EDIT_SAVE_BACKUP = 1 << 0
} MooEditSaveFlags;

struct _MooEditLoader
{
    guint ref_count;

    gboolean    (*load)     (MooEditLoader  *loader,
                             MooEdit        *edit,
                             const char     *file,
                             const char     *encoding,
                             GError        **error);
    gboolean    (*reload)   (MooEditLoader  *loader,
                             MooEdit        *edit,
                             GError        **error);
};

struct _MooEditSaver
{
    guint ref_count;

    gboolean    (*save)         (MooEditSaver   *saver,
                                 MooEdit        *edit,
                                 const char     *file,
                                 const char     *encoding,
                                 MooEditSaveFlags flags,
                                 GError        **error);
    gboolean    (*save_copy)    (MooEditSaver   *saver,
                                 MooEdit        *edit,
                                 const char     *file,
                                 const char     *encoding,
                                 GError        **error);
};


MooEditLoader   *_moo_edit_loader_get_default(void);
MooEditSaver    *_moo_edit_saver_get_default (void);

MooEditLoader   *_moo_edit_loader_ref       (MooEditLoader  *loader);
void             _moo_edit_loader_unref     (MooEditLoader  *loader);
MooEditSaver    *_moo_edit_saver_ref        (MooEditSaver   *saver);
void             _moo_edit_saver_unref      (MooEditSaver   *saver);

gboolean         _moo_edit_loader_load      (MooEditLoader  *loader,
                                             MooEdit        *edit,
                                             const char     *file,
                                             const char     *encoding,
                                             GError        **error);
gboolean         _moo_edit_loader_reload    (MooEditLoader  *loader,
                                             MooEdit        *edit,
                                             GError        **error);
gboolean         _moo_edit_saver_save       (MooEditSaver   *saver,
                                             MooEdit        *edit,
                                             const char     *file,
                                             const char     *encoding,
                                             MooEditSaveFlags flags,
                                             GError        **error);
gboolean         _moo_edit_saver_save_copy  (MooEditSaver   *saver,
                                             MooEdit        *edit,
                                             const char     *file,
                                             const char     *encoding,
                                             GError        **error);


G_END_DECLS

#endif /* __MOO_EDIT_FILE_OPS_H__ */
