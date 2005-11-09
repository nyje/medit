/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   moolang-rules.c
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
#include "mooedit/moolang-rules.h"
#include "mooedit/moolang-aux.h"


typedef enum {
    MATCH_START_ONLY = 1 << 0
} MatchFlags;

#define MooRuleString MooRuleAsciiString
#define MooRuleChar MooRuleAsciiChar
#define MooRule2Char MooRuleAscii2Char
#define MooRuleAnyChar MooRuleAsciiAnyChar


static MooRule *rule_new            (MooRuleType     type,
                                     MooRuleFlags    flags,
                                     const char     *style);

static void     child_rules_match   (MooRuleArray   *array,
                                     MatchData      *data,
                                     MatchResult    *result);
static MooRule *rules_match_real    (MooRuleArray   *array,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);

static gboolean match_string        (MooRuleString  *rule,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);
static gboolean match_regex         (MooRuleRegex   *rule,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);
static gboolean match_char          (MooRuleChar    *rule,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);
static gboolean match_2char         (MooRule2Char   *rule,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);
static gboolean match_any_char      (MooRuleAnyChar *rule,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);
static gboolean match_int           (MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);
static MooRule *match_include       (MooRuleInclude *rule,
                                     MatchData      *data,
                                     MatchResult    *result,
                                     MatchFlags      flags);

static void     rule_string_destroy (MooRuleString  *rule);
static void     rule_regex_destroy  (MooRuleRegex   *rule);
static void     rule_any_char_destroy (MooRuleAnyChar *rule);


void
moo_match_data_init (MatchData          *data,
                     int                 line_number,
                     const GtkTextIter  *line_start,
                     const GtkTextIter  *line_end)
{
    GtkTextBuffer *buffer;

    g_assert (data != NULL);
    g_assert (line_start && gtk_text_iter_starts_line (line_start));
    g_assert (line_number == gtk_text_iter_get_line (line_start));
    g_assert (!line_end || gtk_text_iter_ends_line (line_end));

    data->line_start = *line_start;
    data->line_number = line_number;

    if (line_end)
    {
        data->line_end = *line_end;
    }
    else
    {
        data->line_end = *line_start;
        if (!gtk_text_iter_ends_line (&data->line_end))
            gtk_text_iter_forward_to_line_end (&data->line_end);
    }

    buffer = gtk_text_iter_get_buffer (line_start);
    data->line_string = gtk_text_buffer_get_slice (buffer, line_start, &data->line_end, TRUE);
    data->line_string_len = strlen (data->line_string);

    data->start_iter = *line_start;
    data->start = data->line_string;
    data->start_offset = 0;
}


void
moo_match_data_set_start (MatchData          *data,
                          const GtkTextIter  *start_iter,
                          char               *start,
                          int                 start_offset)
{
    g_assert (data != NULL);
    g_assert (start != NULL);
    g_assert (start_offset >= 0);

    data->start = start;
    data->start_offset = start_offset;

    if (start_iter)
    {
        data->start_iter = *start_iter;
    }
    else
    {
        data->start_iter = data->line_start;
        gtk_text_iter_forward_chars (&data->start_iter, start_offset);
    }
}


void
moo_match_data_destroy (MatchData *data)
{
    if (data->line_string)
        g_free (data->line_string);
}


static MooRule*
rules_match_real (MooRuleArray       *array,
                  MatchData          *data,
                  MatchResult        *result,
                  MatchFlags          flags)
{
    guint i;
    MooRule *matched = NULL;
    MatchResult tmp;

    g_assert (array != NULL);

    if (!array->len)
        return NULL;

    if (flags & MATCH_START_ONLY)
    {
        data->limit = data->start;
        data->limit_offset = 0;
    }
    else
    {
        data->limit = NULL;
        data->limit_offset = SIZE_NOT_SET;
    }

    for (i = 0; i < array->len; ++i)
    {
        MooRule *rule = array->data[i];
        gboolean found = FALSE;
        MooRule *matched_here = NULL;

        if (!(flags & MATCH_START_ONLY))
        {
            /* TODO: first-non-blank */
            if ((rule->flags & MOO_RULE_MATCH_FIRST_CHAR) && data->start != data->line_string)
                continue;
        }

        if ((rule->flags & MOO_RULE_MATCH_FIRST_LINE) && data->line_number != 0)
            continue;

        switch (rule->type)
        {
            case MOO_RULE_ASCII_STRING:
                found = match_string (&rule->str, data, &tmp, flags);
                break;
            case MOO_RULE_REGEX:
                found = match_regex (&rule->regex, data, &tmp, flags);
                break;
            case MOO_RULE_ASCII_CHAR:
                found = match_char (&rule->_char, data, &tmp, flags);
                break;
            case MOO_RULE_ASCII_2CHAR:
                found = match_2char (&rule->_2char, data, &tmp, flags);
                break;
            case MOO_RULE_ASCII_ANY_CHAR:
                found = match_any_char (&rule->anychar, data, &tmp, flags);
                break;
            case MOO_RULE_INT:
                found = match_int (data, &tmp, flags);
                break;
            case MOO_RULE_INCLUDE:
                matched_here = match_include (&rule->incl, data, &tmp, flags);
                found = (matched_here != NULL);
                break;
            case MOO_RULE_KEYWORDS:
                g_return_val_if_reached (FALSE);
                break;
        }

        if (found)
        {
            if (!(flags & MATCH_START_ONLY))
            {
                if (!matched || tmp.match_start < result->match_start)
                {
                    matched = matched_here ? matched_here : rule;
                    *result = tmp;
                    data->limit = tmp.match_start;
                    data->limit_offset = tmp.match_offset;
                }

                if (tmp.match_start == data->start)
                    break;
            }
            else
            {
                matched = matched_here ? matched_here : rule;
                *result = tmp;
                break;
            }
        }
    }

    if (matched && matched->child_rules)
        child_rules_match (matched->child_rules, data, result);

    return matched;
}


static void
child_rules_match (MooRuleArray       *array,
                   MatchData          *data,
                   MatchResult        *result)
{
    MatchResult tmp;
    MooRule *matched;
    char *saved_start;

    g_assert (array != NULL);
    g_assert (result->match_start != NULL);
    g_assert (result->match_end != NULL);
    g_assert (result->match_start <= result->match_end);

    tmp = *result;
    saved_start = data->start;
    data->start = result->match_end;

    matched = rules_match_real (array, data, &tmp, MATCH_START_ONLY);

    if (matched)
    {
        g_return_if_fail (tmp.match_start == result->match_end);
        result->match_end = tmp.match_end;
        if (tmp.match_len >= 0)
        {
            if (result->match_len >= 0)
                result->match_len += tmp.match_len;
        }
        else
        {
            result->match_len = -1;
        }
    }

    data->start = saved_start;
}


MooRule*
moo_rule_array_match (MooRuleArray       *array,
                      MatchData          *data,
                      MatchResult        *result)
{
    return rules_match_real (array, data, result, 0);
}


static MooRule*
rule_new (MooRuleType         type,
          MooRuleFlags        flags,
          const char         *style)
{
    MooRule *rule;

    switch (type)
    {
        case MOO_RULE_ASCII_STRING:
        case MOO_RULE_REGEX:
        case MOO_RULE_ASCII_CHAR:
        case MOO_RULE_ASCII_2CHAR:
        case MOO_RULE_ASCII_ANY_CHAR:
        case MOO_RULE_INCLUDE:
        case MOO_RULE_INT:
            break;

        case MOO_RULE_KEYWORDS:
            g_return_val_if_reached (NULL);
            break;
    }

    rule = g_new0 (MooRule, 1);
    rule->type = type;
    rule->flags = flags;
    rule->style = g_strdup (style);

    return rule;
}


void
moo_rule_free (MooRule *rule)
{
    guint i;

    if (!rule)
        return;

    switch (rule->type)
    {
        case MOO_RULE_ASCII_STRING:
            rule_string_destroy (&rule->str);
            break;
        case MOO_RULE_REGEX:
            rule_regex_destroy (&rule->regex);
            break;
        case MOO_RULE_ASCII_ANY_CHAR:
            rule_any_char_destroy (&rule->anychar);
            break;
        case MOO_RULE_ASCII_CHAR:
        case MOO_RULE_ASCII_2CHAR:
        case MOO_RULE_INCLUDE:
        case MOO_RULE_INT:
        case MOO_RULE_KEYWORDS:
            break;
    }

    if (rule->child_rules)
    {
        for (i = 0; i < rule->child_rules->len; ++i)
            moo_rule_free (rule->child_rules->data[i]);
        g_ptr_array_free ((GPtrArray*) rule->child_rules, TRUE);
    }

    g_free (rule->style);
    g_free (rule);
}


void
moo_rule_add_child_rule (MooRule   *rule,
                         MooRule   *child_rule)
{
    g_return_if_fail (rule != NULL && child_rule != NULL);

    if (!rule->child_rules)
        rule->child_rules = (MooRuleArray*) g_ptr_array_new ();

    g_ptr_array_add ((GPtrArray*) rule->child_rules, child_rule);
}


void
moo_rule_set_end_stay (MooRule            *rule)
{
    g_return_if_fail (rule != NULL);
    rule->exit.type = MOO_CONTEXT_STAY;
    rule->exit.num = 0;
}


void
moo_rule_set_end_pop (MooRule            *rule,
                      guint               num)
{
    g_return_if_fail (rule != NULL && num != 0);
    rule->exit.type = MOO_CONTEXT_POP;
    rule->exit.num = num;
}


void
moo_rule_set_end_switch (MooRule            *rule,
                         MooContext         *target)
{
    g_return_if_fail (rule != NULL && target != 0);
    rule->exit.type = MOO_CONTEXT_SWITCH;
    rule->exit.ctx = target;
}


/*************************************************************************/
/* String match
 */

static gboolean
match_string (MooRuleString  *rule,
              MatchData      *data,
              MatchResult    *result,
              MatchFlags      flags)
{
    /* TODO: limit */

    result->match_start = NULL;

    if (rule->caseless)
    {
        if (flags & MATCH_START_ONLY)
        {
            if (!g_ascii_strncasecmp (data->start, rule->string, rule->length))
                result->match_start = data->start;
        }
        else
        {
            result->match_start = ascii_casestrstr (data->start, rule->string);
        }
    }
    else
    {
        if (flags & MATCH_START_ONLY)
        {
            if (!strncmp (data->start, rule->string, rule->length))
                result->match_start = data->start;
        }
        else
        {
            result->match_start = strstr (data->start, rule->string);
        }
    }

    if (!result->match_start)
        return FALSE;

    result->match_end = result->match_start + rule->length;
    result->match_len = rule->length;
    result->match_offset = -1;
    return TRUE;
}


MooRule*
moo_rule_string_new (const char         *string,
                     MooRuleFlags        flags,
                     const char         *style)
{
    MooRule *rule;
    guint length;

    g_return_val_if_fail (string && string[0], NULL);
    g_return_val_if_fail (g_utf8_validate (string, -1, NULL), NULL);
    g_return_val_if_fail (string_is_ascii (string), NULL);

    length = strlen (string);

    g_return_val_if_fail (length != 0, NULL);

    rule = rule_new (MOO_RULE_ASCII_STRING, flags, style);
    g_return_val_if_fail (rule != NULL, NULL);

    rule->str.caseless = (flags & MOO_RULE_MATCH_CASELESS) ? TRUE : FALSE;

    if (rule->str.caseless)
        rule->str.string = g_ascii_strdown (string, -1);
    else
        rule->str.string = g_strdup (string);

    rule->str.length = length;

    return rule;
}


static void
rule_string_destroy (MooRuleString *rule)
{
    g_free (rule->string);
}


/*************************************************************************/
/* Regex match
 */

MooRule*
moo_rule_regex_new (const char         *pattern,
                    gboolean            non_empty,
                    EggRegexCompileFlags regex_compile_options,
                    EggRegexMatchFlags  regex_match_options,
                    MooRuleFlags        flags,
                    const char         *style)
{
    MooRule *rule;
    EggRegex *regex;
    GError *error = NULL;

    g_return_val_if_fail (pattern && pattern[0], NULL);

    if (flags & MOO_RULE_MATCH_CASELESS)
        regex_compile_options |= EGG_REGEX_CASELESS;

    if (non_empty)
        regex_match_options |= EGG_REGEX_MATCH_NOTEMPTY;

    regex = egg_regex_new (pattern, regex_compile_options,
                           regex_match_options, &error);

    if (!regex)
    {
        g_warning ("could not compile pattern '%s': %s",
                   pattern, error->message);
        g_error_free (error);
        return NULL;
    }

    egg_regex_optimize (regex, &error);

    if (error)
    {
        g_warning ("egg_regex_optimize() failed: %s", error->message);
        g_error_free (error);
    }

    if (pattern[0] == '^')
        flags |= MOO_RULE_MATCH_FIRST_CHAR;

    rule = rule_new (MOO_RULE_REGEX, flags, style);

    if (!rule)
    {
        egg_regex_free (regex);
        return NULL;
    }

    rule->regex.regex = regex;

    return rule;
}


static gboolean
match_regex (MooRuleRegex   *rule,
             MatchData      *data,
             MatchResult    *result,
             MatchFlags      flags)
{
    /* TODO: limit */
    /* XXX line start and stuff */
    int n_matches, start_pos, end_pos;
    EggRegexMatchFlags regex_flags = 0;

    egg_regex_clear (rule->regex);

    if (flags & MATCH_START_ONLY)
        regex_flags |= EGG_REGEX_MATCH_ANCHORED;

    n_matches = egg_regex_match_extended (rule->regex,
                                          data->line_string, data->line_string_len,
                                          data->start - data->line_string,
                                          regex_flags);

    if (n_matches <= 0)
        return FALSE;

    egg_regex_fetch_pos (rule->regex, data->line_string, 0,
                         &start_pos, &end_pos);

    result->match_start = data->line_string + start_pos;
    result->match_end = data->line_string + end_pos;

    result->match_len = -1;
    result->match_offset = -1;

    return TRUE;
}


static void
rule_regex_destroy (MooRuleRegex *rule)
{
    egg_regex_free (rule->regex);
}


/*************************************************************************/
/* Char match
 */

MooRule*
moo_rule_char_new (char                ch,
                   MooRuleFlags        flags,
                   const char         *style)
{
    MooRule *rule;

    g_return_val_if_fail (ch && CHAR_IS_ASCII (ch), NULL);

    rule = rule_new (MOO_RULE_ASCII_CHAR, flags, style);
    g_return_val_if_fail (rule != NULL, NULL);

    if (flags & MOO_RULE_MATCH_CASELESS)
    {
        rule->_char.ch = g_ascii_tolower (ch);
        rule->_char.caseless = TRUE;
    }
    else
    {
        rule->_char.ch = ch;
    }

    return rule;
}


MooRule*
moo_rule_2char_new (char                ch1,
                    char                ch2,
                    MooRuleFlags        flags,
                    const char         *style)
{
    MooRule *rule;

    g_return_val_if_fail (ch1 && CHAR_IS_ASCII (ch1), NULL);
    g_return_val_if_fail (ch2 && CHAR_IS_ASCII (ch2), NULL);

    rule = rule_new (MOO_RULE_ASCII_2CHAR, flags, style);
    g_return_val_if_fail (rule != NULL, NULL);

    if (flags & MOO_RULE_MATCH_CASELESS)
    {
        ch1 = g_ascii_tolower (ch1);
        ch2 = g_ascii_tolower (ch2);
    }

    rule->_2char.str[0] = ch1;
    rule->_2char.str[1] = ch2;
    rule->_2char.str[2] = 0;

    return rule;
}


static gboolean
match_char (MooRuleChar    *rule,
            MatchData      *data,
            MatchResult    *result,
            MatchFlags      flags)
{
    result->match_start = NULL;

    if (flags & MATCH_START_ONLY)
    {
        if (rule->caseless)
        {
            if (data->start[0] == rule->ch)
                result->match_start = data->start;
        }
        else
        {
            if (g_ascii_tolower (data->start[0]) == rule->ch)
                result->match_start = data->start;
        }
    }
    else
    {
        if (rule->caseless)
            result->match_start = ascii_lower_strchr (data->start, rule->ch);
        else
            result->match_start = strchr (data->start, rule->ch);
    }

    if (!result->match_start)
        return FALSE;

    result->match_end = result->match_start + 1;
    result->match_len = 1;
    result->match_offset = -1;

    return TRUE;
}


static gboolean
match_2char (MooRule2Char   *rule,
             MatchData      *data,
             MatchResult    *result,
             MatchFlags      flags)
{
    result->match_start = NULL;

    if (flags & MATCH_START_ONLY)
    {
        if (data->start[0] == rule->str[0] && data->start[1] == rule->str[1])
            result->match_start = data->start;
    }
    else
    {
        result->match_start = strstr (data->start, rule->str);
    }

    if (!result->match_start)
        return FALSE;

    result->match_end = result->match_start + 2;
    result->match_len = 2;
    result->match_offset = -1;
    return TRUE;
}


/*************************************************************************/
/* AnyChar match
 */

MooRule*
moo_rule_any_char_new (const char         *string,
                       MooRuleFlags        flags,
                       const char         *style)
{
    MooRule *rule;
    guint i, len;

    g_return_val_if_fail (string && string[0], NULL);

    len = strlen (string);

    for (i = 0; i < len; ++i)
        g_return_val_if_fail (CHAR_IS_ASCII (string[i]), NULL);

    rule = rule_new (MOO_RULE_ASCII_ANY_CHAR, flags, style);
    g_return_val_if_fail (rule != NULL, NULL);

    rule->anychar.n_chars = len;
    rule->anychar.chars = g_strdup (string);

    return rule;
}


static gboolean
match_any_char (MooRuleAnyChar   *rule,
                MatchData      *data,
                MatchResult    *result,
                MatchFlags      flags)
{
    guint i;

    result->match_start = NULL;

    if (flags & MATCH_START_ONLY)
    {
        for (i = 0; i < rule->n_chars; ++i)
        {
            if (data->start[0] == rule->chars[i])
            {
                result->match_start = data->start;
                break;
            }
        }
    }
    else
    {
        for (i = 0; i < rule->n_chars; ++i)
        {
            if (!result->match_start)
            {
                result->match_start = strchr (data->start, rule->chars[i]);
            }
            else if (result->match_start == data->start + 1)
            {
                if (data->start[0] == rule->chars[i])
                {
                    result->match_start = data->start;
                    break;
                }
            }
            else
            {
                char *tmp = strchr (data->start, rule->chars[i]);
                if (tmp < result->match_start)
                    result->match_start = tmp;
            }

            if (result->match_start == data->start)
                break;
        }
    }

    if (!result->match_start)
        return FALSE;

    result->match_end = result->match_start + 1;
    result->match_len = 1;
    result->match_offset = -1;
    return TRUE;
}


static void
rule_any_char_destroy (MooRuleAnyChar *rule)
{
    g_free (rule->chars);
}


/*************************************************************************/
/* Keywords
 */

MooRule*
moo_rule_keywords_new (GSList             *words,
                       MooRuleFlags        flags,
                       const char         *style)
{
    GSList *l;
    GString *pattern;
    MooRule *rule;

    g_return_val_if_fail (words != NULL, NULL);

    for (l = words; l != NULL; l = l->next)
    {
        char *word = l->data;
        g_return_val_if_fail (word && word[0], NULL);
        g_return_val_if_fail (g_utf8_validate (word, -1, NULL), NULL);
    }

    pattern = g_string_new ("\\b(");

    for (l = words; l != NULL; l = l->next)
    {
        if (l != words)
            g_string_append_c (pattern, '|');
        g_string_append (pattern, l->data);
    }

    g_string_append (pattern, ")\\b");

    rule = moo_rule_regex_new (pattern->str, TRUE, 0, 0, flags, style);

    g_string_free (pattern, TRUE);
    return rule;
}


/*************************************************************************/
/* IncludeRules
 */

MooRule*
moo_rule_include_new (MooContext *ctx)
{
    MooRule *rule;

    g_return_val_if_fail (ctx != NULL, NULL);

    rule = rule_new (MOO_RULE_INCLUDE, 0, NULL);
    g_return_val_if_fail (rule != NULL, NULL);

    rule->incl.ctx = ctx;

    return rule;
}


static MooRule*
match_include (MooRuleInclude *rule,
               MatchData      *data,
               MatchResult    *result,
               MatchFlags      flags)
{
    return rules_match_real (rule->ctx->rules, data, result, flags);
}


/*************************************************************************/
/* Special sequences
 */

MooRule*
moo_rule_int_new (MooRuleFlags        flags,
                  const char         *style)
{
    return rule_new (MOO_RULE_INT, flags, style);
}


#define ISDIGIT(c__) (c__ >= '0' && c__ <= '9')

static gboolean
match_int (MatchData      *data,
           MatchResult    *result,
           MatchFlags      flags)
{
    if (flags & MATCH_START_ONLY)
    {
        if (ISDIGIT(data->start[0]))
        {
            guint i;
            for (i = 1; ISDIGIT(data->start[i]); ++i) ;
            result->match_start = data->start;
            result->match_end = result->match_start + i;
            result->match_len = i;
            result->match_offset = -1;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        guint i;

        for (i = 0; data->start[i] && !ISDIGIT(data->start[i]); ++i) ;

        if (!data->start[i])
            return FALSE;

        result->match_start = data->start + i;

        for ( ; ISDIGIT(data->start[i]); ++i) ;

        result->match_end = result->match_start + i;
        result->match_len = result->match_end - result->match_start;
        result->match_offset = -1;

        return TRUE;
    }
}
