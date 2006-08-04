/*
 *   moopython-mod.c
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

#include "config.h"
#include <Python.h>
#include <pygobject.h>

#include "moopython/moopython-api.h"
#include "moopython/moopython-loader.h"
#include "mooedit/mooplugin-macro.h"
#include "mooutils/moopython.h"
#include "mooutils/mooutils-misc.h"


MOO_MODULE_INIT_FUNC_DECL;
MOO_MODULE_INIT_FUNC_DECL
{
    PyObject *moo_mod;

    if (moo_python_running ())
        return FALSE;

    if (!moo_python_api_init ())
    {
        g_warning ("%s: oops", G_STRLOC);
        return FALSE;
    }

    moo_mod = PyImport_ImportModule ((char*) "moo");

    if (!moo_mod)
    {
        PyErr_Print ();
        g_warning ("%s: could not import moo", G_STRLOC);
        moo_python_api_deinit ();
        return FALSE;
    }

#ifdef pyg_disable_warning_redirections
    pyg_disable_warning_redirections ();
#else
    moo_reset_log_func ();
#endif

    if (!moo_plugin_loader_lookup (MOO_PYTHON_PLUGIN_LOADER_ID))
    {
        MooPluginLoader *loader = _moo_python_get_plugin_loader ();
        moo_plugin_loader_register (loader, MOO_PYTHON_PLUGIN_LOADER_ID);
        g_free (loader);
    }

    return TRUE;
}
