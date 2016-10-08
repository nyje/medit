/*
 *   mooutils-cpp.h
 *
 *   Copyright (C) 2004-2015 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#ifdef __cplusplus

template<typename T>
inline T *moo_object_ref(T *obj)
{
    return static_cast<T*>(g_object_ref(obj));
}

#define MOO_DEFINE_FLAGS(Flags)                                                                                 \
    inline Flags operator | (Flags f1, Flags f2) { return static_cast<Flags>(static_cast<int>(f1) | f2); }      \
    inline Flags operator & (Flags f1, Flags f2) { return static_cast<Flags>(static_cast<int>(f1) & f2); }      \
    inline Flags& operator |= (Flags& f1, Flags f2) { f1 = f1 | f2; return f1; }                                \
    inline Flags& operator &= (Flags& f1, Flags f2) { f1 = f1 & f2; return f1; }                                \
    inline Flags operator ~ (Flags f) { return static_cast<Flags>(~static_cast<int>(f)); }                      \


template<typename GObjType>
struct ObjectTraits;

template<typename GObjType>
inline GObjType* object_new()
{
    return (GObjType*) g_object_new (ObjectTraits<GObjType>::gtype(), nullptr);
}

#define MOO_DEFINE_GOBJ_TRAITS(FooObject, FOO_TYPE_OBJECT)      \
    template<>                                                  \
    struct ObjectTraits<FooObject>                              \
    {                                                           \
        using CType = FooObject;                                \
        static GType gtype()                                    \
        {                                                       \
            return FOO_TYPE_OBJECT;                             \
        }                                                       \
    }

MOO_DEFINE_GOBJ_TRAITS(GObject, G_TYPE_OBJECT);


MOO_DEFINE_FLAGS(GRegexCompileFlags)
MOO_DEFINE_FLAGS(GRegexMatchFlags)


#endif // __cplusplus
