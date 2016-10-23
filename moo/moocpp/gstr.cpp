/*
 *   gstr.cpp
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

#include "gstr.h"

gstr::gstr()
    : m_p(nullptr)
{
}

gstr::gstr(nullptr_t)
    : gstr()
{
}

gstr::~gstr()
{
    g_free(m_p);
}

gstr::gstr(const char* s)
    : gstr()
{
    *this = s;
}

gstr::gstr(char* s, bool)
    : m_p(s)
{
}

gstr::gstr(const gstr& s)
    : gstr()
{
    *this = s;
}

gstr::gstr(gstr&& s)
    : gstr()
{
    steal(s.m_p);
    s.m_p = nullptr;
}

const char* gstr::get() const
{
    return m_p ? m_p : "";
}

void gstr::steal(char* s)
{
    g_free(m_p);
    m_p = s ? g_strdup(s) : nullptr;
}

gstr gstr::take(char* src)
{
    return gstr(src, true);
}

gstr& gstr::operator=(const gstr& other)
{
    if (this != &other)
        *this = other.m_p;
    return *this;
}

gstr& gstr::operator=(const char* other)
{
    if (m_p != other)
    {
        g_free(m_p);
        m_p = other ? g_strdup(other) : nullptr;
    }
    return *this;
}

gstr& gstr::operator=(gstr&& other)
{
    steal(other.m_p);
    other.m_p = nullptr;
    return *this;
}

void gstr::clear()
{
    *this = nullptr;
}

bool gstr::empty() const
{
    return !m_p || !*m_p;
}

bool gstr::operator==(const gstr& other) const
{
    return strcmp(get(), other.get()) == 0;
}

bool gstr::operator==(const char* other) const
{
    return strcmp(get(), other ? other : "") == 0;
}

bool gstr::operator==(nullptr_t) const
{
    return empty();
}

bool gstr::operator<(const gstr& other) const
{
    return strcmp(get(), other.get()) < 0;
}

std::vector<gstr> gstr::from_strv(char** strv)
{
    size_t len = strv ? g_strv_length(strv) : 0;
    std::vector<gstr> result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i)
        result.push_back(strv[i]);
    return result;
}

std::vector<gstr> gstr::split(const char* separator, int max_pieces) const
{
    return from_strv(g_strsplit(get(), separator, max_pieces));
}
