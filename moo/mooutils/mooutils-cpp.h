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

#include <gtk/gtk.h>

#ifdef __cplusplus

template<typename T>
inline T *object_ref(T *obj)
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

template<typename GObjType, typename Arg1>
inline GObjType* object_new(const char* prop1, Arg1 arg1)
{
    return (GObjType*) g_object_new (ObjectTraits<GObjType>::gtype(), prop1, arg1, nullptr);
}

template<typename GObjType, typename Arg1, typename Arg2>
inline GObjType* object_new(const char* prop1, Arg1 arg1, const char* prop2, Arg2 arg2)
{
    return (GObjType*) g_object_new (ObjectTraits<GObjType>::gtype(), prop1, arg1, prop2, arg2, nullptr);
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
    };

MOO_DEFINE_GOBJ_TRAITS(GObject, G_TYPE_OBJECT);


template<typename Owner, typename Value>
class ObjectDataAccessor
{
public:
    ObjectDataAccessor(const char* prop_name)
        : m_prop_name(prop_name)
    {
    }

    Value get(Owner* owner) const
    {
        return (Value) g_object_get_data (G_OBJECT (owner), m_prop_name);
    }

    void set(Owner* owner, Value value) const
    {
        g_object_set_data (G_OBJECT (owner), m_prop_name, value);
    }

    void set(Owner* owner, Value value, GDestroyNotify notify) const
    {
        g_object_set_data_full (G_OBJECT (owner), m_prop_name, value, notify);
    }

    ObjectDataAccessor(const ObjectDataAccessor&) = delete;
    ObjectDataAccessor& operator=(const ObjectDataAccessor&) = delete;

private:
    const char* m_prop_name;
};

template<typename Owner>
class ObjectDataAccessor<Owner, bool>
{
public:
    ObjectDataAccessor(const char* prop_name)
        : m_prop_name(prop_name)
    {
    }

    bool get(Owner* owner) const
    {
        return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (owner), m_prop_name));
    }

    void set(Owner* owner, bool value) const
    {
        g_object_set_data (G_OBJECT (owner), m_prop_name, GINT_TO_POINTER (value));
    }

    void set(Owner* owner, bool value, GDestroyNotify notify) const
    {
        g_object_set_data_full (G_OBJECT (owner), m_prop_name, GINT_TO_POINTER (value), notify);
    }

    ObjectDataAccessor(const ObjectDataAccessor&) = delete;
    ObjectDataAccessor& operator=(const ObjectDataAccessor&) = delete;

private:
    const char* m_prop_name;
};


MOO_DEFINE_FLAGS(GRegexCompileFlags)
MOO_DEFINE_FLAGS(GRegexMatchFlags)
MOO_DEFINE_FLAGS(GdkDragAction)


#endif // __cplusplus
