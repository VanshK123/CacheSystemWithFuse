#define FUSE_USE_VERSION 31

// for fuse, of course
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
// for opening and closing directories
#include <dirent.h>
// for returning errors to the operating system
#include <errno.h>
// for string operations
#include <string.h>
#include <stdio.h>

#include <string>
#include <mutex>

// helpful github: https://github.com/COSI-Lab/CS444-FUSERFS/blob/master/fs.c
// another helpful github: https://libfuse.github.io/doxygen/fuse-3_88_80_2example_2passthrough_8c.html
// helpful for open and create: https://libfuse.github.io/doxygen/fuse-3_88_80_2example_2passthrough_8c.html
// useful tutorial: https://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/

static std::string backing_path;
static std::mutex backing_lock;

// Map a FUSE path ("/foo/bar") to backing_path + "/foo/bar"
static std::string real_path(const char *path) {
    // check if there is a slash
    if (strcmp(path, "/") == 0) {
        // yes there is a slash
        return backing_path;
    }
    // makes sure there is one slash between backing_path and path
    return backing_path + (backing_path.back()=='/' ? "" : "/") + (path[0]=='/' ? path+1 : path);
}

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    std::string realPath = real_path(path);
    // get information about a file
    if (lstat(realPath.c_str(), stbuf) == -1) {
        // returns error
        return -errno;
    } else {
        return 0;
    }
}

static int fs_statfs(const char *path, struct statvfs *stbuf) {
    std::string realPath = real_path(path);
    if (statvfs(realPath.c_str(), stbuf) == -1) {
        // returns error
        return -errno;
    } else {
        return 0;
    }
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
    // closes out file
    close((int)fi->fh);
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    std::string realPath = real_path(path);
    // declare the directory
    DIR *directoryStream = opendir(realPath.c_str());
    // if directory is not found
    if (!directoryStream) {
        // returns error
        return -errno;
    }
    // dirent represents a directory entry
    struct dirent *directoryEntry;
    while ((directoryEntry = readdir(directoryStream)) != nullptr) {
        filler(buf, directoryEntry->d_name, nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
    }
    // close out directory stream
    closedir(directoryStream);
    return 0;
}

static int fs_mkdir(const char *path, mode_t mode) {
    std::string realPath = real_path(path);
    // this creates a new directory at the real path
    if (mkdir(realPath.c_str(), mode) == -1) {
        // returns error
        return -errno;
    } else {
        return 0;
    }
}

static int fs_rmdir(const char *path) {
    std::string realPath = real_path(path);
    // remove directory from real path
    if (rmdir(realPath.c_str()) == -1) {
        // returns error
        return -errno;
    } else {
        return 0;
    }
}

static int fs_unlink(const char *path) {
    std::string realPath = real_path(path);
    // remove a file
    if (unlink(realPath.c_str()) == -1) {
        // returns error
        return -errno;
    } else {
        return 0;
    }
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string realPath = real_path(path);
    // creates a file
    int fileDirectory = open(realPath.c_str(), fi->flags | O_CREAT, mode);
    // if unable to open the file
    if (fileDirectory == -1) {
        // return error
        return -errno;
    }
    // add this file to the file info struct
    fi->fh = fileDirectory;
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    std::string realPath = real_path(path);
    // opens a file
    int fileDirectory = open(realPath.c_str(), fi->flags);
    if (fileDirectory == -1) {
        // return error
        return -errno;
    }
    // add this file to the file info struct
    fi->fh = fileDirectory;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // read data from file
    ssize_t res = pread((int)fi->fh, buf, size, offset);
    if (res < 0) {
        return -errno;
    } else {
        return (int)res;
    }
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // write data into file
    ssize_t res = pwrite((int)fi->fh, buf, size, offset);
    if (res < 0) {
        return -errno;
    } else {
        return (int)res;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        // not enough arguments
        fprintf(stderr, "Usage: %s <backing-dir> <mountpoint> [FUSE opts]\n", argv[0]);
        return 1;
    }

    // The following error checks adjust the path for ls, mkdir, etc. to function without error

    // Resolve backing-dir to an absolute path
    char realb[PATH_MAX];
    if (!realpath(argv[1], realb)) {
        perror("realpath backing-dir");
        return 1;
    }
    backing_path = realb;

    // Ensure it exists
    struct stat st;
    if (lstat(backing_path.c_str(), &st) == -1) {
        if (mkdir(backing_path.c_str(), 0755) == -1) {
            perror("mkdir backing-dir");
            return 1;
        }
    }

    // Build FUSE args: skip argv[1]
    struct fuse_args args = FUSE_ARGS_INIT(argc-1, argv+1);

    static struct fuse_operations operations = {
        .getattr  = fs_getattr,
        .mkdir    = fs_mkdir,
        .unlink   = fs_unlink,
        .rmdir    = fs_rmdir,
        .open     = fs_open,
        .read     = fs_read,
        .write    = fs_write,
        .statfs   = fs_statfs,
        .release  = fs_release,
        .readdir  = fs_readdir,
        .create   = fs_create,
    };

    return fuse_main(args.argc, args.argv, &operations, NULL);
}