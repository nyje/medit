/*
 *   moopython/moo-pygtk.c
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

#include <Python.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mooutils/mooutils-misc.h"
#include "moopython/pygtk/moo-mod.h"
#include "moopython/pygtk/moo-pygtk.h"
#include <pygobject.h>  /* _PyGObjectAPI lives here */
#include <pygtk/pygtk.h>
#include <glib.h>


static PyObject *
moo_version (void)
{
    return PyString_FromString (MOO_VERSION);
}

static PyObject *
moo_detailed_version (void)
{
    PyObject *res = PyDict_New ();
    g_return_val_if_fail (res != NULL, NULL);

    PyDict_SetItemString (res, "full", PyString_FromString (MOO_VERSION));
    PyDict_SetItemString (res, "major", PyInt_FromLong (MOO_VERSION_MAJOR));
    PyDict_SetItemString (res, "minor", PyInt_FromLong (MOO_VERSION_MINOR));
    PyDict_SetItemString (res, "micro", PyInt_FromLong (MOO_VERSION_MICRO));

    return res;
}


static PyMethodDef _moo_functions[] = {
    {NULL, NULL, 0, NULL}
};

static char *_moo_module_doc = (char*)"_moo module.";


static void
func_init_pygobject (void)
{
    PyObject *gobject, *pygtk;

    gobject = PyImport_ImportModule ((char*) "gobject");

    if (gobject != NULL)
    {
        PyObject *mdict = PyModule_GetDict (gobject);
        PyObject *cobject = PyDict_GetItemString (mdict, "_PyGObject_API");

        if (PyCObject_Check (cobject))
        {
            _PyGObject_API = (struct _PyGObject_Functions *) PyCObject_AsVoidPtr (cobject);
        }
        else
        {
            PyErr_SetString (PyExc_RuntimeError,
                             "could not find _PyGObject_API object");
            return;
        }
    }
    else
    {
        return;
    }

    pygtk = PyImport_ImportModule((char*) "gtk._gtk");

    if (pygtk != NULL)
    {
        PyObject *module_dict = PyModule_GetDict (pygtk);
        PyObject *cobject = PyDict_GetItemString (module_dict, "_PyGtk_API");

        if (PyCObject_Check (cobject))
        {
            _PyGtk_API = (struct _PyGtk_FunctionStruct*) PyCObject_AsVoidPtr (cobject);
        }
        else
        {
            PyErr_SetString (PyExc_RuntimeError,
                             "could not find _PyGtk_API object");
            return;
        }
    }
    else
    {
        return;
    }
}

gboolean
_moo_pygtk_init (void)
{
    PyObject *_moo_module, *code, *moo_mod, *submod;

    func_init_pygobject ();

    if (PyErr_Occurred ())
        return FALSE;

    _moo_module = Py_InitModule3 ((char*) "_moo", _moo_functions, _moo_module_doc);

    if (PyErr_Occurred ())
        return FALSE;

    PyImport_AddModule ((char*) "moo");

    PyModule_AddObject (_moo_module, (char*)"version", moo_version());
    PyModule_AddObject (_moo_module, (char*)"detailed_version", moo_detailed_version());

#ifdef MOO_BUILD_UTILS
    if (!_moo_utils_mod_init ())
        return FALSE;
    submod = PyImport_ImportModule ((char*) "moo.utils");
    PyModule_AddObject (_moo_module, (char*) "utils", submod);
#endif
#ifdef MOO_BUILD_TERM
    if (!_moo_term_mod_init ())
        return FALSE;
    submod = PyImport_ImportModule ((char*) "moo.term");
    PyModule_AddObject (_moo_module, (char*) "term", submod);
#endif
#ifdef MOO_BUILD_EDIT
    if (!_moo_edit_mod_init ())
        return FALSE;
    submod = PyImport_ImportModule ((char*) "moo.edit");
    PyModule_AddObject (_moo_module, (char*) "edit", submod);
#endif
#ifdef MOO_BUILD_APP
    if (!_moo_app_mod_init ())
        return FALSE;
    submod = PyImport_ImportModule ((char*) "moo.app");
    PyModule_AddObject (_moo_module, (char*) "app", submod);
#endif

    code = Py_CompileString (MOO_PY, "moo/__init__.py", Py_file_input);

    if (!code)
        return FALSE;

    moo_mod = PyImport_ExecCodeModule ((char*) "moo", code);
    Py_DECREF (code);

    if (!moo_mod)
        return FALSE;

    return TRUE;
}
