/*
 *   moocpp/gobjptrtypes-glib.h
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

#ifdef __cplusplus

#include <glib-object.h>
#include <mooglib/moo-glib.h>
#include "moocpp/gobjptr.h"

#include <stdarg.h>

namespace moo {

///////////////////////////////////////////////////////////////////////////////////////////
//
// GObject
//

template<>
class gobj_ref<GObject> : public gobj_ref_base
{
public:
    MOO_DEFINE_GOBJREF_METHODS_IMPL(GObject, gobj_ref_base)

    gulong  signal_connect          (const char* detailed_signal, GCallback c_handler, void* data);
    gulong  signal_connect_swapped  (const char* detailed_signal, GCallback c_handler, void* data);
    void    signal_emit_by_name     (const char* detailed_signal, ...);
    void    signal_emit             (guint signal_id, GQuark detail, ...);

    bool    signal_has_handler_pending          (guint signal_id, GQuark detail, bool may_be_blocked);
    gulong  signal_connect_closure_by_id        (guint signal_id, GQuark detail, GClosure* closure, bool after);
    gulong  signal_connect_closure              (const char* detailed_signal, GClosure* closure, bool after);
    gulong  signal_connect_data                 (const char* detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
    void    signal_handler_block                (gulong handler_id);
    void    signal_handler_unblock              (gulong handler_id);
    void    signal_handler_disconnect           (gulong handler_id);
    bool    signal_handler_is_connected         (gulong handler_id);
    gulong  signal_handler_find                 (GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_block_matched       (GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_unblock_matched     (GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);
    guint   signal_handlers_disconnect_matched  (GSignalMatchType mask, guint signal_id, GQuark detail, GClosure* closure, gpointer func, gpointer data);


    void    set_data                (const char* key, gpointer value);
    void    set_data_full           (const char* key, gpointer data, GDestroyNotify destroy);

    void    set                     (const char* first_prop, ...) G_GNUC_NULL_TERMINATED;
    void    set_property            (const char* property_name, const GValue* value);

    void    notify                  (const char* property_name);
    void    freeze_notify           ();
    void    thaw_notify             ();
};

namespace g {

#define MOO_GOBJ_TYPEDEFS(CppObject, CObject)                       \
    using CppObject             = moo::gobj_ref<CObject>;           \
    using CppObject##Ptr        = moo::gobj_ptr<CObject>;           \
    using CppObject##RawPtr     = moo::gobj_raw_ptr<CObject>;       \
    using Const##CppObject##Ptr = moo::gobj_raw_ptr<const CObject>;

MOO_GOBJ_TYPEDEFS(Object, GObject);

} // namespace g

} // namespace moo

#endif // __cplusplus
