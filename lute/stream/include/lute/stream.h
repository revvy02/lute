#pragma once

#include "lua.h"
#include "lualib.h"

struct uv_stream_s;
typedef struct uv_stream_s uv_stream_t;

int luteopen_stream(lua_State* L);

// Called by process module to create Stream userdatas wrapping pipes.
void pushStream(lua_State* L, uv_stream_t* handle, bool readable, bool writable);

namespace stream
{

int read(lua_State* L);
int write(lua_State* L);
int close(lua_State* L);

static const luaL_Reg lib[] = {
    {"read", read},
    {"write", write},
    {"close", close},
    {nullptr, nullptr}
};

} // namespace stream
