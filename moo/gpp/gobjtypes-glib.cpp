/*
 *   moocpp/gobjtypes-glib.cpp
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

#include "moogpp/gobjtypes-glib.h"
#include <unordered_map>

namespace {

//std::unordered_map<GType, g::obj_type_info*> known_types;
//
//g::obj_type_info* lookup_type(GType gtyp)
//{
//    auto itr = known_types.find(gtyp);
//    if (itr == known_types.end())
//        return nullptr;
//    else
//        return itr->second;
//}
//
//g::obj_type_info* get_type(GType gtyp)
//{
//    g::obj_type_info* ti = lookup_type(gtyp);
//    if (ti)
//        return ti;
//
//    g_return_val_if_fail(g_type_fundamental(gtyp) == G_TYPE_OBJECT, nullptr);
//    g_return_val_if_fail(gtyp != G_TYPE_OBJECT, nullptr);
//
//    GType parent = g_type_parent(gtyp);
//    g::obj_type_info* parent_ti = get_type(parent);
//
//    if (gtyp == G_TYPE_OBJECT)
//    {
//        
//    }
//}

} // namespace
