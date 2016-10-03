/*
 *   moofiledialog-win32.h
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

#include <windows.h>
#include <string>
#include <vector>

std::vector<std::string> moo_show_win32_file_open_dialog(HWND hwnd_parent, const std::string& start_folder);
std::string moo_show_win32_file_save_as_dialog(HWND hwnd_parent, const std::string& start_folder, const std::string& basename);
