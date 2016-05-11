/*
 *   mooundo2.cpp
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

#include "mooutils/mooundo2.h"
#include "moocpp/moocpp.h"

using namespace moo;

namespace {

using Action = MooUndoAction;
using ActionPtr = std::unique_ptr<Action>;

class Entry;
class ActionEntry;
class GroupEntry;
using EntryPtr = std::unique_ptr<Entry>;
using ActionEntryPtr = std::unique_ptr<ActionEntry>;
using GroupEntryPtr = std::unique_ptr<GroupEntry>;

class Entry
{
public:
    virtual ~Entry() = default;

    virtual void undo() = 0;
    virtual void redo() = 0;

    virtual bool is_empty() const = 0;
};

class ActionEntry : public Entry
{
public:
    ActionEntry(ActionPtr&& action)
        : m_action(move(action))
    {
    }

    void undo() override
    {
        g_return_if_fail(m_action != nullptr);
        m_action->undo();
    }

    void redo() override
    {
        g_return_if_fail(m_action != nullptr);
        m_action->redo();
    }

    bool is_empty() const override { return false; }

    MOO_DISABLE_COPY_OPS(ActionEntry);

private:
    ActionPtr m_action;
};

class GroupEntry : public Entry
{
public:
    GroupEntry(GroupEntry* parent, bool user)
        : m_parent(parent)
        , m_user(user)
        , m_status_change(false)
    {
    }

    void undo() override
    {
        for (auto itr = m_entries.rbegin(); itr != m_entries.rend(); ++itr)
            (*itr)->undo();
    }

    void redo() override
    {
        for (auto itr = m_entries.begin(); itr != m_entries.end(); ++itr)
            (*itr)->redo();
    }

    GroupEntry* get_parent() const { return m_parent; }
    
    bool is_empty() const override
    {
        return std::all_of(m_entries.begin(), m_entries.end(), [] (const EntryPtr& e) { return e->is_empty(); });
    }

    bool merge_action(ActionPtr& action)
    {
        // zzz
        return false;
    }

    GroupEntry& add_group()
    {
        GroupEntryPtr ep = std::make_unique<GroupEntry>(this, false);
        GroupEntry& ret = *ep;
        m_entries.push_back(move(ep));
        return ret;
    }

    void add_action(MooUndoActionPtr&& action)
    {
        g_return_if_fail(action);
        m_entries.push_back(std::make_unique<ActionEntry>(move(action)));
    }

    void set_status_change(bool value) { m_status_change = value; }
    bool is_status_change() const { return m_status_change; }

    MOO_DISABLE_COPY_OPS(GroupEntry);

private:
    GroupEntry* m_parent;
    std::vector<EntryPtr> m_entries;
    bool m_user;
    bool m_status_change;
};

} // anonymous namespace

struct MooUndoManager::Private
{
    Private()
    {
        stack.reserve(50);
    }

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;

    void clear_redo() { if (redo_idx < stack.size()) stack.resize(redo_idx); }
    void clear_merge_candidate() { merge_candidate = nullptr; }

    bool in_undo_redo = false;

    // Undo manager is enabled by default, but it can be temporarily (or permanently) disabled,
    // and disable/enable calls may be nested.
    unsigned disable_count = 0;

    // Sequence of all actions, from earliest to latest. When undo is performed, the sequence
    // does not change, but redo_idx points to the action which has just been undone, and redo
    // similarly moves the redo_idx pointer forward.
    std::vector<GroupEntryPtr> stack;
    size_t redo_idx = 0;

    // When begin_group() is called, a new group is created and pushed onto the current
    // group stack (virtual, implemented as the sequence of group->parent pointers),
    // and current_group is set to point to that new group. When end_group() is called,
    // the group is closed, and current_group is set to its parent (which will be null
    // for the top-level group).
    GroupEntry* current_group;

    // When a character is typed or deleted, a group containing the simple edit action is
    // pushed onto the stack, and merge_candidate is set to point to it. When another action
    // is performed, if merge_candidate is not null, we will try to merge that action with the
    // group pointed to by the merge_candidate. merge_candidate is set to null whenever something
    // non-trivial happens, like undo/redo, begin/end group, etc.
    GroupEntry* merge_candidate = nullptr;

    bool is_text_modified = false;
};


MooUndoGroup::MooUndoGroup(MooUndoManager& mgr, bool user)
    : m_mgr(mgr)
{
    mgr.begin_group(user);
}

MooUndoGroup::~MooUndoGroup()
{
    m_mgr.end_group();
}


MooDisableUndo::MooDisableUndo(MooUndoManager& mgr)
    : m_mgr(mgr)
{
    m_mgr.set_enabled(false);
}

MooDisableUndo::~MooDisableUndo()
{
    m_mgr.set_enabled(true);
}


MooUndoManager::MooUndoManager()
    : m_p(std::make_unique<Private>())
{
}

bool MooUndoManager::can_undo()
{
    return is_enabled() && m_p->redo_idx > 0;
}

bool MooUndoManager::can_redo()
{
    return is_enabled() && m_p->redo_idx < m_p->stack.size();
}

void MooUndoManager::undo()
{
    g_return_if_fail(!m_p->in_undo_redo);
    g_return_if_fail(can_undo());
    g_return_if_fail(m_p->current_group == nullptr);

    m_p->clear_merge_candidate();

    GroupEntry& entry = *m_p->stack[m_p->redo_idx - 1];

    {
        auto r = set_temp_value(m_p->in_undo_redo, true);
        entry.undo();
        m_p->redo_idx -= 1;
    }

    if (entry.is_status_change())
    {
        g_assert(m_p->is_text_modified);
        m_p->is_text_modified = false;
        // zzz signal
    }
}

void MooUndoManager::redo()
{
    g_return_if_fail(!m_p->in_undo_redo);
    g_return_if_fail(can_redo());
    g_return_if_fail(m_p->current_group == nullptr);

    m_p->clear_merge_candidate();

    GroupEntry& entry = *m_p->stack[m_p->redo_idx];

    {
        auto r = set_temp_value(m_p->in_undo_redo, true);
        entry.redo();
        m_p->redo_idx += 1;
    }

    if (entry.is_status_change())
    {
        g_assert(!m_p->is_text_modified);
        m_p->is_text_modified = true;
        // zzz signal
    }
}

void MooUndoManager::add_action(MooUndoActionPtr&& action)
{
    g_return_if_fail(!m_p->in_undo_redo);
    g_return_if_fail(action != nullptr);

    if (!is_enabled())
        return;

    m_p->clear_redo();

    if (m_p->merge_candidate)
    {
        g_assert(m_p->is_text_modified);

        if (m_p->merge_candidate->merge_action(action))
            return;
    }

    m_p->clear_merge_candidate();

    if (!m_p->is_text_modified)
        g_assert(!m_p->current_group || m_p->current_group->is_empty());

    if (m_p->current_group)
    {
        m_p->current_group->add_action(move(action));
    }
    else
    {
        MooUndoGroup _(*this, /*user*/ true);
        m_p->current_group->add_action(move(action));
        m_p->merge_candidate = m_p->current_group;
    }

    if (!m_p->is_text_modified)
    {
        m_p->stack.back()->set_status_change(true);
        m_p->is_text_modified = true;
    }

    m_p->redo_idx = m_p->stack.size();
}

void MooUndoManager::begin_group(bool user)
{
    if (!is_enabled())
        return;

    m_p->clear_redo();

    if (m_p->current_group)
    {
        g_return_if_fail(!user);
        m_p->current_group = &m_p->current_group->add_group();
    }
    else
    {
        GroupEntryPtr group = std::make_unique<GroupEntry>(nullptr, user);
        m_p->current_group = group.get();
        m_p->stack.push_back(move(group));
        m_p->redo_idx = m_p->stack.size();
    }
}

void MooUndoManager::end_group()
{
    if (!is_enabled())
        return;

    g_return_if_fail(m_p->current_group != nullptr);

    GroupEntry* cur_group = m_p->current_group;
    GroupEntry* parent = cur_group->get_parent();

    g_return_if_fail(!parent || !m_p->merge_candidate);

    if (!parent && cur_group->is_empty())
    {
        g_return_if_fail(!m_p->stack.empty() && cur_group == m_p->stack.back().get());
        m_p->stack.pop_back();
        m_p->redo_idx = m_p->stack.size();
    }

    m_p->current_group = parent;
}

void MooUndoManager::clear(bool is_modified)
{
    if (!is_enabled())
        return;

    g_return_if_fail(!m_p->in_undo_redo);
    g_return_if_fail(!m_p->current_group);

    m_p->stack.clear();
    m_p->redo_idx = 0;
    m_p->merge_candidate = nullptr;
    m_p->is_text_modified = is_modified;
}

void MooUndoManager::set_unmodified()
{
    m_p->is_text_modified = false;

    for (auto& ep : m_p->stack)
        ep->set_status_change(false);

    if (can_redo())
        m_p->stack[m_p->redo_idx]->set_status_change(true);
}

void MooUndoManager::set_enabled(bool enabled)
{
    g_return_if_fail(!enabled || m_p->disable_count > 0);

    clear(true);

    if (enabled)
        --m_p->disable_count;
    else
        ++m_p->disable_count;
}

bool MooUndoManager::is_enabled()
{
    return m_p->disable_count == 0;
}
