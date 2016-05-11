/*
 *   moocpp/utils.h
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

#ifndef __cplusplus

#define MOO_CPP_DECLS(code)
#define MOO_CPP_DFLT_PARAM(val)

#else // __cplusplus

#define MOO_CPP_DECLS(code) G_END_DECLS code G_BEGIN_DECLS
#define MOO_CPP_DFLT_PARAM(val) = val

#include <algorithm>
#include <memory>
#include <list>
#include <vector>
#include <functional>
#include <mooglib/moo-glib.h>

namespace moo {

#define MOO_DISABLE_COPY_OPS(Object)            \
    Object(const Object&) = delete;             \
    Object& operator=(const Object&) = delete;

#define MOO_DEFINE_FLAGS(Flags)                                                                                 \
    inline Flags operator | (Flags f1, Flags f2) { return static_cast<Flags>(static_cast<int>(f1) | f2); }      \
    inline Flags operator & (Flags f1, Flags f2) { return static_cast<Flags>(static_cast<int>(f1) & f2); }      \
    inline Flags& operator |= (Flags& f1, Flags f2) { f1 = f1 | f2; return f1; }                                \
    inline Flags& operator &= (Flags& f1, Flags f2) { f1 = f1 & f2; return f1; }                                \
    inline Flags operator ~ (Flags f) { return static_cast<Flags>(~static_cast<int>(f)); }                      \

template<typename Container, typename U>
auto find(const Container& cont, const U& elm) -> decltype(cont.begin())
{
    return std::find(cont.begin(), cont.end(), elm);
}

template<typename Container, typename U>
auto find(Container& cont, const U& elm) -> decltype(cont.begin())
{
    return std::find(cont.begin(), cont.end(), elm);
}

template<typename Container, typename UnPr>
auto find_if(const Container& cont, const UnPr& pred) -> decltype(cont.begin())
{
    return std::find_if(cont.begin(), cont.end(), pred);
}

template<typename Container, typename U>
bool contains(const Container& vec, const U& elm)
{
    return find(vec, elm) != vec.end();
}

template<typename Container, typename U>
bool contains_key(const Container& map, const U& elm)
{
    return map.find(elm) != map.end();
}

template<typename Container, typename UnPr>
bool any_of(const Container& cont, const UnPr& pred)
{
    return std::any_of(cont.begin(), cont.end(), pred);
}

template<typename Container, typename U>
void remove(Container& vec, const U& elm)
{
    auto itr = find(vec, elm);
    g_return_if_fail(itr != vec.end());
    vec.erase(itr);
}

template<typename Container, typename UnPr>
void remove_if (Container& vec, const UnPr& pr)
{
    auto itr = find_if (vec, pr);
    g_return_if_fail (itr != vec.end ());
    vec.erase (itr);
}

// C++14
template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T> (new T (std::forward<Args> (args)...));
}

template<class T, class... Args>
std::shared_ptr<T> make_shared (Args&&... args)
{
    return std::shared_ptr<T> (new T (std::forward<Args> (args)...));
}

class raii
{
public:
    using func_type = std::function<void()>;

    raii(func_type&& f) : m_f(std::move(f)) {}
    ~raii() { if (m_do) m_f(); }

    raii(raii&& other) : m_f(move(other.m_f)) { other.m_do = false; }

    MOO_DISABLE_COPY_OPS(raii);

private:
    func_type m_f;
    bool m_do = true;
};

template<typename Func>
raii on_scope_exit(Func&& f)
{
    return raii{ move(f) };
}

template<typename T>
class value_restore
{
public:
    value_restore(T& x)
        : m_r(x)
        , m_v(x)
    {
    }

    value_restore(value_restore&& other)
        : m_r(other.m_r)
        , m_v(std::move(other.m_v))
    {
        other.m_do = false;
    }

    ~value_restore()
    {
        if (m_do)
            m_r = m_v;
    }

    MOO_DISABLE_COPY_OPS(value_restore);

private:
    T& m_r;
    T m_v;
    bool m_do = true;
};

template<typename T>
value_restore<T> set_temp_value(T& x, const T& tmp_val)
{
    value_restore<T> r(x);
    x = tmp_val;
    return r;
}

} // namespace moo

#endif // __cplusplus
