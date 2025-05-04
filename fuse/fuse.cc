// fuse/fuse.cc
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <memory>
#include <vector>
#include <set>
#include <algorithm>

#include "cache/cache_manager.h"
#include "backend/backend.h"

static std::unique_ptr<cache_fs::Backend> backend;
static std::string                        cache_dir;
static std::string                        file_remote_dir; // for file://
static bool                               http_mode = false;
static const int                          CACHE_TIMEOUT = 60;

// Build local‐cache path: "/foo" → cache_dir + "/foo"
static std::string real_cache_path(const char* path) {
    if (strcmp(path, "/")==0) return cache_dir;
    return cache_dir + (cache_dir.back()=='/'?"":"/") + (path[0]=='/'?path+1:path);
}
// Build file:// remote path: "/foo" → file_remote_dir + "/foo"
static std::string real_file_path(const char* path) {
    if (strcmp(path, "/")==0) return file_remote_dir;
    return file_remote_dir + (file_remote_dir.back()=='/'?"":"/") + (path[0]=='/'?path+1:path);
}

// --- helper: parse HTML index listing out of a GET on a directory ---
static void parse_html_links(const std::string &html, std::set<std::string> &out) {
    size_t pos = 0;
    while ((pos = html.find("<a href=\"", pos))!=std::string::npos) {
        pos += 9;
        size_t end = html.find('"', pos);
        if (end==std::string::npos) break;
        std::string href = html.substr(pos, end-pos);
        // skip parent/back entries
        if (href=="../" || href=="./") { pos = end; continue; }
        // strip trailing slash
        if (!href.empty() && href.back()=='/') href.pop_back();
        out.insert(href);
        pos = end;
    }
}

// --- FUSE callbacks ---
static int fs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info*) {
    memset(stbuf,0,sizeof(*stbuf));
    if (strcmp(path,"/")==0) {
        stbuf->st_mode = S_IFDIR|0755;
        stbuf->st_nlink=2;
        return 0;
    }
    // file:// mode → stat underlying FS
    if (!file_remote_dir.empty()) {
        std::string rp = real_file_path(path);
        int r = stat(rp.c_str(), stbuf);
        return r==-1 ? -errno : 0;
    }
    // HTTP or other → fetch via cache layer to get size
    if (!cache_has_valid_entry(path)) {
        char tmp;
        if (backend->download(path,&tmp,0,0)<0)
            return -ENOENT;
    }
    cache_entry* e = cache_get_entry(path);
    if (!e) return -ENOENT;
    stbuf->st_mode = S_IFREG|0644;
    stbuf->st_nlink=1;
    stbuf->st_size = e->size;
    return 0;
}

static int fs_statfs(const char*, struct statvfs* stbuf) {
    return statvfs(cache_dir.c_str(), stbuf)==-1 ? -errno : 0;
}

static int fs_readdir(const char* path, void* buf,
                      fuse_fill_dir_t filler,
                      off_t, struct fuse_file_info*,
                      enum fuse_readdir_flags)
{
    if (strcmp(path,"/")!=0) return -ENOENT;
    filler(buf,".",nullptr,0,FUSE_FILL_DIR_PLUS);
    filler(buf,"..",nullptr,0,FUSE_FILL_DIR_PLUS);

    std::set<std::string> names;

    // 1) file:// listing
    if (!file_remote_dir.empty()) {
        DIR* d = opendir(file_remote_dir.c_str());
        if (d) {
            struct dirent* de;
            while ((de = readdir(d))) {
                std::string n = de->d_name;
                if (n!="." && n!="..") names.insert(n);
            }
            closedir(d);
        }
    }
    // 2) HTTP listing by GET+HTML parse
    else if (http_mode) {
        // GET "/" → HTML index
        std::string html;
        const size_t MAX = 64*1024;
        std::vector<char> buf(MAX);
        ssize_t got = backend->download("/", buf.data(), buf.size(), 0);
        if (got>0) {
            html.assign(buf.data(), buf.data()+got);
            parse_html_links(html, names);
        }
    }

    // 3) any cached or newly written files
    DIR* d2 = opendir(cache_dir.c_str());
    if (d2) {
        struct dirent* de;
        while ((de = readdir(d2))) {
            std::string n = de->d_name;
            if (n!="." && n!="..") names.insert(n);
        }
        closedir(d2);
    }

    for (auto &n : names)
        filler(buf, n.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    return 0;
}

static int fs_mkdir(const char* p, mode_t m) {
    return mkdir(real_cache_path(p).c_str(),m)==-1? -errno:0;
}
static int fs_rmdir(const char* p) {
    return rmdir(real_cache_path(p).c_str())==-1? -errno:0;
}
static int fs_unlink(const char* p) {
    return unlink(real_cache_path(p).c_str())==-1? -errno:0;
}

static int fs_open(const char*, struct fuse_file_info*)   { return 0; }
static int fs_create(const char*, mode_t, struct fuse_file_info*) { return 0; }

static int fs_read(const char* path, char* buf, size_t sz,
                   off_t off, struct fuse_file_info*)
{
    // file:// direct read
    if (!file_remote_dir.empty()) {
        std::string rp = real_file_path(path);
        int fd = open(rp.c_str(),O_RDONLY);
        if (fd<0) return -errno;
        ssize_t n = pread(fd,buf,sz,off);
        close(fd);
        if (n<0) return -errno;
        // cache it too
        cache_store_file(path,buf,n,off);
        return (int)n;
    }
    // HTTP or generic
    ssize_t n = backend->download(path,buf,sz,off);
    return n<0? -EIO: (int)n;
}

static int fs_write(const char* path, const char* buf, size_t sz,
                    off_t off, struct fuse_file_info*)
{
    // file:// direct write
    if (!file_remote_dir.empty()) {
        std::string rp = real_file_path(path);
        int fd = open(rp.c_str(),O_CREAT|O_WRONLY,0644);
        if (fd<0) return -errno;
        ssize_t n = pwrite(fd,buf,sz,off);
        close(fd);
        return n<0? -errno: (int)n;
    }
    // HTTP
    ssize_t n = backend->upload(path,buf,sz,off);
    return n<0? -EIO: (int)n;
}

static int fs_flush(const char*, struct fuse_file_info*)  { return 0; }
static int fs_release(const char*, struct fuse_file_info*) {
    cache_apply_eviction();
    return 0;
}

static int fs_truncate(const char* path, off_t sz, struct fuse_file_info*) {
    std::vector<char> z(sz,0);
    cache_store_file(path,z.data(),z.size(),0);
    if (!file_remote_dir.empty()) {
        std::string rp = real_file_path(path);
        int fd = open(rp.c_str(),O_CREAT|O_WRONLY,0644);
        if (fd>=0) { ftruncate(fd,sz); close(fd); }
    }
    return 0;
}

static int fs_utimens(const char*, const struct timespec[2], struct fuse_file_info*) { return 0; }
static int fs_access(const char*, int) { return 0; }

int main(int argc, char* argv[])
{
    if (argc<4) {
        fprintf(stderr,"Usage: %s <cache_dir> <backend_url> <mountpoint> [FUSE opts]\n",argv[0]);
        return 1;
    }

    // — resolve cache_dir
    char rb[PATH_MAX];
    if (!realpath(argv[1],rb)) {
        if (mkdir(argv[1],0755)==-1){ perror("mkdir"); return 1; }
        if (!realpath(argv[1],rb)){ perror("realpath"); return 1; }
    }
    cache_dir = rb;
    if (cache_init(cache_dir.c_str(),CACHE_TIMEOUT)!=0){
        fprintf(stderr,"cache_init failed\n"); return 1;
    }

    // — parse URL scheme
    std::string url(argv[2]);
    if (url.rfind("file://",0)==0) {
        file_remote_dir = url.substr(7);
    } else if (url.rfind("http://",0)==0 || url.rfind("https://",0)==0) {
        http_mode = true;
    }

    // — init HTTP/backend (even file:// just to satisfy interface)
    backend = cache_fs::create_backend(url);
    if (!backend){ fprintf(stderr,"backend init failed\n"); return 1; }

    // — fill operations
    struct fuse_operations ops;
    memset(&ops,0,sizeof(ops));
    ops.getattr  = fs_getattr;
    ops.statfs   = fs_statfs;
    ops.readdir  = fs_readdir;
    ops.mkdir    = fs_mkdir;
    ops.rmdir    = fs_rmdir;
    ops.unlink   = fs_unlink;
    ops.open     = fs_open;
    ops.create   = fs_create;
    ops.read     = fs_read;
    ops.write    = fs_write;
    ops.flush    = fs_flush;
    ops.release  = fs_release;
    ops.truncate = fs_truncate;
    ops.utimens  = fs_utimens;
    ops.access   = fs_access;

    // — pass mountpoint+opts (skip prog & cache_dir)
    struct fuse_args args = FUSE_ARGS_INIT(argc-2, argv+2);
    int ret = fuse_main(args.argc, args.argv, &ops, nullptr);

    cache_cleanup();
    return ret;
}
