#pragma once

#include "lua.h"
#include "lualib.h"

// open the library as a standard global luau library
int luaopen_fs(lua_State* L);
// open the library as a table on top of the stack
int luteopen_fs(lua_State* L);

namespace fs
{

/* Takes  path: string, a mode: 'r|a|w|x|+' (defaulting to r when omitted)
   Returns a Stream backed by the opened file descriptor */
int open(lua_State* L);

/* Reads from a file handle (Stream or legacy lightuserdata).
   Returns buffer for Stream handles, string for legacy handles. */
int read(lua_State* L);

/* Writes to a file handle (Stream or legacy lightuserdata). */
int write(lua_State* L);

/* Closes a file handle (Stream or legacy lightuserdata). */
int close(lua_State* L);

/* Removes a file */
int remove(lua_State* L);

/* Creates a folder */
int mkdir(lua_State* L);

/* Removes a directory */
int rmdir(lua_State* L);

/* Gets the metadata of a file */
int stat(lua_State* L);

/* Checks if a file exists */
int exists(lua_State* L);

/* Copies a file to another path */
int copy(lua_State* L);

/* Creates a link to a file */
int link(lua_State* L);

/* Creates a symlink to a file */
int symlink(lua_State* L);

/* Gets the type of a file entry */
int type(lua_State* L);

/* Sets up a filesystem watch event */
int fs_watch(lua_State* L);

/* Lists the contents of a directory */
int listdir(lua_State* L);

static const luaL_Reg lib[] = {
    {"open", open},
    {"read", read},
    {"write", write},
    {"close", close},

    {"remove", remove},

    {"stat", stat},
    {"exists", exists},
    {"type", type},

    {"watch", fs_watch},
    {"link", link},
    {"symlink", symlink},
    {"copy", copy},

    {"mkdir", mkdir},
    {"listdir", listdir},
    {"rmdir", rmdir},

    {NULL, NULL},
};

} // namespace fs
