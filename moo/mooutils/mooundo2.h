/*
 *   mooundo2.h
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

#include "moocpp/utils.h"
#include <memory>

class MooUndoClient;
class MooUndoManager;
class MooUndoGroup;
class MooUndoAction;

class MooUndoClient
{
public:
};

class MooUndoGroup
{
public:
    MooUndoGroup(MooUndoManager& mgr, bool user = true);
    ~MooUndoGroup();

    MOO_DISABLE_COPY_OPS(MooUndoGroup);

private:
    MooUndoManager& m_mgr;
};

class MooDisableUndo
{
public:
    MooDisableUndo(MooUndoManager& mgr);
    ~MooDisableUndo();

    MOO_DISABLE_COPY_OPS(MooDisableUndo);

private:
    MooUndoManager& m_mgr;
};

class MooUndoAction
{
public:
    virtual ~MooUndoAction() = default;

    virtual void undo() = 0;
    virtual void redo() = 0;
};

using MooUndoActionPtr = std::unique_ptr<MooUndoAction>;

class MooUndoManager
{
public:
    MooUndoManager();
    ~MooUndoManager();

    bool can_undo();
    bool can_redo();

    void undo();
    void redo();

    void add_action(MooUndoActionPtr&& action);

    void set_unmodified();
    void clear(bool is_modified);

    bool is_enabled();

    MOO_DISABLE_COPY_OPS(MooUndoManager);

private:
    void begin_group(bool user);
    void end_group();

    void set_enabled(bool enabled);

private:
    friend class MooUndoGroup;
    friend class MooDisableUndo;

    struct Private;
    std::unique_ptr<Private> m_p;
};
