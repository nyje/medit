/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   moolinebuffer.c
 *
 *   Copyright (C) 2004-2005 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#define MOOEDIT_COMPILATION
#include "mooedit/moolinebuffer.h"
#include <string.h>


static void     invalidate_line (LineBuffer *line_buf,
                                 Line       *line,
                                 int         index);


LineBuffer*
moo_line_buffer_new (void)
{
    LineBuffer *buf = g_new0 (LineBuffer, 1);
    buf->tree = moo_text_btree_new ();
    BUF_SET_CLEAN (buf);
    return buf;
}


Line*
moo_line_buffer_insert (LineBuffer     *line_buf,
                        int             index)
{
    Line *line = moo_text_btree_insert (line_buf->tree, index, NULL);
    invalidate_line (line_buf, line, index);
    return line;
}


void
moo_line_buffer_clamp_invalid (LineBuffer *line_buf)
{
    if (!BUF_CLEAN (line_buf))
    {
        int size = moo_text_btree_size (line_buf->tree);
        AREA_CLAMP (&line_buf->invalid, size);
    }
}


void
moo_line_buffer_split_line (LineBuffer *line_buf,
                            int         line,
                            int         num_new_lines,
                            GtkTextTag *tag)
{
    Line *l;
    GSList *tags;

    moo_text_btree_insert_range (line_buf->tree, line + 1, num_new_lines, tag);

    l = moo_line_buffer_get_line (line_buf, line);
    invalidate_line (line_buf, l, line);
    tags = g_slist_copy (l->hl_info->tags);
    g_slist_foreach (tags, (GFunc) g_object_ref, NULL);

    l = moo_line_buffer_get_line (line_buf, line + num_new_lines);
    invalidate_line (line_buf, l, line + num_new_lines);
    l->hl_info->tags = g_slist_concat (l->hl_info->tags, tags);
}


void
moo_line_buffer_delete (LineBuffer *line_buf,
                        int         first,
                        int         num,
                        int         move_to)
{
    Line *line;
    GSList *old_tags = NULL;
    MooLineMark **old_marks = NULL;
    guint n_old_marks = 0;

    if (move_to >= 0)
    {
        line = moo_line_buffer_get_line (line_buf, first + num - 1);
        old_tags = line->hl_info->tags;
        line->hl_info->tags = NULL;

        old_marks = line->marks;
        n_old_marks = line->n_marks;
        line->marks = NULL;

        if (n_old_marks)
            moo_text_btree_update_n_marks (line_buf->tree, line, -n_old_marks);
    }

    moo_text_btree_delete_range (line_buf->tree, first, num);

    if (move_to >= 0)
    {
        line = moo_line_buffer_get_line (line_buf, move_to);

        line->hl_info->tags = g_slist_concat (line->hl_info->tags, old_tags);

        if (n_old_marks)
        {
            MooLineMark **tmp = g_new (MooLineMark*, n_old_marks + line->n_marks);

            if (line->n_marks)
                memcpy (tmp, line->marks, line->n_marks * sizeof (MooLineMark*));
            memcpy (&tmp[line->n_marks], old_marks, n_old_marks * sizeof (MooLineMark*));

            g_free (line->marks);
            line->marks = tmp;

            moo_text_btree_update_n_marks (line_buf->tree, line, n_old_marks);
        }
    }

    moo_line_buffer_invalidate (line_buf, move_to >= 0 ? move_to : first);
}


static void
invalidate_line_one (Line *line)
{
    moo_line_erase_segments (line);
    line->hl_info->start_node = NULL;
    line->hl_info->tags_applied = FALSE;
}

void
moo_line_buffer_invalidate (LineBuffer *line_buf,
                            int         index)
{
    invalidate_line (line_buf, moo_text_btree_get_data (line_buf->tree, index), index);
}


static void
invalidate_line (LineBuffer *line_buf,
                 Line       *line,
                 int         index)
{
    invalidate_line_one (line);

    if (line_buf->invalid.empty)
    {
        line_buf->invalid.empty = FALSE;
        line_buf->invalid.first = index;
        line_buf->invalid.last = index;
    }
    else
    {
        line_buf->invalid.first = MIN (line_buf->invalid.first, index);
        line_buf->invalid.last = MAX (line_buf->invalid.last, index);
        moo_line_buffer_clamp_invalid (line_buf);
    }
}


void
moo_line_buffer_invalidate_all (LineBuffer *line_buf)
{
    moo_line_buffer_invalidate (line_buf, 0);
    AREA_SET (&line_buf->invalid, moo_text_btree_size (line_buf->tree));
}


void
moo_line_erase_segments (Line *line)
{
    g_assert (line != NULL);
    line->hl_info->n_segments = 0;
}


void
moo_line_add_segment (Line           *line,
                      int             len,
                      CtxNode        *ctx_node,
                      CtxNode        *match_node,
                      MooRule        *rule)
{
    HLInfo *info = line->hl_info;

    if (info->n_segments == info->n_segments_alloc__)
    {
        info->n_segments_alloc__ = MAX (2, 1.5 * info->n_segments_alloc__);
        info->segments = g_realloc (info->segments, info->n_segments_alloc__ * sizeof (Segment));
    }

    info->segments[info->n_segments].len = len;
    info->segments[info->n_segments].ctx_node = ctx_node;
    info->segments[info->n_segments].match_node = match_node;
    info->segments[info->n_segments].rule = rule;

    info->n_segments++;
}


Line*
moo_line_buffer_get_line (LineBuffer *line_buf,
                          int         index)
{
    return moo_text_btree_get_data (line_buf->tree, index);
}


void
moo_line_buffer_free (LineBuffer *line_buf)
{
    if (line_buf)
    {
        moo_text_btree_free (line_buf->tree);
        g_free (line_buf);
    }
}


static void
line_add_mark (LineBuffer  *line_buf,
               MooLineMark *mark,
               Line        *line)
{
    if (line->marks)
    {
        MooLineMark **tmp = g_new (MooLineMark*, line->n_marks + 1);
        memcpy (tmp, line->marks, line->n_marks * sizeof(MooLineMark*));
        g_free (line->marks);
        line->marks = tmp;
    }
    else
    {
        g_assert (!line->n_marks);
        line->marks = g_new (MooLineMark*, 1);
    }

    line->marks[line->n_marks] = mark;
    moo_text_btree_update_n_marks (line_buf->tree, line, 1);
}


void
moo_line_buffer_add_mark (LineBuffer  *line_buf,
                          MooLineMark *mark,
                          int          index)
{
    Line *line;

    g_return_if_fail (index < (int) moo_text_btree_size (line_buf->tree));
    g_assert (_moo_line_mark_get_line (mark) == NULL);

    line = moo_line_buffer_get_line (line_buf, index);
    line_add_mark (line_buf, mark, line);
    _moo_line_mark_set_line (mark, line, index, line_buf->tree->stamp);
}


static void
line_remove_mark (LineBuffer  *line_buf,
                  MooLineMark *mark,
                  Line        *line)
{
    guint i;

    g_assert (line->marks);

    for (i = 0; i < line->n_marks; ++i)
        if (line->marks[i] == mark)
            break;

    g_assert (i < line->n_marks);

    if (line->n_marks == 1)
    {
        g_free (line->marks);
        line->marks = NULL;
    }
    else if (i < line->n_marks - 1)
    {
        g_memmove (&line->marks[i], &line->marks[i+1], line->n_marks - i - 1);
    }
    else
    {
        line->marks[line->n_marks - 1] = NULL;
    }

    moo_text_btree_update_n_marks (line_buf->tree, line, -1);
}


void
moo_line_buffer_remove_mark (LineBuffer     *line_buf,
                             MooLineMark    *mark)
{
    Line *line;
    int index;

    line = _moo_line_mark_get_line (mark);
    index = moo_line_mark_get_line (mark);

    g_assert (line != NULL);
    g_assert (line == moo_line_buffer_get_line (line_buf, index));

    _moo_line_mark_set_line (mark, NULL, -1, 0);
    line_remove_mark (line_buf, mark, line);
}


/* XXX */
void
moo_line_buffer_move_mark (LineBuffer  *line_buf,
                           MooLineMark *mark,
                           int          line)
{
    g_return_if_fail (line < (int) moo_text_btree_size (line_buf->tree));
    moo_line_buffer_remove_mark (line_buf, mark);
    moo_line_buffer_add_mark (line_buf, mark, line);
}


static GSList *
node_get_marks (BTree  *tree,
                BTNode *node,
                int     first_line,
                int     last_line,
                int     node_offset)
{
    GSList *total = NULL;
    int i, j;

    if (!node->n_marks)
        return NULL;

    if (last_line < node_offset || first_line >= node_offset + node->count)
        return NULL;

    if (node->is_bottom)
    {
        for (i = MAX (first_line - node_offset, 0); i < node->n_children; ++i)
        {
            Line *line;

            if (i + node_offset > last_line)
                break;

            line = node->data[i];

            for (j = 0; j < (int) line->n_marks; ++j)
            {
                MooLineMark *mark = line->marks[j];
                _moo_line_mark_set_line (mark, line, i + node_offset, tree->stamp);
                total = g_slist_prepend (total, line->marks[j]);
            }
        }
    }
    else
    {
        for (i = 0; i < node->n_children; ++i)
        {
            GSList *child_list;

            if (node->children[i]->n_marks)
            {
                child_list = node_get_marks (tree, node->children[i],
                                             first_line, last_line,
                                             node_offset);
                total = g_slist_concat (child_list, total);
            }

            node_offset += node->children[i]->count;

            if (last_line < node_offset)
                break;
        }
    }

    return total;
}


GSList *
moo_line_buffer_get_marks_in_range (LineBuffer     *line_buf,
                                    int             first_line,
                                    int             last_line)
{
    int size = moo_text_btree_size (line_buf->tree);

    g_assert (first_line >= 0);
    g_return_val_if_fail (first_line < size, NULL);

    if (last_line < 0 || last_line >= size)
        last_line = size - 1;

    g_return_val_if_fail (first_line <= last_line, NULL);

    return g_slist_reverse (node_get_marks (line_buf->tree,
                                            line_buf->tree->root,
                                            first_line, last_line, 0));
}


guint
moo_line_buffer_get_stamp (LineBuffer *line_buf)
{
    return line_buf->tree->stamp;
}


static guint
node_get_index (BTNode* node)
{
    guint index = 0;

    while (node->parent)
    {
        int i;

        for (i = 0; i < node->parent->n_children; ++i)
        {
            if (node->parent->children[i] != node)
                index += node->parent->children[i]->count;
            else
                break;
        }

        g_assert (i < node->parent->n_children);
        node = node->parent;
    }

    return index;
}


int
moo_line_buffer_get_line_index (LineBuffer     *line_buf,
                                Line           *line)
{
    guint index;
    index = node_get_index ((BTNode*) line);
    g_assert (line == moo_line_buffer_get_line (line_buf, index));
    return index;
}
