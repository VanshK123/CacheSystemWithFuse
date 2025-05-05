#include "backend/backend.h"
#include <curl/curl.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cache_fs {

namespace {

struct SliceWriter {
char*  dst;
size_t cap;
size_t pos;
};
size_t write_slice_cb(void* ptr, size_t sz, size_t nm, void* ud) {
size_t n   = sz * nm;
auto*  sw  = static_cast<SliceWriter*>(ud);
size_t cpy = std::min(n, sw->cap - sw->pos);
if (cpy) {
    memcpy(sw->dst + sw->pos, ptr, cpy);
    sw->pos += cpy;
}
return n;
}

struct UploadBuf {
const char* data;
size_t      total;
size_t      pos;
};

#ifdef ENABLE_PUT
size_t read_upload_cb(char* ptr, size_t sz, size_t nm, void* ud) {
auto* up = static_cast<UploadBuf*>(ud);
size_t avail = up->total - up->pos;
size_t cpy   = std::min(avail, sz * nm);
if (cpy) {
    memcpy(ptr, up->data + up->pos, cpy);
    up->pos += cpy;
}
return cpy;
}
#endif

}

class HttpBackend : public Backend {
public:
int init(const std::string& url, const std::string& bearer_token = "") override {
    base_url_     = url;
    bearer_token_ = bearer_token;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

ssize_t download(const std::string& path, char* buffer, std::size_t size, off_t offset) override {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    std::string url = base_url_ + path;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    std::string range = "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(offset + size - 1);
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, range.c_str());
    if (!bearer_token_.empty()) {
        std::string auth = "Authorization: Bearer " + bearer_token_;
        hdrs = curl_slist_append(hdrs, auth.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    SliceWriter sw{buffer, size, 0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_slice_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sw);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? static_cast<ssize_t>(sw.pos) : -1;
}

ssize_t upload(const std::string& path, const char* buffer, std::size_t size, off_t offset) override {
#ifndef ENABLE_PUT
    (void)path; 
    (void)buffer; 
    (void)size; 
    (void)offset;
    return -ENOSYS;
#else
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    std::string url = base_url_ + path;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    std::string rng = "Content-Range: bytes " + std::to_string(offset) + "-" + std::to_string(offset + size - 1) + "/*";
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, rng.c_str());
    if (!bearer_token_.empty()) {
        std::string auth = "Authorization: Bearer " + bearer_token_;
        hdrs = curl_slist_append(hdrs, auth.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    UploadBuf up{buffer, size, 0};
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_upload_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA, &up);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? static_cast<ssize_t>(size) : -1;
#endif
}

int remove(const std::string& path) override {
#ifndef ENABLE_PUT
    (void)path;
    return -ENOSYS;
#else
    CURL* curl = curl_easy_init();
    if (!curl) return -1;
    std::string url = base_url_ + path;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (!bearer_token_.empty()) {
        std::string auth = "Authorization: Bearer " + bearer_token_;
        struct curl_slist* hdrs = nullptr;
        hdrs = curl_slist_append(hdrs, auth.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    }
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : -1;
#endif
}

private:
std::string base_url_;
std::string bearer_token_;
};

static std::mutex                        g_mtx;
static std::unique_ptr<Backend>          g_backend;
static Backend*                          g_backend_raw = nullptr;

std::unique_ptr<Backend> create_backend(const std::string& url) {
    auto b = std::make_unique<HttpBackend>();
    if (b->init(url) != 0) return nullptr;

    std::lock_guard lk(g_mtx);
    g_backend      = std::move(b);
    g_backend_raw  = g_backend.get();
    return std::unique_ptr<Backend>(g_backend_raw);
}

ssize_t backend_read_range(const std::string& path, char* buf, std::size_t len, off_t off) {
    std::lock_guard lk(g_mtx);
    if (!g_backend_raw) return -ENODEV;
    return g_backend_raw->download(path, buf, len, off);
}

ssize_t backend_put_range(const std::string& path, const char* buf, std::size_t len, off_t off) {
    std::lock_guard lk(g_mtx);
    if (!g_backend_raw) return -ENODEV;
    return g_backend_raw->upload(path, buf, len, off);
}

int backend_delete(const std::string& path) {
    std::lock_guard lk(g_mtx);
    if (!g_backend_raw) return -ENODEV;
    return g_backend_raw->remove(path);
}

}
