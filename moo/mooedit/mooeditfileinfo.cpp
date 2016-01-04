/**
 * class:MooOpenInfo: (parent GObject): information for opening a file
 *
 * Object which contains filename, character encoding, line
 * number, and options to use in moo_editor_open_file().
 **/

/**
 * class:MooSaveInfo: (parent GObject): information for saving a file
 *
 * Object which contains a filename and character encoding to
 * use in moo_editor_save() and moo_editor_save_as().
 **/

/**
 * class:MooReloadInfo: (parent GObject): information for reloading a file
 *
 * Object which contains character encoding and line number to
 * use in moo_editor_reload().
 **/

#include "mooeditfileinfo-impl.h"
#include <mooutils/mooutils-misc.h>
#include <moocpp/gobjectutils.h>

using namespace moo;

static void moo_open_info_class_init   (MooOpenInfoClass *klass);
static void moo_save_info_class_init   (MooSaveInfoClass *klass);
static void moo_reload_info_class_init (MooReloadInfoClass *klass);

MOO_DEFINE_OBJECT_ARRAY (MooOpenInfo, moo_open_info)

G_DEFINE_TYPE (MooOpenInfo, moo_open_info, G_TYPE_OBJECT)
G_DEFINE_TYPE (MooSaveInfo, moo_save_info, G_TYPE_OBJECT)
G_DEFINE_TYPE (MooReloadInfo, moo_reload_info, G_TYPE_OBJECT)

/**
 * moo_open_info_new_file: (static-method-of MooOpenInfo) (moo-kwargs)
 *
 * @file:
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 * @line: (type index) (default -1)
 * @flags: (default 0)
 *
 * Returns: (transfer full)
 **/
MooOpenInfo *
moo_open_info_new_file (GFile       *file,
                        const char  *encoding,
                        int          line,
                        MooOpenFlags flags)
{
    g_return_val_if_fail (G_IS_FILE (file), nullptr);

    MooOpenInfo *info = MOO_OPEN_INFO (g_object_new (MOO_TYPE_OPEN_INFO, nullptr));
    new(info)(MooOpenInfo);

    info->file = wrap_new(g_file_dup(file));
    info->encoding = gstr::make_copy(encoding);
    info->line = line;
    info->flags = flags;

    return info;
}

/**
 * moo_open_info_new: (constructor-of MooOpenInfo) (moo-kwargs)
 *
 * @path: (type const-filename)
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 * @line: (type index) (default -1)
 * @flags: (default 0)
 *
 * Returns: (transfer full)
 **/
MooOpenInfo *
moo_open_info_new (const char  *path,
                   const char  *encoding,
                   int          line,
                   MooOpenFlags flags)
{
    GFile *file = g_file_new_for_path (path);
    MooOpenInfo *info = moo_open_info_new_file (file, encoding, line, flags);
    g_object_unref (file);
    return info;
}

/**
 * moo_open_info_new_uri: (static-method-of MooOpenInfo) (moo-kwargs)
 *
 * @uri: (type const-utf8)
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 * @line: (type index) (default -1)
 * @flags: (default 0)
 *
 * Returns: (transfer full)
 **/
MooOpenInfo *
moo_open_info_new_uri (const char  *uri,
                       const char  *encoding,
                       int          line,
                       MooOpenFlags flags)
{
    GFile *file = g_file_new_for_uri (uri);
    MooOpenInfo *info = moo_open_info_new_file (file, encoding, line, flags);
    g_object_unref (file);
    return info;
}

/**
 * moo_open_info_dup:
 *
 * Returns: (transfer full)
 **/
MooOpenInfo *
moo_open_info_dup (MooOpenInfo *info)
{
    MooOpenInfo *copy;

    g_return_val_if_fail (info != NULL, NULL);

    copy = moo_open_info_new_file (info->file.get(), info->encoding, info->line, info->flags);
    g_return_val_if_fail (copy != NULL, NULL);

    return copy;
}

void
moo_open_info_free (MooOpenInfo *info)
{
    if (info)
        g_object_unref (info);
}


/**
 * moo_open_info_get_filename: (moo.private 1)
 *
 * Returns: (type filename)
 **/
char *
moo_open_info_get_filename (MooOpenInfo *info)
{
    g_return_val_if_fail(MOO_IS_OPEN_INFO(info), NULL);
    return info->file->get_path().release_owned();
}

/**
 * moo_open_info_get_uri: (moo.private 1)
 *
 * Returns: (type utf8)
 **/
char *
moo_open_info_get_uri (MooOpenInfo *info)
{
    g_return_val_if_fail(MOO_IS_OPEN_INFO(info), NULL);
    return info->file->get_uri().release_owned();
}

/**
 * moo_open_info_get_file: (moo.private 1)
 *
 * Returns: (transfer full)
 **/
GFile *
moo_open_info_get_file (MooOpenInfo *info)
{
    g_return_val_if_fail(MOO_IS_OPEN_INFO(info), NULL);
    return info->file->dup().release();
}

/**
 * moo_open_info_get_encoding: (moo.private 1)
 *
 * Returns: (type const-utf8)
 **/
const char *
moo_open_info_get_encoding (MooOpenInfo *info)
{
    g_return_val_if_fail(MOO_IS_OPEN_INFO(info), NULL);
    return info->encoding;
}

/**
 * moo_open_info_set_encoding: (moo.private 1)
 *
 * @info:
 * @encoding: (type const-utf8) (allow-none)
 **/
void
moo_open_info_set_encoding (MooOpenInfo *info,
                            const char  *encoding)
{
    g_return_if_fail(MOO_IS_OPEN_INFO(info));
    info->encoding.copy(encoding);
}

/**
 * moo_open_info_get_line:
 *
 * Returns: (type index)
 **/
int
moo_open_info_get_line (MooOpenInfo *info)
{
    g_return_val_if_fail(MOO_IS_OPEN_INFO(info), -1);
    return info->line;
}

/**
 * moo_open_info_set_line:
 *
 * @info:
 * @line: (type index)
 **/
void
moo_open_info_set_line (MooOpenInfo *info,
                        int          line)
{
    g_return_if_fail(MOO_IS_OPEN_INFO(info));
    info->line = line;
}

/**
 * moo_open_info_get_flags:
 **/
MooOpenFlags
moo_open_info_get_flags (MooOpenInfo *info)
{
    g_return_val_if_fail(MOO_IS_OPEN_INFO(info), MOO_OPEN_FLAGS_NONE);
    return info->flags;
}

/**
 * moo_open_info_set_flags:
 **/
void
moo_open_info_set_flags(MooOpenInfo  *info,
                        MooOpenFlags  flags)
{
    g_return_if_fail(MOO_IS_OPEN_INFO(info));
    info->flags = flags;
}

/**
 * moo_open_info_add_flags:
 **/
void
moo_open_info_add_flags(MooOpenInfo  *info,
                        MooOpenFlags  flags)
{
    g_return_if_fail(MOO_IS_OPEN_INFO(info));
    info->flags |= flags;
}


static void
moo_open_info_finalize(GObject *object)
{
    MooOpenInfo *info = (MooOpenInfo*) object;
    info->~MooOpenInfo();
    G_OBJECT_CLASS(moo_open_info_parent_class)->finalize(object);
}

static void
moo_open_info_class_init(MooOpenInfoClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = moo_open_info_finalize;
}

static void
moo_open_info_init(MooOpenInfo *info)
{
    info->line = -1;
}


/**
 * moo_save_info_new_file: (static-method-of MooSaveInfo)
 *
 * @file:
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 *
 * Returns: (transfer full)
 **/
MooSaveInfo *
moo_save_info_new_file(GFile      *file,
                       const char *encoding)
{
    g_return_val_if_fail(G_IS_FILE(file), NULL);

    MooSaveInfo *info = MOO_SAVE_INFO(g_object_new(MOO_TYPE_SAVE_INFO, NULL));
    new(info)(MooSaveInfo);

    info->file.wrap_new(g_file_dup(file));
    info->encoding.copy(encoding);

    return info;
}

/**
 * moo_save_info_new: (constructor-of MooSaveInfo)
 *
 * @path: (type const-filename)
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 *
 * Returns: (transfer full)
 **/
MooSaveInfo *
moo_save_info_new(const char *path,
                  const char *encoding)
{
    auto file = gobj_ptr<GFile>::new_for_path(path);
    MooSaveInfo *info = moo_save_info_new_file(file.get(), encoding);
    return info;
}

/**
 * moo_save_info_new_uri: (static-method-of MooSaveInfo)
 *
 * @uri: (type const-utf8)
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 *
 * Returns: (transfer full)
 **/
MooSaveInfo *
moo_save_info_new_uri (const char *uri,
                       const char *encoding)
{
    GFile *file = g_file_new_for_uri (uri);
    MooSaveInfo *info = moo_save_info_new_file (file, encoding);
    g_object_unref (file);
    return info;
}

/**
 * moo_save_info_dup:
 *
 * Returns: (transfer full)
 **/
MooSaveInfo *
moo_save_info_dup (MooSaveInfo *info)
{
    MooSaveInfo *copy;

    g_return_val_if_fail (info != NULL, NULL);

    copy = moo_save_info_new_file (info->file.get(), info->encoding);
    g_return_val_if_fail (copy != NULL, NULL);

    return copy;
}

void
moo_save_info_free (MooSaveInfo *info)
{
    if (info)
        g_object_unref (info);
}

static void
moo_save_info_finalize (GObject *object)
{
    MooSaveInfo *info = (MooSaveInfo*) object;
    info->~MooSaveInfo();
    G_OBJECT_CLASS (moo_save_info_parent_class)->finalize (object);
}

static void
moo_save_info_class_init (MooSaveInfoClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = moo_save_info_finalize;
}

static void
moo_save_info_init (G_GNUC_UNUSED MooSaveInfo *info)
{
}


/**
 * moo_reload_info_new: (constructor-of MooReloadInfo)
 *
 * @encoding: (type const-utf8) (allow-none) (default NULL)
 * @line: (type index) (default -1)
 **/
MooReloadInfo *
moo_reload_info_new (const char *encoding,
                     int         line)
{
    MooReloadInfo *info = MOO_RELOAD_INFO (g_object_new (MOO_TYPE_RELOAD_INFO, NULL));
    new(info)(MooReloadInfo);

    info->encoding.copy(encoding);
    info->line = line;

    return info;
}

/**
 * moo_reload_info_dup:
 *
 * Returns: (transfer full)
 **/
MooReloadInfo *
moo_reload_info_dup (MooReloadInfo *info)
{
    MooReloadInfo *copy;

    g_return_val_if_fail (info != NULL, NULL);

    copy = moo_reload_info_new (info->encoding, info->line);
    g_return_val_if_fail (copy != NULL, NULL);

    return copy;
}

void
moo_reload_info_free (MooReloadInfo *info)
{
    if (info)
        g_object_unref (info);
}


/**
 * moo_reload_info_get_line:
 *
 * Returns: (type index)
 **/
int
moo_reload_info_get_line (MooReloadInfo *info)
{
    g_return_val_if_fail (MOO_IS_RELOAD_INFO (info), -1);
    return info->line;
}

/**
 * moo_reload_info_set_line:
 *
 * @info:
 * @line: (type index)
 **/
void
moo_reload_info_set_line (MooReloadInfo *info,
                          int            line)
{
    g_return_if_fail (MOO_IS_RELOAD_INFO (info));
    info->line = line;
}


static void
moo_reload_info_finalize (GObject *object)
{
    MooReloadInfo *info = (MooReloadInfo*) object;
    info->~MooReloadInfo();
    G_OBJECT_CLASS (moo_reload_info_parent_class)->finalize (object);
}

static void
moo_reload_info_class_init (MooReloadInfoClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = moo_reload_info_finalize;
}

static void
moo_reload_info_init (MooReloadInfo *info)
{
    info->line = -1;
}
