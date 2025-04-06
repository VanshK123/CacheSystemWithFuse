#include "backend.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <json/json.h>
#include <regex>

namespace cache_fs {

class HttpBackend : public Backend {
private:
    std::string base_url;
    CURL* curl;
    
    struct MemoryStruct {
        char* memory;
        size_t size;
        
        MemoryStruct() : memory(nullptr), size(0) {}
        
        ~MemoryStruct() {
            if (memory) {
                free(memory);
            }
        }
    };
    
    struct UploadStruct {
        const char* data;
        size_t size;
        size_t offset;
        
        UploadStruct(const char* d, size_t s, size_t o) 
            : data(d), size(s), offset(o) {}
    };
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t realsize = size * nmemb;
        auto* mem = static_cast<MemoryStruct*>(userp);
        
        char* ptr = static_cast<char*>(realloc(mem->memory, mem->size + realsize + 1));
        if (!ptr) {
            return 0;
        }
        
        mem->memory = ptr;
        std::memcpy(&(mem->memory[mem->size]), contents, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
        
        return realsize;
    }
    
    static size_t read_callback(void* ptr, size_t size, size_t nmemb, void* userp) {
        auto* upload = static_cast<UploadStruct*>(userp);
        size_t max_size = size * nmemb;
        
        if (upload->offset >= upload->size) {
            return 0;
        }
        
        size_t bytes_left = upload->size - upload->offset;
        size_t bytes_to_copy = (bytes_left < max_size) ? bytes_left : max_size;
        
        std::memcpy(ptr, upload->data + upload->offset, bytes_to_copy);
        upload->offset += bytes_to_copy;
        
        return bytes_to_copy;
    }
    
    CURLcode perform_request(const std::string& url, MemoryStruct& chunk, 
                            const std::string& custom_request = "", 
                            UploadStruct* upload = nullptr) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        if (!custom_request.empty()) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, custom_request.c_str());
        } else {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        }
        
        if (upload) {
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
            curl_easy_setopt(curl, CURLOPT_READDATA, upload);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(upload->size));
        } else {
            curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
        }
        
        return curl_easy_perform(curl);
    }
    
    bool parse_file_info(const std::string& json_data, FileInfo& info) {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        
        bool parsing_successful = reader->parse(
            json_data.c_str(), json_data.c_str() + json_data.size(), &root, &errors);
        
        if (!parsing_successful) {
            return false;
        }
        
        if (root.isMember("name") && root.isMember("size") && 
            root.isMember("mtime") && root.isMember("is_directory")) {
            info.name = root["name"].asString();
            info.size = root["size"].asUInt64();
            info.mtime = root["mtime"].asInt64();
            info.is_directory = root["is_directory"].asBool();
            return true;
        }
        
        return false;
    }
    
    bool parse_file_listing(const std::string& json_data, std::vector<FileInfo>& files) {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        
        bool parsing_successful = reader->parse(
            json_data.c_str(), json_data.c_str() + json_data.size(), &root, &errors);
        
        if (!parsing_successful || !root.isArray()) {
            return false;
        }
        
        files.clear();
        
        for (const auto& item : root) {
            if (item.isMember("name") && item.isMember("size") && 
                item.isMember("mtime") && item.isMember("is_directory")) {
                FileInfo info;
                info.name = item["name"].asString();
                info.size = item["size"].asUInt64();
                info.mtime = item["mtime"].asInt64();
                info.is_directory = item["is_directory"].asBool();
                files.push_back(info);
            }
        }
        
        return true;
    }
    
    std::string normalize_path(const std::string& path) {
        if (path.empty() || path[0] != '/') {
            return "/" + path;
        }
        return path;
    }
    
public:
    HttpBackend() : curl(nullptr) {}
    
    ~HttpBackend() override {
        if (curl) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }
    
    int init(const std::string& base_url) override {
        this->base_url = base_url;
        
        // Initialize libcurl
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        
        if (!curl) {
            return -EINVAL;
        }
        
        return 0;
    }
    
    int get_info(const std::string& path, FileInfo& info) override {
        std::string url = base_url + "/api/info" + normalize_path(path);
        MemoryStruct chunk;
        
        CURLcode res = perform_request(url, chunk);
        
        if (res != CURLE_OK) {
            return -ENOENT;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 200) {
            return -ENOENT;
        }
        
        if (!parse_file_info(chunk.memory, info)) {
            return -EINVAL;
        }
        
        return 0;
    }
    
    int list_dir(const std::string& path, std::vector<FileInfo>& files) override {
        std::string url = base_url + "/api/list" + normalize_path(path);
        MemoryStruct chunk;
        
        CURLcode res = perform_request(url, chunk);
        
        if (res != CURLE_OK) {
            return -ENOENT;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 200) {
            return -ENOENT;
        }
        
        if (!parse_file_listing(chunk.memory, files)) {
            return -EINVAL;
        }
        
        return 0;
    }
    
    ssize_t download(const std::string& path, char* buffer, size_t size, off_t offset) override {
        std::string url = base_url + "/api/data" + normalize_path(path);
        MemoryStruct chunk;
        
        std::ostringstream range_header;
        range_header << "Range: bytes=" << offset << "-" << (offset + size - 1);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, range_header.str().c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = perform_request(url, chunk);
        
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            return -EIO;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 206 && http_code != 200) {
            return -ENOENT;
        }
        
        size_t bytes_to_copy = std::min(chunk.size, size);
        std::memcpy(buffer, chunk.memory, bytes_to_copy);
        
        return bytes_to_copy;
    }
    
    ssize_t upload(const std::string& path, const char* buffer, size_t size, off_t offset) override {
        std::string url = base_url + "/api/data" + normalize_path(path);
        MemoryStruct chunk;
        
        UploadStruct upload_data(buffer, size, 0);
        
        if (offset > 0) {
            std::ostringstream content_range;
            content_range << "Content-Range: bytes " << offset << "-" 
                        << (offset + size - 1) << "/*";
            
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, content_range.str().c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            CURLcode res = perform_request(url, chunk, "PATCH", &upload_data);
            curl_slist_free_all(headers);
            
            if (res != CURLE_OK) {
                return -EIO;
            }
        } else {
            CURLcode res = perform_request(url, chunk, "PUT", &upload_data);
            
            if (res != CURLE_OK) {
                return -EIO;
            }
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 200 && http_code != 201 && http_code != 204) {
            return -EIO;
        }
        
        return size;
    }
    
    int create(const std::string& path, bool is_directory) override {
        std::string url = base_url + "/api/create" + normalize_path(path);
        MemoryStruct chunk;
        
        if (is_directory) {
            url += "?directory=true";
        }
        
        CURLcode res = perform_request(url, chunk, "POST");
        
        if (res != CURLE_OK) {
            return -EIO;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 201 && http_code != 204) {
            return -EIO;
        }
        
        return 0;
    }
    
    int remove(const std::string& path, bool is_directory) override {
        std::string url = base_url + "/api/delete" + normalize_path(path);
        MemoryStruct chunk;
        
        if (is_directory) {
            url += "?directory=true";
        }
        
        CURLcode res = perform_request(url, chunk, "DELETE");
        
        if (res != CURLE_OK) {
            return -EIO;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 200 && http_code != 204) {
            return -EIO;
        }
        
        return 0;
    }
    
    int rename(const std::string& old_path, const std::string& new_path) override {
        std::string url = base_url + "/api/rename";
        MemoryStruct chunk;
        
        std::string payload = "{\"old_path\":\"" + normalize_path(old_path) + 
                        "\",\"new_path\":\"" + normalize_path(new_path) + "\"}";
        
        UploadStruct upload_data(payload.c_str(), payload.size(), 0);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = perform_request(url, chunk, "POST", &upload_data);
        
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            return -EIO;
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code != 200 && http_code != 204) {
            return -EIO;
        }
        
        return 0;
    }
};

std::unique_ptr<Backend> create_backend(const std::string& url) {
    
    std::regex scheme_regex("^([a-zA-Z][a-zA-Z0-9+.-]*)://");
    std::smatch match;
    std::string scheme;
    
    if (std::regex_search(url, match, scheme_regex) && match.size() > 1) {
        scheme = match[1].str();
    }
    
    if (scheme == "http" || scheme == "https" || scheme.empty()) {
        auto backend = std::make_unique<HttpBackend>();
        backend->init(url);
        return backend;
    }
    
    return nullptr;
}

}