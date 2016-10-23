/*
 *   gutil.h
 *
 *   Copyright (C) 2004-2016 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#pragma once

#include <glib.h>
#include <moocpp/gstr.h>

namespace g
{

gstr build_filename(const gstr& comp1, const gstr& comp2);
gstr build_filename(const gstr& comp1, const gstr& comp2, const gstr& comp3);
gstr build_filename(const gstr& comp1, const gstr& comp2, const gstr& comp3, const gstr& comp4);
gstr build_filename(const gstr& comp1, const gstr& comp2, const gstr& comp3, const gstr& comp4, const gstr& comp5);
gstr build_filenamev(const std::vector<gstr>& components);
gstr get_current_dir();
gstr path_get_basename(const gchar *file_name);
gstr path_get_dirname(const gchar *file_name);
gstr get_current_dir();

} // namespace g
