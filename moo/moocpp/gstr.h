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

    gstr(const gstr& s)
        : gstr()
    {
        copy_from(s.m_p);
    }

    gstr(gstr&& s)
        : gstr()
    {
        steal(s.m_p);
        s.m_p = nullptr;
    }

    const char* get() const
    {
        return m_p;
    }

    const char* get_non_null() const
    {
        return m_p ? m_p : "";
    }

    void steal(char* s)
    {
        g_free(m_p);
        m_p = s;
    }

    static gstr take(char* src)
    {
        gstr result;
        result.steal(src);
        return result;
    }

    void copy_from(const char* s)
    {
        if (s != m_p)
        {
            g_free(m_p);
            m_p = s ? g_strdup(s) : nullptr;
        }
    }

    static gstr copy(const char* s)
    {
        gstr result;
        result.copy_from(s);
        return result;
    }

    gstr dup() const
    {
        return take(m_p ? g_strdup(m_p) : nullptr); 
    }

    gstr& operator=(const gstr& other)
    {
        if (this != &other)
            copy_from(other.m_p);
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

    bool is_null() const
    {
        return !m_p;
    }

    bool equal_include_null(const gstr& other) const
    {
        if (!m_p)
            return !other.m_p;
        else if (!other.m_p)
            return false;
        else
            return strcmp(m_p, other.m_p) == 0;
    }

    bool equal_ignore_null(const gstr& other) const
    {
        return strcmp(get_non_null(), other.get_non_null()) == 0;
    }

private:
    char* m_p;
};
