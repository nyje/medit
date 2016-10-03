/*
 *   moocpp/gobj.h
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

#include <glib-object.h>

namespace g {

namespace impl
{

class base
{
public:
    virtual ~base() = default;
    virtual GObject* _gobj() = 0;
};

} // namespace impl

namespace impl
{

class obj_base
{
protected:
    obj_base(GObject* o);
    ~obj_base();

    GObject* _gobj() const { return m_gobj; }

private:
    GObject* m_gobj;
};

class internals_accessor;

} // namespace impl

struct obj_type_info
{
    obj_type_info(GType g_type);

    GType g_type;
};

struct iface_type_info
{
    iface_type_info(GType g_type);

    GType g_type;
};

#define MOO_GOBJ_CLASS_DECL_BASE(Self, GName, G_TYPE) \
        friend class g::impl::internals_accessor; \
\
    public: \
        static const g::obj_type_info type_info; \
\
        using g_class = GName; \
\
        GName *gobj() { return reinterpret_cast<GName*>(_gobj()); } \
\
        operator GName&() { return *gobj(); } \
\
        static g::obj_ptr<Self> wrap_new(GName* o); \

#define MOO_GOBJ_CLASS_DECL(Self, Parent, GName, G_TYPE) \
MOO_GOBJ_CLASS_DECL_BASE(Self, GName, G_TYPE) \
protected: \
    Self() = delete; \
    explicit Self(GName* o) : Parent(reinterpret_cast<Parent::g_class*>(o)) {} \
    GObject *_gobj() { return Parent::_gobj(); } \


#define MOO_GOBJ_IFACE_DECL(Self, GName, G_TYPE) \
    friend class g::impl::internals_accessor; \
\
public: \
    static const g::iface_type_info type_info; \
\
    using g_iface = GName; \
\
    GName *gobj() { return reinterpret_cast<GName*>(_gobj()); } \
\
    operator GName&() { return *gobj(); } \
\
    static g::obj_ptr<Self> wrap_new(GName* o); \

#define MOO_DECLARE_GOBJ_CLASS(Self) \
    class Self; \
    using Self##Ptr = g::obj_ptr<Self> \

#define MOO_DEFINE_SIMPLE_GOBJ_CLASS(Self, Parent, GSelf, G_TYPE) \
MOO_DECLARE_GOBJ_CLASS(Self); \
class Self : public Parent \
{ \
    MOO_GOBJ_CLASS_DECL(Self, Parent, GSelf, G_TYPE) \
}


#define MOO_CUSTOM_GOBJ_CLASS_DECL(Self, Parent) \
    friend class impl::internals_accessor; \
\
public: \
    static const g::obj_type_info type_info; \


namespace impl {

template<typename T, typename RefUnref>
class ref_ptr
{
public:
    ref_ptr(T* p = nullptr) : m_p(p) { if (m_p) RefUnref::ref(m_p); }
    ~ref_ptr() { if (m_p) RefUnref::unref(m_p); }

    T* get() const { return m_p; }
    T* operator->() const { return m_p; }
    T& operator*() const { return *m_p; }

    void reset() { *this = nullptr; }

    ref_ptr(const ref_ptr& rhs) : ref_ptr(rhs.m_p) {}
    ref_ptr& operator=(const ref_ptr& rhs) { *this = rhs.m_p; return *this; }
    ref_ptr(ref_ptr&& rhs) : m_p(rhs.m_p) { rhs.m_p = nullptr; }
    ref_ptr& operator=(ref_ptr&& rhs) { std::swap(m_p, rhs.m_p); return *this; }
    ref_ptr& operator=(T* p) { if (m_p != p) { T* tmp = m_p; m_p = p; if (tmp) RefUnref::unref(tmp); if(m_p) RefUnref::ref(m_p); } return *this; }

    template<typename U>
    ref_ptr(U* p) : ref_ptr(static_cast<T*>(p)) {}
    template<typename U>
    ref_ptr& operator=(U* p) { *this = static_cast<T*>(p); return *this; }

    template<typename U>
    ref_ptr(const ref_ptr<U, RefUnref>& rhs) : ref_ptr(rhs.m_p) {}
    template<typename U>
    ref_ptr& operator=(const ref_ptr<U, RefUnref>& rhs) { *this = rhs.get(); return *this; }
    template<typename U>
    ref_ptr(ref_ptr<U, RefUnref>&& rhs) : m_p(rhs.m_p) { rhs.m_p = nullptr; }
    template<typename U>
    ref_ptr& operator=(ref_ptr<U, RefUnref>&& rhs);

private:
    T* m_p;
};

class obj_ref_unref
{
public:
    template<typename T>
    static void ref(T* o) { g_object_ref(o->gobj()); }

    template<typename T>
    static void unref(T* o) { g_object_unref(o->gobj()); }
};

} // namespace impl

template<typename T>
using obj_ptr = impl::ref_ptr<T, impl::obj_ref_unref>;

///////////////////////////////////////////////////////////////////////////////////////////
//
// GObject
//

class Object;
using ObjectPtr = obj_ptr<Object>;

class Object : public impl::obj_base
{
    MOO_GOBJ_CLASS_DECL_BASE(Object, GObject, G_TYPE_OBJECT)

protected:
    Object() = delete;
    explicit Object(GObject* o) : impl::obj_base(o) {}

public:
    gulong  connect(const char* detailed_signal, GCallback c_handler, void* data);
    gulong  connect_swapped(const char* detailed_signal, GCallback c_handler, void* data);

    void    signal_emit_by_name(const char* detailed_signal, ...);
    void    signal_emit(guint signal_id, GQuark detail, ...);

    bool    signal_has_handler_pending(guint signal_id, GQuark detail, bool may_be_blocked);
    gulong  signal_connect_closure_by_id(guint signal_id, GQuark detail, GClosure* closure, bool after);
    gulong  signal_connect_closure(const char* detailed_signal, GClosure* closure, bool after);
    gulong  signal_connect_data(const char* detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
    void    signal_handler_block(gulong handler_id);
    void    signal_handler_unblock(gulong handler_id);
    void    signal_handler_disconnect(gulong handler_id);
    bool    signal_handler_is_connected(gulong handler_id);
    gulong  signal_handler_find(GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_block_matched(GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_unblock_matched(GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_disconnect_matched(GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_disconnect_by_func(GCallback c_handler, void* data);

#if 0
    void*   get_data(const char* key);
    void*   get_data(GQuark q);
    void    set_data(const char* key, gpointer value, GDestroyNotify destroy = nullptr);
    void    set_data(GQuark q, gpointer value, GDestroyNotify destroy = nullptr);

    void    set_property(const char* property_name, const GValue* value);

    template<typename T>
    void set(const char* prop, const T& value)
    {
        g_object_set(gobj(), prop, cpp_vararg_value_fixer<T>::apply(value), nullptr);
    }

    template<typename T, typename... Args>
    void set(const char* prop, const T& value, Args&&... args)
    {
        set(prop, value);
        set(std::forward<Args>(args)...);
    }

    void get(const char* prop, bool& dest)
    {
        gboolean val;
        g_object_get(gobj(), prop, &val, nullptr);
        dest = val;
    }

    template<typename T>
    void get(const char* prop, T&& dest)
    {
        g_object_get(gobj(), prop, cpp_vararg_dest_fixer<T>::apply(std::forward<T>(dest)), nullptr);
    }

    template<typename T, typename... Args>
    void get(const char* prop, T&& dest, Args&&... args)
    {
        get(prop, std::forward<T>(dest));
        get(std::forward<Args>(args)...);
    }

#endif // 0

    void    notify(const char* property_name);
    void    freeze_notify();
    void    thaw_notify();
};

///////////////////////////////////////////////////////////////////////////////////////////
//
// GIface
//

class Iface : public impl::base
{
};

} // namespace g
