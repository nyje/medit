#ifndef MOO_EDIT_FILE_INFO_IMPL_H
#define MOO_EDIT_FILE_INFO_IMPL_H

#include "mooeditfileinfo.h"

G_BEGIN_DECLS

struct MooOpenInfo : public GObject
{
    GFile *file;
    char *encoding;
    int line;
    MooOpenFlags flags;
};

struct MooOpenInfoClass
{
    GObjectClass parent_class;
};

struct MooReloadInfo : public GObject {
    char *encoding;
    int line;
};

struct MooReloadInfoClass
{
    GObjectClass parent_class;
};

struct MooSaveInfo : public GObject {
    GFile *file;
    char *encoding;
};

struct MooSaveInfoClass
{
    GObjectClass parent_class;
};

G_END_DECLS

#endif /* MOO_EDIT_FILE_INFO_IMPL_H */
