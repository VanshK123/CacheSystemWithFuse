// fuse.cc
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>     // getuid(), getgid(), realpath
#include <fcntl.h>      // open flags
#include <dirent.h>     // opendir, readdir
#include <errno.h>
#include <string.h>     // memset, strcmp
#include <limits.h>     // PATH_MAX
#include <stdio.h>      // fprintf, perror

#include <string>
#include <mutex>

static std::string backing_path;
static std::mutex backing_lock;

// Map a FUSE path ("/foo/bar") to backing_path + "/foo/bar"
static std::string real_path(const char *path) {
    if (strcmp(path, "/") == 0)
        return backing_path;
    // ensure exactly one slash between backing_path and path
    return backing_path + (backing_path.back()=='/' ? "" : "/") + (path[0]=='/' ? path+1 : path);
}

static int fs_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi)
{
    (void)fi;
    std::string rp = real_path(path);
    int res = lstat(rp.c_str(), stbuf);
    return res == -1 ? -errno : 0;
}

static int fs_statfs(const char *path, struct statvfs *stbuf)
{
    std::string rp = real_path(path);
    int res = statvfs(rp.c_str(), stbuf);
    return res == -1 ? -errno : 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    (void)offset; (void)fi; (void)flags;
    std::string rp = real_path(path);
    DIR *dp = opendir(rp.c_str());
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
        filler(buf, de->d_name, nullptr, 0,
               static_cast<fuse_fill_dir_flags>(0));
    }
    closedir(dp);
    return 0;
}

static int fs_mkdir(const char *path, mode_t mode)
{
    std::string rp = real_path(path);
    int res = mkdir(rp.c_str(), mode);
    return res == -1 ? -errno : 0;
}

static int fs_rmdir(const char *path)
{
    std::string rp = real_path(path);
    int res = rmdir(rp.c_str());
    return res == -1 ? -errno : 0;
}

static int fs_unlink(const char *path)
{
    std::string rp = real_path(path);
    int res = unlink(rp.c_str());
    return res == -1 ? -errno : 0;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    std::string rp = real_path(path);
    int fd = open(rp.c_str(), fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    std::string rp = real_path(path);
    int fd = open(rp.c_str(), fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    ssize_t res = pread((int)fi->fh, buf, size, offset);
    return res < 0 ? -errno : (int)res;
}

static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    ssize_t res = pwrite((int)fi->fh, buf, size, offset);
    return res < 0 ? -errno : (int)res;
}

static int fs_flush(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    int res = fsync((int)fi->fh);
    return res == -1 ? -errno : 0;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    close((int)fi->fh);
    return 0;
}

static int fs_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi)
{
    (void)fi;
    std::string rp = real_path(path);
    int res = truncate(rp.c_str(), size);
    return res == -1 ? -errno : 0;
}

static int fs_utimens(const char *path, const struct timespec ts[2],
                      struct fuse_file_info *fi)
{
    (void)fi;
    std::string rp = real_path(path);
    int res = utimensat(AT_FDCWD, rp.c_str(), ts, AT_SYMLINK_NOFOLLOW);
    return res == -1 ? -errno : 0;
}

static int fs_access(const char *path, int mask)
{
    std::string rp = real_path(path);
    int res = access(rp.c_str(), mask);
    return res == -1 ? -errno : 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <backing-dir> <mountpoint> [FUSE opts]\n", argv[0]);
        return 1;
    }

    // 1) Resolve backing-dir to an absolute path
    char realb[PATH_MAX];
    if (!realpath(argv[1], realb)) {
        perror("realpath backing-dir");
        return 1;
    }
    backing_path = realb;

    // 2) Ensure it exists
    struct stat st;
    if (lstat(backing_path.c_str(), &st) == -1) {
        if (mkdir(backing_path.c_str(), 0755) == -1) {
            perror("mkdir backing-dir");
            return 1;
        }
    }

    // 3) Build FUSE args: skip argv[1]
    struct fuse_args args = FUSE_ARGS_INIT(argc-1, argv+1);

    struct fuse_operations ops;
    memset(&ops, 0, sizeof(ops));
    ops.getattr  = fs_getattr;
    ops.statfs   = fs_statfs;
    ops.readdir  = fs_readdir;
    ops.mkdir    = fs_mkdir;
    ops.rmdir    = fs_rmdir;
    ops.unlink   = fs_unlink;
    ops.create   = fs_create;
    ops.open     = fs_open;
    ops.read     = fs_read;
    ops.write    = fs_write;
    ops.flush    = fs_flush;
    ops.release  = fs_release;
    ops.truncate = fs_truncate;
    ops.utimens  = fs_utimens;
    ops.access   = fs_access;

    return fuse_main(args.argc, args.argv, &ops, NULL);
}
