#ifndef MOO_SYS_TIME_H
#define MOO_SYS_TIME_H

/* for struct timeval */
#include <winsock.h>

#ifdef __cplusplus
extern "C" {
#endif

#define gettimeofday _moo_win32_gettimeofday
int _moo_win32_gettimeofday (struct timeval *tp,
                             void           *tzp);

#ifdef __cplusplus
}
#endif

#endif /* MOO_SYS_TIME_H */
