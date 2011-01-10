#ifndef MEDIT_LUA_H
#define MEDIT_LUA_H

#include <glib-object.h>

#ifdef __cplusplus

#include "moolua/lua/moolua.h"

bool         medit_lua_setup            (lua_State *L,
                                         bool       default_init);

lua_State   *medit_lua_new              (bool        default_init);
void         medit_lua_free             (lua_State  *L);

bool         medit_lua_do_string        (lua_State  *L,
                                         const char *string);
bool         medit_lua_do_file          (lua_State  *L,
                                         const char *filename);

#endif // __cplusplus

G_BEGIN_DECLS

#define MOO_TYPE_LUA_STATE      (moo_lua_state_get_type ())
#define MOO_LUA_STATE(object)   (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_LUA_STATE, MooLuaState))

typedef struct MooLuaState MooLuaState;

typedef enum {
    MOO_LUA_RUN_OK,
    MOO_LUA_RUN_INCOMPLETE,
    MOO_LUA_RUN_ERROR
} MooLuaRunResult;

GType        moo_lua_state_get_type     (void) G_GNUC_CONST;
char        *moo_lua_state_run_string   (MooLuaState    *lstate,
                                         const char     *string);

void         medit_lua_run_string       (const char     *string);
void         medit_lua_run_file         (const char     *filename);

G_END_DECLS

#endif // MEDIT_LUA_H
