#ifndef BACKEND_H
#define BACKEND_H

#include <string>
#include <vector>
#include <ctime>
#include <memory>

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

    virtual int get_info(const std::string& path, FileInfo& info) = 0;

    virtual int list_dir(const std::string& path, std::vector<FileInfo>& files) = 0;

    virtual ssize_t download(const std::string& path, char* buffer, size_t size, off_t offset) = 0;

    virtual ssize_t upload(const std::string& path, const char* buffer, size_t size, off_t offset) = 0;

    virtual int create(const std::string& path, bool is_directory) = 0;

    virtual int remove(const std::string& path, bool is_directory) = 0;

    virtual int rename(const std::string& old_path, const std::string& new_path) = 0;
};

std::unique_ptr<Backend> create_backend(const std::string& url);

}

#endif