// backend.h
#ifndef BACKEND_H
#define BACKEND_H

#include <string>
#include <vector>
#include <ctime>
#include <memory>
#include <sys/types.h>   // for ssize_t

namespace cache_fs {

struct FileInfo {
    std::string name;
    size_t size;
    time_t mtime;
    bool is_directory;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual int init(const std::string& base_url) = 0;

    virtual ssize_t download(const std::string& path,
                             char*        buffer,
                             size_t       size,
                             off_t        offset) = 0;

    virtual ssize_t upload(const std::string& path,
                           const char*        buffer,
                           size_t             size,
                           off_t              offset) = 0;
};

std::unique_ptr<Backend> create_backend(const std::string& url);

} // namespace cache_fs

#endif // BACKEND_H
