#ifndef CACHE_FS_BACKEND_H
#define CACHE_FS_BACKEND_H

#include <cstddef>
#include <ctime>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

namespace cache_fs {

struct FileInfo {
    std::string name;
    std::size_t size   = 0;
    std::time_t mtime  = 0;
    bool        is_directory = false;
};

class Backend {
public:
    virtual ~Backend() = default;
    virtual int init(const std::string& base_url, const std::string& bearer_token = "") = 0;
    virtual ssize_t download(const std::string& path, char* buffer, std::size_t size, off_t offset) = 0;
    virtual ssize_t upload(const std::string& path, const char* buffer, std::size_t size, off_t offset) = 0;
    virtual int remove(const std::string& path) = 0;
};

std::shared_ptr<Backend> create_backend(const std::string& url);

ssize_t backend_read_range(const std::string& path, char* buf, std::size_t len, off_t off);
ssize_t backend_put_range (const std::string& path, const char* buf, std::size_t len, off_t off);
int     backend_delete    (const std::string& path);

}

#endif
