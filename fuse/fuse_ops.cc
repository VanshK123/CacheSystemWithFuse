#define _GNU_SOURCE
#include "fuse_ops.h"
#include <pthread.h>

#include "fuse_config.h"
#include "fuse_i.h"
#include "fuse_lowlevel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include "fuse_kernel.h"
#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <dlfcn.h>
#include <assert.h>
#include <poll.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/file.h>

#define FUSE_NODE_SLAB 1

#ifndef MAP_ANONYMOUS
#undef FUSE_NODE_SLAB
#endif

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1) /* Exchange source and dest */
#endif

#define FUSE_DEFAULT_INTR_SIGNAL SIGUSR1

#define FUSE_UNKNOWN_INO 0xffffffff
#define OFFSET_MAX 0x7fffffffffffffffLL

#define NODE_TABLE_MIN_SIZE 8192

struct fuse_fs
{
    struct fuse_operations op;
    void *user_data;
    int debug;
};

int fuse_fs_getattr(struct fuse_fs *fs, const char *path, struct stat *buf,
                    struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.getattr)
    {
        if (fs->debug)
        {
            char buf[10];
            fuse_log(FUSE_LOG_DEBUG, "getattr[%s] %s\n",
                     file_info_string(fi, buf, sizeof(buf)),
                     path);
        }
        return fs->op.getattr(path, buf, fi);
    }
    else
    {
        return -ENOSYS;
    }
}

int fuse_fs_open(struct fuse_fs *fs, const char *path,
                 struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.open)
    {
        int err;

        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "open flags: 0x%x %s\n", fi->flags,
                     path);

        err = fs->op.open(path, fi);

        if (fs->debug && !err)
            fuse_log(FUSE_LOG_DEBUG, "   open[%llu] flags: 0x%x %s\n",
                     (unsigned long long)fi->fh, fi->flags, path);

        return err;
    }
    else
    {
        return 0;
    }
}

static void fuse_free_buf(struct fuse_bufvec *buf)
{
    if (buf != NULL)
    {
        size_t i;

        for (i = 0; i < buf->count; i++)
            if (!(buf->buf[i].flags & FUSE_BUF_IS_FD))
                free(buf->buf[i].mem);
        free(buf);
    }
}

int fuse_fs_read(struct fuse_fs *fs, const char *path, char *mem, size_t size,
                 off_t off, struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.read || fs->op.read_buf)
    {
        int res;

        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG,
                     "read[%llu] %zu bytes from %llu flags: 0x%x\n",
                     (unsigned long long)fi->fh,
                     size, (unsigned long long)off, fi->flags);

        if (fs->op.read_buf)
        {
            struct fuse_bufvec *buf = NULL;

            res = fs->op.read_buf(path, &buf, size, off, fi);
            if (res == 0)
            {
                struct fuse_bufvec dst = FUSE_BUFVEC_INIT(size);

                dst.buf[0].mem = mem;
                res = fuse_buf_copy(&dst, buf, 0);
            }
            fuse_free_buf(buf);
        }
        else
        {
            res = fs->op.read(path, mem, size, off, fi);
        }

        if (fs->debug && res >= 0)
            fuse_log(FUSE_LOG_DEBUG, "   read[%llu] %u bytes from %llu\n",
                     (unsigned long long)fi->fh,
                     res,
                     (unsigned long long)off);
        if (res >= 0 && res > (int)size)
            fuse_log(FUSE_LOG_ERR, "fuse: read too many bytes\n");

        return res;
    }
    else
    {
        return -ENOSYS;
    }
}

int fuse_fs_release(struct fuse_fs *fs, const char *path,
                    struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.release)
    {
        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "release%s[%llu] flags: 0x%x\n",
                     fi->flush ? "+flush" : "",
                     (unsigned long long)fi->fh, fi->flags);

        return fs->op.release(path, fi);
    }
    else
    {
        return 0;
    }
}

int fuse_fs_mkdir(struct fuse_fs *fs, const char *path, mode_t mode)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.mkdir)
    {
        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "mkdir %s 0%o umask=0%03o\n",
                     path, mode, fuse_get_context()->umask);

        return fs->op.mkdir(path, mode);
    }
    else
    {
        return -ENOSYS;
    }
}

int fuse_fs_rmdir(struct fuse_fs *fs, const char *path)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.rmdir)
    {
        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "rmdir %s\n", path);

        return fs->op.rmdir(path);
    }
    else
    {
        return -ENOSYS;
    }
}

int fuse_fs_link(struct fuse_fs *fs, const char *oldpath, const char *newpath)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.link)
    {
        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "link %s %s\n", oldpath, newpath);

        return fs->op.link(oldpath, newpath);
    }
    else
    {
        return -ENOSYS;
    }
}

int fuse_fs_unlink(struct fuse_fs *fs, const char *path)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.unlink)
    {
        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "unlink %s\n", path);

        return fs->op.unlink(path);
    }
    else
    {
        return -ENOSYS;
    }
}

int fuse_fs_rename(struct fuse_fs *fs, const char *oldpath,
                   const char *newpath, unsigned int flags)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.rename)
    {
        if (fs->debug)
            fuse_log(FUSE_LOG_DEBUG, "rename %s %s 0x%x\n", oldpath, newpath,
                     flags);

        return fs->op.rename(oldpath, newpath, flags);
    }
    else
    {
        return -ENOSYS;
    }
}
