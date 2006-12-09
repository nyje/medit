/*
 *   moofolder.h
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

#ifndef MOO_FILE_VIEW_COMPILATION
#error "This file may not be included"
#endif

#ifndef __MOO_FOLDER_H__
#define __MOO_FOLDER_H__

#include "moofileview/moofile.h"

G_BEGIN_DECLS


#define MOO_TYPE_FOLDER             (_moo_folder_get_type ())
#define MOO_FOLDER(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_FOLDER, MooFolder))
#define MOO_IS_FOLDER(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_FOLDER))


GType        _moo_folder_get_type       (void) G_GNUC_CONST;

const char  *_moo_folder_get_path       (MooFolder      *folder);
/* list should be freed and elements unref'ed */
GSList      *_moo_folder_list_files     (MooFolder      *folder);
MooFile     *_moo_folder_get_file       (MooFolder      *folder,
                                         const char     *basename);
char        *_moo_folder_get_file_path  (MooFolder      *folder,
                                         MooFile        *file);
char        *_moo_folder_get_file_uri   (MooFolder      *folder,
                                         MooFile        *file);
/* result should be unref'ed */
MooFolder   *_moo_folder_get_parent     (MooFolder      *folder,
                                         MooFileFlags    wanted);
char        *_moo_folder_get_parent_path(MooFolder      *folder);

char       **_moo_folder_get_file_info  (MooFolder      *folder,
                                         MooFile        *file);
void         _moo_folder_reload         (MooFolder      *folder);


G_END_DECLS

#endif /* __MOO_FOLDER_H__ */
