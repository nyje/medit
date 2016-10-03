/*
 *   moocpp/gobjtypes-glib.h
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

#ifndef __cplusplus
#error "This is a C++ header"
#endif

#include <moogpp/gobj.h>
#include <moogpp/utils.h>

namespace g {

class Binding;
using BindingPtr = obj_ptr<Binding>;

class Binding : public Object
{
    MOO_GOBJ_CLASS_DECL(Binding, Object, GBinding, G_TYPE_BINDING)
};

} // namespace g

MOO_DEFINE_FLAGS(GSignalFlags);
MOO_DEFINE_FLAGS(GConnectFlags);
MOO_DEFINE_FLAGS(GSpawnFlags);
MOO_DEFINE_FLAGS(GLogLevelFlags);
MOO_DEFINE_FLAGS(GRegexCompileFlags);
MOO_DEFINE_FLAGS(GIOCondition);
