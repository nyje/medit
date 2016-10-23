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

#include "moocpp/fileutils.h"

gstr g::build_filename(const gstr& comp1, const gstr& comp2)
{
    return gstr::take(g_build_filename(comp1.get(), comp2.get(), nullptr));
}

gstr g::build_filename(const gstr& comp1, const gstr& comp2, const gstr& comp3)
{
    return gstr::take(g_build_filename(comp1.get(), comp2.get(), comp3.get(), nullptr));
}

gstr g::build_filename(const gstr& comp1, const gstr& comp2, const gstr& comp3, const gstr& comp4)
{
    return gstr::take(g_build_filename(comp1.get(), comp2.get(), comp3.get(), comp4.get(), nullptr));
}

gstr g::build_filename(const gstr& comp1, const gstr& comp2, const gstr& comp3, const gstr& comp4, const gstr& comp5)
{
    return gstr::take(g_build_filename(comp1.get(), comp2.get(), comp3.get(), comp4.get(), comp5.get(), nullptr));
}

gstr g::build_filenamev(const std::vector<gstr>& components)
{
    GPtrArray* strv = g_ptr_array_new_full(components.size() + 1, nullptr);
    for (const auto& comp: components)
        g_ptr_array_add(strv, const_cast<char*>(comp.get()));
    g_ptr_array_add(strv, nullptr);
    gstr result = gstr::take(g_build_filenamev((char**) strv->pdata));
    g_ptr_array_free(strv, true);
    return result;
}

gstr g::get_current_dir()
{
    return gstr::take(g_get_current_dir());
}

gstr g::path_get_basename(const gchar *file_name)
{
    return gstr::take(g_path_get_basename(file_name));
}

gstr g::path_get_dirname(const gchar *file_name)
{
    return gstr::take(g_path_get_dirname(file_name));
}
