/*
 *   gstr.h
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

class gstr
{
public:
    gstr()
        : m_p(nullptr)
    {
    }

    gstr(nullptr_t)
        : gstr()
    {
    }

    ~gstr()
    {
        g_free(m_p);
    }

    gstr(const char* s)
        : gstr()
    {
        *this = s;
    }

    gstr(const gstr& s)
        : gstr()
    {
        *this = s;
    }

    gstr(gstr&& s)
        : gstr()
    {
        steal(s.m_p);
        s.m_p = nullptr;
    }

    const char* get() const
    {
        return m_p ? m_p : "";
    }

    void steal(char* s)
    {
        g_free(m_p);
        m_p = s ? g_strdup(s) : nullptr;
    }

    static gstr take(char* src)
    {
        gstr result;
        result.steal(src);
        return result;
    }

    gstr& operator=(const gstr& other)
    {
        if (this != &other)
            *this = other.m_p;
        return *this;
    }

    gstr& operator=(const char* other)
    {
        if (m_p != other)
        {
            g_free(m_p);
            m_p = other ? g_strdup(other) : nullptr;
        }
        return *this;
    }

    gstr& operator=(gstr&& other)
    {
        steal(other.m_p);
        other.m_p = nullptr;
        return *this;
    }

    bool empty() const
    {
        return !m_p || !*m_p;
    }

    bool operator==(const gstr& other) const
    {
        return strcmp(get(), other.get()) == 0;
    }

    bool operator==(const char* other) const
    {
        return strcmp(get(), other ? other : "") == 0;
    }

    bool operator==(nullptr_t) const
    {
        return empty();
    }

    template<typename T>
    bool operator!=(const T& other) const
    {
        return !(*this == other);
    }

private:
    char* m_p;
};
