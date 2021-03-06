#ifndef MOO_FNMATCH_H
#define MOO_FNMATCH_H

// #include "mooutils/mooutils-misc.h"
// #include "mooutils/mooutils-fs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define fnmatch _moo_win32_fnmatch
int _moo_win32_fnmatch  (const char *pattern,
                         const char *string,
                         int         flags);

#ifdef __cplusplus
}
#endif

#endif /* MOO_FNMATCH_H */
