// backend/http_backend.cc

#include "backend/backend.h"
#include "cache/cache_manager.h"
#include <curl/curl.h>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

namespace cache_fs {

// Liquid‐buffer for libcurl GET
struct CurlBuffer {
    char*  data;
    size_t size;
    CurlBuffer(): data((char*)malloc(1)), size(0) {}
    ~CurlBuffer() { free(data); }
};
static size_t write_cb(void* ptr, size_t sz, size_t nm, void* ud) {
    size_t realsz = sz * nm;
    CurlBuffer* buf = (CurlBuffer*)ud;
    char* p = (char*)realloc(buf->data, buf->size + realsz + 1);
    if (!p) return 0;
    buf->data = p;
    memcpy(buf->data + buf->size, ptr, realsz);
    buf->size += realsz;
    buf->data[buf->size] = '\0';
    return realsz;
}

// Upload‐buffer for libcurl PUT
struct UploadBuffer {
    const char* data;
    size_t      pos;
    size_t      total;
    UploadBuffer(const char* d, size_t t): data(d), pos(0), total(t) {}
};
static size_t read_cb(void* ptr, size_t sz, size_t nm, void* ud) {
    UploadBuffer* up = (UploadBuffer*)ud;
    size_t avail = up->total - up->pos;
    size_t tocopy = std::min(avail, sz * nm);
    if (tocopy) {
        memcpy(ptr, up->data + up->pos, tocopy);
        up->pos += tocopy;
    }
    return tocopy;
}

class HttpBackend : public Backend {
    std::string base_url_;
public:
    int init(const std::string& url) override {
        base_url_ = url;
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return 0;
    }

    ssize_t download(const std::string& path,
                     char*               buffer,
                     size_t              size,
                     off_t               offset) override
    {
        // 1) Cache hit?
        if (cache_has_valid_entry(path.c_str())) {
            ssize_t n = cache_read_file(path.c_str(), buffer, size, offset);
            if (n >= 0) return n;
        }
        // 2) HTTP GET
        CURL* curl = curl_easy_init();
        if (!curl) return -1;
        std::string url = base_url_ + path;
        CurlBuffer chunk;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        if (curl_easy_perform(curl) != CURLE_OK) {
            curl_easy_cleanup(curl);
            return -1;
        }
        curl_easy_cleanup(curl);

        // 3) Populate cache
        cache_store_file(path.c_str(), chunk.data, chunk.size, 0);

        // 4) Copy out requested slice
        if (offset >= (off_t)chunk.size) return 0;
        size_t avail = chunk.size - offset;
        size_t tocopy = std::min(avail, size);
        memcpy(buffer, chunk.data + offset, tocopy);
        return (ssize_t)tocopy;
    }

    ssize_t upload(const std::string& path,
                   const char*        buffer,
                   size_t             size,
                   off_t              offset) override
    {
        // 1) Cache write
        if (cache_store_file(path.c_str(), buffer, size, offset) != 0)
            return -1;

        // 2) Read full object
        cache_entry* e = cache_get_entry(path.c_str());
        if (!e) return -1;
        std::vector<char> full(e->size);
        if (cache_read_file(path.c_str(), full.data(), e->size, 0) < 0)
            return -1;

        // 3) HTTP PUT
        CURL* curl = curl_easy_init();
        if (!curl) return -1;
        std::string url = base_url_ + path;
        UploadBuffer up(full.data(), full.size());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_cb);
        curl_easy_setopt(curl, CURLOPT_READDATA, &up);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)full.size());
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return (res == CURLE_OK) ? (ssize_t)size : -1;
    }
};

std::unique_ptr<Backend> create_backend(const std::string& url) {
    auto b = std::make_unique<HttpBackend>();
    return (b->init(url) == 0) ? std::move(b) : nullptr;
}

} // namespace cache_fs
