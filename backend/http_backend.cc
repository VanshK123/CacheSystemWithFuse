// http_backend.cc

#include "backend.h"
#include "cache_manager.h"

#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace cache_fs {

static void mkdir_p(const std::string& dir) {
    std::string so_far;
    for (size_t i = 0; i < dir.size(); ++i) {
        so_far.push_back(dir[i]);
        if (dir[i] == '/') {
            mkdir(so_far.c_str(), 0755);
        }
    }
    // final component
    mkdir(dir.c_str(), 0755);
}

class FileBackend : public Backend {
    std::string base_dir_;

public:
    int init(const std::string& base_url) override {
        const std::string prefix = "file://";
        if (base_url.rfind(prefix, 0) != 0) return -1;
        base_dir_ = base_url.substr(prefix.size());
        return 0;
    }

    ssize_t download(const std::string& path,
                     char*               buffer,
                     size_t              size,
                     off_t               offset) override
    {
        if (cache_has_valid_entry(path.c_str())) {
            ssize_t n = cache_read_file(path.c_str(), buffer, size, offset);
            if (n >= 0) return n;
        }

        std::string full = base_dir_ + path;
        std::ifstream ifs(full, std::ios::binary);
        if (!ifs) return -1;
        std::vector<char> tmp((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
        ifs.close();

        cache_store_file(path.c_str(), tmp.data(), tmp.size(), 0);

        if (offset >= (off_t)tmp.size()) return 0;
        size_t avail = tmp.size() - offset;
        size_t tocopy = avail < size ? avail : size;
        memcpy(buffer, tmp.data() + offset, tocopy);
        return (ssize_t)tocopy;
    }

    ssize_t upload(const std::string& path,
                   const char*        buffer,
                   size_t             size,
                   off_t              offset) override
    {
        if (cache_store_file(path.c_str(), buffer, size, offset) != 0)
            return -1;

        std::string full = base_dir_ + path;
        
        auto pos = full.find_last_of('/');
        if (pos != std::string::npos) {
            mkdir_p(full.substr(0, pos));
        }

        std::fstream ofs(full,
                         std::ios::binary | std::ios::in | std::ios::out);
        if (!ofs) {
            ofs.open(full, std::ios::binary | std::ios::out);
            ofs.close();
            ofs.open(full, std::ios::binary | std::ios::in | std::ios::out);
        }
        if (!ofs) return -1;
        ofs.seekp(offset);
        ofs.write(buffer, size);
        if (!ofs) return -1;
        ofs.close();
        return (ssize_t)size;
    }
};

std::unique_ptr<Backend> create_backend(const std::string& url) {
    auto b = std::make_unique<FileBackend>();
    return (b->init(url) == 0 ? std::move(b) : nullptr);
}

}
