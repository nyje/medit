/*
 *   mooplugin-python.c
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

#ifndef MOO_INSTALL_LIB
#define NO_IMPORT_PYGOBJECT
#endif

#include "pygobject.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "moopython/mooplugin-python.h"
#include "moopython/moopython-utils.h"
#include "mooutils/moopython.h"
#include "mooutils/mooutils-misc.h"
#include "moopython/pygtk/moo-pygtk.h"
#include "mooedit/mooplugin-loader.h"

#define PLUGIN_SUFFIX ".py"
#define LIBDIR "lib"


static PyObject *main_mod;
static gboolean in_moo_module;


static MooPyObject*
run_simple_string (const char *str)
{
    PyObject *dict;
    g_return_val_if_fail (str != NULL, NULL);
    dict = PyModule_GetDict (main_mod);
    return (MooPyObject*) PyRun_String (str, Py_file_input, dict, dict);
}


static MooPyObject*
run_string (const char  *str,
            MooPyObject *locals,
            MooPyObject *globals)
{
    g_return_val_if_fail (str != NULL, NULL);
    g_return_val_if_fail (locals != NULL, NULL);
    g_return_val_if_fail (globals != NULL, NULL);
    return (MooPyObject*) PyRun_String (str, Py_file_input,
                                        (PyObject*) locals,
                                        (PyObject*) globals);
}


static MooPyObject*
run_file (gpointer    fp,
          const char *filename)
{
    PyObject *dict;
    g_return_val_if_fail (fp != NULL && filename != NULL, NULL);
    dict = PyModule_GetDict (main_mod);
    return (MooPyObject*) PyRun_File (fp, filename, Py_file_input, dict, dict);
}


static MooPyObject*
incref (MooPyObject *obj)
{
    if (obj)
    {
        Py_INCREF ((PyObject*) obj);
    }

    return obj;
}

static void
decref (MooPyObject *obj)
{
    if (obj)
    {
        Py_DECREF ((PyObject*) obj);
    }
}


static MooPyObject *
py_object_from_gobject (gpointer gobj)
{
    g_return_val_if_fail (!gobj || G_IS_OBJECT (gobj), NULL);
    return (MooPyObject*) pygobject_new (gobj);
}


static MooPyObject *
dict_get_item (MooPyObject *dict,
               const char  *key)
{
    g_return_val_if_fail (dict != NULL, NULL);
    g_return_val_if_fail (key != NULL, NULL);
    return (MooPyObject*) PyDict_GetItemString ((PyObject*) dict, (char*) key);
}

static gboolean
dict_set_item (MooPyObject *dict,
               const char  *key,
               MooPyObject *val)
{
    g_return_val_if_fail (dict != NULL, FALSE);
    g_return_val_if_fail (key != NULL, FALSE);
    g_return_val_if_fail (val != NULL, FALSE);

    if (PyDict_SetItemString ((PyObject*) dict, (char*) key, (PyObject*) val))
    {
        PyErr_Print ();
        return FALSE;
    }

    return TRUE;
}

static gboolean
dict_del_item (MooPyObject *dict,
               const char  *key)
{
    g_return_val_if_fail (dict != NULL, FALSE);
    g_return_val_if_fail (key != NULL, FALSE);

    if (PyDict_DelItemString ((PyObject*) dict, (char*) key))
    {
        PyErr_Print ();
        return FALSE;
    }

    return TRUE;
}


static MooPyObject *
get_script_dict (const char *name)
{
    PyObject *dict, *builtins;

    builtins = PyImport_ImportModule ((char*) "__builtin__");
    g_return_val_if_fail (builtins != NULL, NULL);

    dict = PyDict_New ();
    PyDict_SetItemString (dict, (char*) "__builtins__", builtins);

    if (name)
    {
        PyObject *py_name = PyString_FromString (name);
        PyDict_SetItemString (dict, (char*) "__name__", py_name);
        Py_XDECREF (py_name);
    }

    Py_XDECREF (builtins);
    return (MooPyObject*) dict;
}


static void
err_print (void)
{
    PyErr_Print ();
}


static char *
get_info (void)
{
    return g_strdup_printf ("%s %s", Py_GetVersion (), Py_GetPlatform ());
}


static gboolean
moo_python_api_init (void)
{
    static int argc;
    static char **argv;

    static MooPyAPI api = {
        incref, decref, err_print,
        get_info,
        run_simple_string, run_string, run_file,
        py_object_from_gobject,
        get_script_dict,
        dict_get_item, dict_set_item, dict_del_item
    };

    g_return_val_if_fail (!moo_python_running(), FALSE);

    if (!moo_python_init (MOO_PY_API_VERSION, &api))
    {
        g_warning ("%s: oops", G_STRLOC);
        return FALSE;
    }

    g_assert (moo_python_running ());

    if (!argv)
    {
        argc = 1;
        argv = g_new0 (char*, 2);
        argv[0] = g_strdup ("");
    }

#if PY_MINOR_VERSION >= 4
    /* do not let python install signal handlers */
    Py_InitializeEx (FALSE);
#else
    Py_Initialize ();
#endif

    /* pygtk wants sys.argv */
    PySys_SetArgv (argc, argv);

    moo_py_init_print_funcs ();

    main_mod = PyImport_AddModule ((char*)"__main__");

    if (!main_mod)
    {
        g_warning ("%s: could not import __main__", G_STRLOC);
        PyErr_Print ();
        moo_python_init (MOO_PY_API_VERSION, NULL);
        return FALSE;
    }

    Py_XINCREF ((PyObject*) main_mod);
    return TRUE;
}


// static void
// moo_python_plugin_read_file (const char *path)
// {
//     PyObject *mod, *code;
//     char *modname = NULL, *content = NULL;
//     GError *error = NULL;
//
//     g_return_if_fail (path != NULL);
//
//     if (!g_file_get_contents (path, &content, NULL, &error))
//     {
//         g_warning ("%s: could not read plugin file", G_STRLOC);
//         g_warning ("%s: %s", G_STRLOC, error->message);
//         g_error_free (error);
//         return;
//     }
//
//     modname = g_strdup_printf ("moo_plugin_%08x%08x", g_random_int (), g_random_int ());
//     code = Py_CompileString (content, path, Py_file_input);
//
//     if (!code)
//     {
//         PyErr_Print ();
//         goto out;
//     }
//
//     mod = PyImport_ExecCodeModule (modname, code);
//     Py_DECREF (code);
//
//     if (!mod)
//     {
//         PyErr_Print ();
//         goto out;
//     }
//
//     Py_DECREF (mod);
//
// out:
//     g_free (content);
//     g_free (modname);
// }


// static void
// moo_python_plugin_read_dir (const char *path)
// {
//     GDir *dir;
//     const char *name;
//
//     g_return_if_fail (path != NULL);
//
//     dir = g_dir_open (path, 0, NULL);
//
//     if (!dir)
//         return;
//
//     while ((name = g_dir_read_name (dir)))
//     {
//         char *file_path, *prefix, *suffix;
//
//         if (!g_str_has_suffix (name, PLUGIN_SUFFIX))
//             continue;
//
//         suffix = g_strrstr (name, PLUGIN_SUFFIX);
//         prefix = g_strndup (name, suffix - name);
//
//         file_path = g_build_filename (path, name, NULL);
//
//         /* don't try broken links */
//         if (g_file_test (file_path, G_FILE_TEST_EXISTS))
//             moo_python_plugin_read_file (file_path);
//
//         g_free (prefix);
//         g_free (file_path);
//     }
//
//     g_dir_close (dir);
// }


// static void
// moo_python_plugin_read_dirs (void)
// {
//     char **d;
//     char **dirs = moo_plugin_get_dirs ();
//     PyObject *sys = NULL, *path = NULL;
//
//     sys = PyImport_ImportModule ((char*) "sys");
//
//     if (sys)
//         path = PyObject_GetAttrString (sys, (char*) "path");
//
//     if (!path || !PyList_Check (path))
//     {
//         g_critical ("%s: oops", G_STRLOC);
//     }
//     else
//     {
//         for (d = dirs; d && *d; ++d)
//         {
//             char *libdir = g_build_filename (*d, LIBDIR, NULL);
//             PyObject *s = PyString_FromString (libdir);
//             PyList_Append (path, s);
//             Py_XDECREF (s);
//             g_free (libdir);
//         }
//     }
//
//     for (d = dirs; d && *d; ++d)
//         moo_python_plugin_read_dir (*d);
//
//     g_strfreev (dirs);
// }


static gboolean
init_moo_pygtk (void)
{
#ifndef MOO_INSTALL_LIB
    return _moo_pygtk_init ();
#else
    PyObject *module = PyImport_ImportModule ((char*) "moo");

    if (!module)
        PyErr_SetString(PyExc_ImportError,
                        "could not import moo");

    return module != NULL;
#endif
}


static gboolean
moo_python_stuff_init (void)
{
    if (!in_moo_module && !main_mod)
    {
        if (!moo_python_api_init ())
            return FALSE;

        if (!init_moo_pygtk ())
        {
            PyErr_Print ();
            moo_python_init (MOO_PY_API_VERSION, NULL);
            return FALSE;
        }

#ifdef pyg_disable_warning_redirections
        pyg_disable_warning_redirections ();
#else
        moo_reset_log_func ();
#endif
    }

    return TRUE;
}


gboolean
_moo_python_plugin_init (void)
{
    MooPluginLoader *loader;

    if (!moo_python_stuff_init ())
        return FALSE;

    loader = _moo_python_get_plugin_loader ();
    moo_plugin_loader_register (loader, MOO_PYTHON_PLUGIN_LOADER_ID);
    g_object_unref (loader);

    return TRUE;
}


void
_moo_python_plugin_deinit (void)
{
    Py_XDECREF (main_mod);
    main_mod = NULL;
    moo_python_init (MOO_PY_API_VERSION, NULL);
}


#ifdef MOO_PYTHON_PLUGIN
MOO_MODULE_INIT_FUNC_DECL;
MOO_MODULE_INIT_FUNC_DECL
{
    return !moo_python_running () &&
            _moo_python_plugin_init ();
}
#endif


#ifdef MOO_PYTHON_MODULE
void initmoo (void)
{
    in_moo_module = TRUE;

    if (!moo_python_running())
        moo_python_api_init ();

    _moo_pygtk_init ();
}
#endif


/***************************************************************************/
/* Python plugin loader
 */

GType _moo_python_plugin_loader_get_type (void);
typedef MooPluginLoader MooPythonPluginLoader;
typedef MooPluginLoaderClass MooPythonPluginLoaderClass;
G_DEFINE_TYPE (MooPythonPluginLoader, _moo_python_plugin_loader, MOO_TYPE_PLUGIN_LOADER)

static PyObject *sys_module;

static gboolean
sys_path_add_dir (const char *dir)
{
    PyObject *path;
    PyObject *s;

    if (!sys_module)
        sys_module = PyImport_ImportModule ((char*) "sys");

    if (!sys_module)
    {
        PyErr_Print ();
        return FALSE;
    }

    path = PyObject_GetAttrString (sys_module, (char*) "path");

    if (!path)
    {
        PyErr_Print ();
        return FALSE;
    }

    if (!PyList_Check (path))
    {
        g_critical ("sys.path is not a list");
        Py_DECREF (path);
        return FALSE;
    }

    s = PyString_FromString (dir);
    PyList_Append (path, s);

    Py_DECREF (s);
    Py_DECREF (path);
    return TRUE;
}

static void
sys_path_remove_dir (const char *dir)
{
    PyObject *path;
    int i;

    if (!sys_module)
        return;

    path = PyObject_GetAttrString (sys_module, (char*) "path");

    if (!path || !PyList_Check (path))
        return;

    for (i = PyList_GET_SIZE (path) - 1; i >= 0; --i)
    {
        PyObject *item = PyList_GET_ITEM (path, i);

        if (PyString_CheckExact (item) &&
            !strcmp (PyString_AsString (item), dir))
        {
            if (PySequence_DelItem (path, i) != 0)
                PyErr_Print ();
            break;
        }
    }

    Py_DECREF (path);
}

static void
sys_path_add_plugin_dirs (void)
{
    char **d;
    char **dirs = moo_plugin_get_dirs ();
    static gboolean been_here = FALSE;

    if (been_here)
        return;

    been_here = TRUE;

    for (d = dirs; d && *d; ++d)
    {
        char *libdir = g_build_filename (*d, LIBDIR, NULL);
        sys_path_add_dir (libdir);
        g_free (libdir);
    }

    g_strfreev (dirs);
}


static PyObject *
do_load_file (const char *path)
{
    PyObject *mod = NULL;
    PyObject *code;
    char *modname = NULL, *content = NULL;
    GError *error = NULL;

    if (!g_file_get_contents (path, &content, NULL, &error))
    {
        g_warning ("could not read file '%s': %s", path, error->message);
        g_error_free (error);
        return NULL;
    }

    modname = g_strdup_printf ("moo_module_%08x", g_random_int ());
    code = Py_CompileString (content, path, Py_file_input);

    if (!code)
    {
        PyErr_Print ();
        goto out;
    }

    mod = PyImport_ExecCodeModule (modname, code);
    Py_DECREF (code);

    if (!mod)
    {
        PyErr_Print ();
        goto out;
    }

out:
    g_free (content);
    g_free (modname);

    return mod;
}


static PyObject *
load_file (const char *path)
{
    char *dirname;
    gboolean dir_added;
    PyObject *retval;

    g_return_val_if_fail (path != NULL, NULL);

    if (!moo_python_stuff_init ())
        return NULL;

    sys_path_add_plugin_dirs ();

    dirname = g_path_get_dirname (path);
    dir_added = sys_path_add_dir (dirname);

    retval = do_load_file (path);

    if (dir_added)
        sys_path_remove_dir (dirname);

    g_free (dirname);

    return retval;
}


static void
load_python_module (G_GNUC_UNUSED MooPluginLoader *loader,
                    const char      *module_file,
                    G_GNUC_UNUSED const char *ini_file)
{
    Py_XDECREF (load_file (module_file));
}


static void
load_python_plugin (G_GNUC_UNUSED MooPluginLoader *loader,
                    const char      *plugin_file,
                    const char      *plugin_id,
                    MooPluginInfo   *info,
                    MooPluginParams *params,
                    G_GNUC_UNUSED const char *ini_file)
{
    PyObject *mod;
    PyObject *py_plugin_type;
    GType plugin_type;

    if (!(mod = load_file (plugin_file)))
        return;

    py_plugin_type = PyObject_GetAttrString (mod, (char*) "__plugin__");

    if (!py_plugin_type)
    {
        g_warning ("file %s doesn't define __plugin__ attribute",
                   plugin_file);
    }
    else if (py_plugin_type == Py_None)
    {
        /* it's fine, ignore */
    }
    else if (!PyType_Check (py_plugin_type))
    {
        g_warning ("__plugin__ attribute in file %s is not a type",
                   plugin_file);
    }
    else if (!(plugin_type = pyg_type_from_object (py_plugin_type)))
    {
        g_warning ("__plugin__ attribute in file %s is not a valid type",
                   plugin_file);
        PyErr_Clear ();
    }
    else if (!g_type_is_a (plugin_type, MOO_TYPE_PLUGIN))
    {
        g_warning ("__plugin__ attribute in file %s is not a MooPlugin subclass",
                   plugin_file);
    }
    else
    {
        moo_plugin_register (plugin_id, plugin_type, info, params);
    }

    Py_XDECREF (py_plugin_type);
    Py_XDECREF (mod);
}


static void
_moo_python_plugin_loader_class_init (MooPluginLoaderClass *klass)
{
    klass->load_module = load_python_module;
    klass->load_plugin = load_python_plugin;
}


static void
_moo_python_plugin_loader_init (G_GNUC_UNUSED MooPluginLoader *loader)
{
}


gpointer
_moo_python_get_plugin_loader (void)
{
    return g_object_new (_moo_python_plugin_loader_get_type (), NULL);
}
