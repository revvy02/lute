#include "fs_impl.h"

#include "lute/fileutils.h"
#include "lute/stream.h"
#include "lute/time.h"
#include "lute/UVRequest.h"

#include "lua.h"
#include "lualib.h"

#include "uv.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#if !defined(S_ISCHR) && defined(S_IFMT) && defined(S_IFCHR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif

#if !defined(S_ISLNK) && defined(S_IFMT) && defined(S_IFLNK)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

#if !defined(S_ISFIFO) && defined(S_IFMT) && defined(S_IFIFO)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif

namespace fs
{

using FSRequest = uvutils::UVRequest<uv_fs_t>;

struct FSOpen : FSRequest
{
    FSOpen(lua_State* L, bool readable, bool writable)
        : FSRequest(L)
        , readable(readable)
        , writable(writable)
    {
    }

    bool readable;
    bool writable;
};

struct FSPathPairRequest : FSRequest
{
    FSPathPairRequest(lua_State* L, const char* src, const char* dest)
        : FSRequest(L)
        , src(src)
        , dest(dest)
    {
    }

    const std::string src;
    const std::string dest;
};

int open_impl(lua_State* L, const char* path, int flags, int mode)
{
    bool readable = (flags & O_RDWR) || !(flags & O_WRONLY);
    bool writable = (flags & O_RDWR) || (flags & O_WRONLY);

    uvutils::ScopedUVRequest<FSOpen> req(L, readable, writable);
    uv_fs_open(
        req->getLoop(),
        &req->req,
        path,
        flags,
        mode,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSOpen>(req);
            auto result = req->result;
            if (result < 0)
            {
                r->fail("Error opening file %s: %s", req->path, uv_strerror(result));
                return;
            }

            bool readable = r->readable;
            bool writable = r->writable;

            r->succeed(
                [result, readable, writable](lua_State* L)
                {
                    pushStreamFromFd(L, result, readable, writable);
                    return 1;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int remove_impl(lua_State* L, const char* path)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_unlink(
        req->getLoop(),
        &req->req,
        path,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;
            if (result < 0)
            {
                r->fail("Error removing file %s: %s", req->path, uv_strerror(result));
                return;
            }

            r->succeed(
                [](lua_State* L)
                {
                    return 0;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

static const char* fileModeToType(uint64_t mode)
{
    if (S_ISDIR(mode))
    {
        return UV_TYPENAME_DIR;
    }
    else if (S_ISREG(mode))
    {
        return UV_TYPENAME_FILE;
    }
    else if (S_ISCHR(mode))
    {
        return UV_TYPENAME_CHAR;
    }
    else if (S_ISLNK(mode))
    {
        return UV_TYPENAME_LINK;
    }
#ifdef S_ISBLK
    else if (S_ISBLK(mode))
    {
        return UV_TYPENAME_BLOCK;
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(mode))
    {
        return UV_TYPENAME_FIFO;
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(mode))
    {
        return UV_TYPENAME_SOCKET;
    }
#endif
    else
    {
        return UV_TYPENAME_UNKNOWN;
    }
}

static int createDurationFromTimespec32(lua_State* L, uv_timespec_t timespec)
{
    uv_timespec64_t extended{static_cast<int64_t>(timespec.tv_sec), static_cast<int32_t>(timespec.tv_nsec)};
    return createDurationFromTimespec(L, extended);
}

int stat_impl(lua_State* L, const char* path)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_stat(
        req->getLoop(),
        &req->req,
        path,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;

            if (result < 0)
            {
                r->fail("Error getting metadata of file %s: %s", req->path, uv_strerror(result));
                return;
            }

            r->succeed(
                [stat = req->statbuf](lua_State* L)
                {
                    lua_createtable(L, 0, 6);

                    auto type = fileModeToType(stat.st_mode);
                    lua_pushstring(L, type);
                    lua_setfield(L, -2, "type");

                    // this is fine unless the file is 9 petabytes
                    lua_pushnumber(L, static_cast<double>(stat.st_size));
                    lua_setfield(L, -2, "size");

                    createDurationFromTimespec32(L, stat.st_birthtim);
                    lua_setfield(L, -2, "created");

                    createDurationFromTimespec32(L, stat.st_atim);
                    lua_setfield(L, -2, "accessed");

                    createDurationFromTimespec32(L, stat.st_mtim);
                    lua_setfield(L, -2, "modified");

                    // permissions
                    lua_createtable(L, 0, 2);

                    // libuv writes this correctly cross-platform
                    bool canAnyWrite = stat.st_mode & 0222;
                    lua_pushboolean(L, !canAnyWrite);
                    lua_setfield(L, -2, "readonly");

                    lua_setfield(L, -2, "permissions");

                    return 1;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int exists_impl(lua_State* L, const char* path)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_access(
        req->getLoop(),
        &req->req,
        path,
        F_OK,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;

            if (result < 0 && result != UV_ENOENT)
            {
                r->fail("exists: Error checking existence of %s: %s", req->path, uv_strerror(result));
                return;
            }

            r->succeed(
                [exists = (result == 0)](lua_State* L)
                {
                    lua_pushboolean(L, exists);
                    return 1;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int type_impl(lua_State* L, const char* path)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_stat(
        req->getLoop(),
        &req->req,
        path,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;

            if (result < 0)
            {
                r->fail("Error getting type of file %s: %s", req->path, uv_strerror(result));
                return;
            }

            r->succeed(
                [stat = req->statbuf](lua_State* L)
                {
                    auto type = fileModeToType(stat.st_mode);
                    lua_pushstring(L, type);
                    return 1;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int link_impl(lua_State* L, const char* path, const char* dest)
{
    uvutils::ScopedUVRequest<FSPathPairRequest> req{L, path, dest};
    uv_fs_link(
        req->getLoop(),
        &req->req,
        path,
        dest,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSPathPairRequest>(req);
            auto result = req->result;

            if (result < 0)
            {
                r->fail("link: Error creating link from %s to %s: %s", r->src.c_str(), r->dest.c_str(), uv_strerror(result));
                return;
            }

            r->succeed(
                [](lua_State* L)
                {
                    return 0;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int symlink_impl(lua_State* L, const char* path, const char* dest)
{
    uvutils::ScopedUVRequest<FSPathPairRequest> req{L, path, dest};
    int flags = 0;
#if _WIN32
    flags = Lute::isDirectory(path) ? UV_FS_SYMLINK_DIR : 0;
#endif

    uv_fs_symlink(
        req->getLoop(),
        &req->req,
        path,
        dest,
        flags,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSPathPairRequest>(req);
            auto result = req->result;

            if (result < 0)
            {
                r->fail("symlink: Error creating symlink from %s to %s: %s", r->src.c_str(), r->dest.c_str(), uv_strerror(result));
                return;
            }

            r->succeed(
                [](lua_State* L)
                {
                    return 0;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int copy_impl(lua_State* L, const char* path, const char* dest)
{
    uvutils::ScopedUVRequest<FSPathPairRequest> req{L, path, dest};
    uv_fs_copyfile(
        req->getLoop(),
        &req->req,
        path,
        dest,
        0,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSPathPairRequest>(req);
            auto result = req->result;

            if (result < 0)
            {
                r->fail("copy: Error copying file from %s to %s: %s", r->src.c_str(), r->dest.c_str(), uv_strerror(result));
                return;
            }

            r->succeed(
                [](lua_State* L)
                {
                    return 0;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

int mkdir_impl(lua_State* L, const char* path, int mode)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_mkdir(
        req->getLoop(),
        &req->req,
        path,
        mode,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;
            if (result < 0)
            {
                r->fail("Error creating directory %s: %s", req->path, uv_strerror(result));
                return;
            }

            r->succeed(
                [](lua_State* L)
                {
                    return 0;
                }
            );
        }
    );

    return lua_yield(L, 0);
}


int rmdir_impl(lua_State* L, const char* path)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_rmdir(
        req->getLoop(),
        &req->req,
        path,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;
            if (result < 0)
            {
                r->fail("Error removing directory %s: %s", req->path, uv_strerror(result));
                return;
            }

            r->succeed(
                [](lua_State* L)
                {
                    return 0;
                }
            );
        }
    );

    return lua_yield(L, 0);
}


int listdir_impl(lua_State* L, const char* path)
{
    uvutils::ScopedUVRequest<FSRequest> req(L);
    uv_fs_scandir(
        req->getLoop(),
        &req->req,
        path,
        0,
        [](uv_fs_t* req)
        {
            auto r = uvutils::retake<FSRequest>(req);
            auto result = req->result;
            if (result < 0)
            {
                r->fail("listdir: Error listing directory %s (%s)", req->path, uv_strerror(result));
                return;
            }

            std::vector<std::pair<std::string, uv_dirent_type_t>> entries;
            uv_dirent_t dirent;
            int err = 0;
            while ((err = uv_fs_scandir_next(req, &dirent)) >= 0)
            {
                entries.emplace_back(dirent.name, dirent.type);
            }

            if (err != UV_EOF)
            {
                r->fail("listdir: Error reading directory entry (%s)", uv_strerror(err));
                return;
            }

            r->succeed(
                [entries = std::move(entries)](lua_State* L)
                {
                    lua_createtable(L, entries.size(), 0);
                    for (size_t i = 0; i < entries.size(); ++i)
                    {
                        lua_pushinteger(L, i + 1);

                        lua_createtable(L, 0, 2);

                        lua_pushstring(L, entries[i].first.c_str());
                        lua_setfield(L, -2, "name");

                        lua_pushstring(L, UV_DIRENT_TYPES[entries[i].second]);
                        lua_setfield(L, -2, "type");

                        lua_settable(L, -3);
                    }
                    return 1;
                }
            );
        }
    );

    return lua_yield(L, 0);
}

} // namespace fs
