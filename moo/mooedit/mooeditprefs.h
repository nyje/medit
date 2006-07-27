/*
 *   mooeditprefs.h
 *
 *   Copyright (C) 2004-2006 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifndef __MOO_EDIT_PREFS_H__
#define __MOO_EDIT_PREFS_H__

#include <mooedit/mooeditor.h>

G_BEGIN_DECLS


#define MOO_EDIT_PREFS_PREFIX "Editor"

GtkWidget  *moo_edit_prefs_page_new         (MooEditor      *editor);
GtkWidget  *moo_edit_colors_prefs_page_new  (MooEditor      *editor);

/* defined in mooeditprefs.c */
const char *moo_edit_setting                (const char     *setting_name);

#define MOO_EDIT_PREFS_DEFAULT_LANG             "default_lang"
#define MOO_EDIT_PREFS_SAVE_SESSION             "save_session"

#define MOO_EDIT_PREFS_TAB_KEY_ACTION           "tab_key_action"
#define MOO_EDIT_PREFS_SPACES_NO_TABS           "spaces_instead_of_tabs"
#define MOO_EDIT_PREFS_INDENT_WIDTH             "indent_width"
#define MOO_EDIT_PREFS_AUTO_INDENT              "auto_indent"
#define MOO_EDIT_PREFS_BACKSPACE_INDENTS        "backspace_indents"

#define MOO_EDIT_PREFS_AUTO_SAVE                "auto_save"
#define MOO_EDIT_PREFS_AUTO_SAVE_INTERVAL       "auto_save_interval"
#define MOO_EDIT_PREFS_MAKE_BACKUPS             "make_backups"
#define MOO_EDIT_PREFS_STRIP                    "strip"

#define MOO_EDIT_PREFS_COLOR_SCHEME             "color_scheme"

#define MOO_EDIT_PREFS_SMART_HOME_END           "smart_home_end"
#define MOO_EDIT_PREFS_WRAP_ENABLE              "wrapping_enable"
#define MOO_EDIT_PREFS_WRAP_WORDS               "wrapping_dont_split_words"
#define MOO_EDIT_PREFS_ENABLE_HIGHLIGHTING      "enable_highlighting"
#define MOO_EDIT_PREFS_HIGHLIGHT_MATCHING       "highlight_matching_brackets"
#define MOO_EDIT_PREFS_HIGHLIGHT_MISMATCHING    "highlight_mismatching_brackets"
#define MOO_EDIT_PREFS_HIGHLIGHT_CURRENT_LINE   "highlight_current_line"
#define MOO_EDIT_PREFS_SHOW_LINE_NUMBERS        "show_line_numbers"
#define MOO_EDIT_PREFS_SHOW_TABS                "show_tabs"
#define MOO_EDIT_PREFS_SHOW_TRAILING_SPACES     "show_trailing_spaces"
#define MOO_EDIT_PREFS_USE_DEFAULT_FONT         "use_default_font"
#define MOO_EDIT_PREFS_FONT                     "font"

// #define MOO_EDIT_PREFS_DIALOGS_SAVE             "dialogs/save"
// #define MOO_EDIT_PREFS_DIALOGS_OPEN             "dialogs/open"
#define MOO_EDIT_PREFS_LAST_DIR                 "last_dir"

// #define MOO_EDIT_PREFS_SEARCH_SELECTED              "search/search_selected"
#define MOO_EDIT_PREFS_QUICK_SEARCH_FLAGS       "quick_search_flags"
#define MOO_EDIT_PREFS_SEARCH_FLAGS             "search_flags"


G_END_DECLS

#endif /* __MOO_EDIT_PREFS_H__ */
