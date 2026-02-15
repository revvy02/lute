#include "lute/stream.h"

#include "lute/runtime.h"
#include "lute/userdatas.h"

#include "lua.h"
#include "lualib.h"

#include "uv.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace stream
{

struct LuaStream
{
    uv_stream_t* handle = nullptr;
    bool readable = false;
    bool writable = false;
    bool closed = false;
    bool eof = false;
    bool ownsHandle = false;
};

static LuaStream* checkStream(lua_State* L, int idx)
{
    LuaStream* s = static_cast<LuaStream*>(lua_touserdatatagged(L, idx, kStreamTag));
    if (!s)
        luaL_typeerrorL(L, idx, "Stream");
    return s;
}

// State for an in-flight read operation
struct ReadState
{
    ResumeToken token;
    LuaStream* stream = nullptr;
    std::vector<char> buffer;
};

static void readAllocCallback(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf)
{
    ReadState* state = static_cast<ReadState*>(handle->data);
    state->buffer.resize(suggestedSize);
    buf->base = state->buffer.data();
    buf->len = state->buffer.size();
}

static void readCallback(uv_stream_t* uvStream, ssize_t nread, const uv_buf_t* buf)
{
    ReadState* state = static_cast<ReadState*>(uvStream->data);

    uv_read_stop(uvStream);

    if (nread > 0)
    {
        std::string data(buf->base, nread);
        state->token->complete(
            [data = std::move(data)](lua_State* L) -> int
            {
                void* bufData = lua_newbuffer(L, data.size());
                memcpy(bufData, data.data(), data.size());
                return 1;
            }
        );
    }
    else if (nread == UV_EOF)
    {
        state->stream->eof = true;
        state->token->complete(
            [](lua_State* L) -> int
            {
                lua_pushnil(L);
                return 1;
            }
        );
    }
    else if (nread < 0)
    {
        state->stream->eof = true;
        state->token->fail(std::string("stream read error: ") + uv_strerror(nread));
    }

    // Restore original handle data and clean up
    uvStream->data = nullptr;
    delete state;
}

int read(lua_State* L)
{
    LuaStream* s = checkStream(L, 1);

    if (!s->readable)
        luaL_errorL(L, "stream is not readable");
    if (s->closed)
        luaL_errorL(L, "stream is closed");
    if (s->eof)
    {
        lua_pushnil(L);
        return 1;
    }

    auto token = getResumeToken(L);

    ReadState* state = new ReadState();
    state->token = std::move(token);
    state->stream = s;

    s->handle->data = state;

    int err = uv_read_start(s->handle, readAllocCallback, readCallback);
    if (err < 0)
    {
        s->handle->data = nullptr;
        state->token->runtime->releasePendingToken();
        delete state;
        luaL_errorL(L, "failed to start reading: %s", uv_strerror(err));
    }

    return lua_yield(L, 0);
}

// State for an in-flight write operation
struct WriteState
{
    uv_write_t req;
    ResumeToken token;
    std::string data;
};

static void writeCallback(uv_write_t* req, int status)
{
    WriteState* state = static_cast<WriteState*>(req->data);

    if (status < 0)
    {
        state->token->fail(std::string("stream write error: ") + uv_strerror(status));
    }
    else
    {
        state->token->complete(
            [](lua_State*) -> int
            {
                return 0;
            }
        );
    }

    delete state;
}

int write(lua_State* L)
{
    LuaStream* s = checkStream(L, 1);

    if (!s->writable)
        luaL_errorL(L, "stream is not writable");
    if (s->closed)
        luaL_errorL(L, "stream is closed");

    size_t len = 0;
    const char* data = nullptr;

    if (lua_isbuffer(L, 2))
    {
        data = static_cast<const char*>(lua_tobuffer(L, 2, &len));
    }
    else
    {
        data = luaL_checklstring(L, 2, &len);
    }

    auto token = getResumeToken(L);

    WriteState* state = new WriteState();
    state->token = std::move(token);
    state->data.assign(data, len);
    state->req.data = state;

    uv_buf_t buf = uv_buf_init(state->data.data(), state->data.size());

    int err = uv_write(&state->req, s->handle, &buf, 1, writeCallback);
    if (err < 0)
    {
        state->token->runtime->releasePendingToken();
        delete state;
        luaL_errorL(L, "failed to write: %s", uv_strerror(err));
    }

    return lua_yield(L, 0);
}

int close(lua_State* L)
{
    LuaStream* s = checkStream(L, 1);

    if (s->closed)
        return 0;

    s->closed = true;

    if (!uv_is_closing((uv_handle_t*)s->handle))
    {
        if (s->ownsHandle)
        {
            uv_close(
                (uv_handle_t*)s->handle,
                [](uv_handle_t* handle)
                {
                    delete handle;
                }
            );
        }
        else
        {
            uv_close((uv_handle_t*)s->handle, nullptr);
        }
    }

    return 0;
}

} // namespace stream

void pushStream(lua_State* L, uv_stream_t* handle, bool readable, bool writable)
{
    stream::LuaStream* s = static_cast<stream::LuaStream*>(
        lua_newuserdatataggedwithmetatable(L, sizeof(stream::LuaStream), kStreamTag));
    new (s) stream::LuaStream{};
    s->handle = handle;
    s->readable = readable;
    s->writable = writable;
}

// Parent stdio backing handles — heap-allocated, live for process lifetime
struct StdioHandles
{
    union
    {
        uv_pipe_t pipe;
        uv_tty_t tty;
    } handles[3];
    bool isTty[3] = {};
};

static StdioHandles* initStdioHandles(uv_loop_t* loop)
{
    auto* stdio = new StdioHandles();

    int fds[3] = {fileno(stdin), fileno(stdout), fileno(stderr)};

    for (int i = 0; i < 3; i++)
    {
        uv_handle_type ht = uv_guess_handle(fds[i]);

        if (ht == UV_TTY)
        {
            stdio->isTty[i] = true;
            uv_tty_init(loop, &stdio->handles[i].tty, fds[i], (i == 0) ? 1 : 0);
        }
        else
        {
            stdio->isTty[i] = false;
            uv_pipe_init(loop, &stdio->handles[i].pipe, 0);
            uv_pipe_open(&stdio->handles[i].pipe, fds[i]);
        }

        // Unref so parent stdio handles don't keep the event loop alive.
        // Without this, piped stdio (e.g. on CI) prevents clean shutdown.
        uv_unref((uv_handle_t*)&stdio->handles[i]);
    }

    return stdio;
}

static uv_stream_t* getStdioStream(StdioHandles* stdio, int index)
{
    if (stdio->isTty[index])
        return (uv_stream_t*)&stdio->handles[index].tty;
    else
        return (uv_stream_t*)&stdio->handles[index].pipe;
}

int luteopen_stream(lua_State* L)
{
    // Set up Stream metatable
    luaL_newmetatable(L, "Stream");

    lua_pushstring(L, "Stream");
    lua_setfield(L, -2, "__type");

    lua_setuserdatadtor(
        L,
        kStreamTag,
        [](lua_State*, void* ud)
        {
            std::destroy_at(static_cast<stream::LuaStream*>(ud));
        }
    );

    lua_setuserdatametatable(L, kStreamTag);

    // Build lib table
    lua_createtable(L, 0, std::size(stream::lib) + 3); // +3 for stdin/stdout/stderr

    for (auto& [name, func] : stream::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    // Init parent stdio streams
    uv_loop_t* loop = getRuntimeLoop(L);
    StdioHandles* stdio = initStdioHandles(loop);

    // stdin — readable
    pushStream(L, getStdioStream(stdio, 0), true, false);
    lua_setfield(L, -2, "stdin");

    // stdout — writable
    pushStream(L, getStdioStream(stdio, 1), false, true);
    lua_setfield(L, -2, "stdout");

    // stderr — writable
    pushStream(L, getStdioStream(stdio, 2), false, true);
    lua_setfield(L, -2, "stderr");

    lua_setreadonly(L, -1, 1);

    return 1;
}
