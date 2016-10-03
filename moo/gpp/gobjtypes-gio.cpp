/*
 *   gpp/gobjtypes-gio.cpp
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

#include "moogpp/gobjtypes-gio.h"

using namespace g;

FilePtr File::new_for_path(const char* path)
{
    GFile* f = g_file_new_for_path(path);
    g_return_val_if_fail(f != nullptr, nullptr);
    return File::wrap_new(f);
}
