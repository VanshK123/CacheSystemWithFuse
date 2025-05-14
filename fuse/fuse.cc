// fuse version we used
#define FUSE_USE_VERSION 31

// include the main fuse header file
#include <fuse3/fuse.h>
// fuse API stuff, useful for the fuse operation functions
#include <sys/statvfs.h>
// used for read, write, and close
#include <unistd.h>
// for opening and closing directories (opendir, etc.)
#include <dirent.h>
// needed for strcmp, memset, strcpy, etc.
#include <string.h>
// printf, fprintf, and perror
#include <stdio.h>
// stat struct
#include <sys/stat.h>
// file options (O_CREAT, etc.)
#include <fcntl.h>
// useful for cout debugging
#include <iostream>
// for string operations
#include <string>
// for special pointers (html work)
#include <memory>
// includes vectors
#include <vector>
// includes sets
#include <set>
// includes find and sort
#include <algorithm>
// isdigit and isspace
#include <cctype>
// stoi which converts a string to an integer
#include <cstdlib>

#include "cache/cache_manager.h"
#include "backend/backend.h"

using namespace std;

// global variables
static shared_ptr<cache_fs::Backend> dataBackend;
static shared_ptr<cache_fs::Backend> apiBackend;
static string cacheDirectory;
static string fileRemoteDirectory;
static bool httpMode = false;

// if there is a slash, keep it for the cache
static string realCachePath(const char* path) {

    // if the input is just a slash, then return the path normally
    if (path == "/") {
        return cacheDirectory;
    } else {
        // add a slash to the last character of the cacheDirectory if it doesn't end with a slash (causes errors otherwise)
        if (cacheDirectory.back() != '/') {
            cacheDirectory.back() = '/';
        }
        // avoids duplicate slashes
        if (path[0] == '/') {
            // skip the slash by moving the pointer by one character
            path + 1;
        }
        return cacheDirectory + path;
    }

}

static string realFilePath(const char* path) {

    // if the input is just a slash, then return the path normally
    if (path == "/") {
        return fileRemoteDirectory;
    } else {
        // add a slash to the last character of the fileRemoteDirectory if it doesn't end with a slash (causes errors otherwise)
        if (fileRemoteDirectory.back() != '/') {
            fileRemoteDirectory.back() = '/';
        }
        // avoids duplicate slashes
        if (path[0] == '/') {
            // skip the slash by moving the pointer by one character
            path + 1;
        }
        return fileRemoteDirectory + path;
    }

}

static void findJsonNames(const string &json, set<string> &out) {

    // initialize position
    size_t pos = 0;
    while ((pos = json.find("\"name\"", pos)) != -1) {
        pos = json.find(':', pos);
        if (pos == -1) {
            // error
            return;
        }
        // increment to next character
        pos += 1;
        while (pos < json.size() && isspace(json[pos])) {
            // continue to increment until character is found
            pos += 1;
        }
        if (pos >= json.size() || json[pos] != '"') {
            // move on to next iteration
            continue;
        }
        // increment to next character
        pos += 1;
        // note start position
        size_t start = pos;
        // end position is at "
        size_t end = json.find('"', pos);
        if (end == -1) {
            // error
            return;
        }
        out.insert(json.substr(start, end - start));
        pos = end + 1;
    }

}

static bool get_file_info(const char* path, bool &isDirectory, off_t &size) {

    vector<char> buf(1024);
    string filePath = string("/info") + path;
    ssize_t bytes = apiBackend->download(filePath, buf.data(), buf.size(), 0);
    if (bytes < 0) {
        return false;
    }
    string json(buf.data(), buf.data() + bytes);

    // parse size
    size = 0;
    if (auto pos = json.find("\"size\""); pos != -1) {
        pos = json.find(':', pos);
        if (pos != -1) {
            // increment to next position to check next character
            pos += 1;
            // maintain character is a digit
            while (pos < json.size() && !isdigit(json[pos])) {
                // continue to increment position
                pos += 1;
            }
            // note starting position before searching for end
            size_t start = pos;
            // maintain character is a digit
            while (pos < json.size() && isdigit(json[pos])) {
                // continue to increment position
                pos += 1;
            }
            // stoi converts string into int
            size = stoi(json.substr(start, pos - start));
        }
    }

    // check if there is a directory
    isDirectory = false;
    if (auto pos = json.find("\"is_directory\""); pos != -1) {
        // note position where : follows the directory
        pos = json.find(':', pos);
        if (pos != -1 && json.find("true", pos) != -1)
            isDirectory = true;
    }

    return true;

}

static int getAttribute(const char* path, struct stat* stbuf, struct fuse_file_info*) {

    // zero out the buffer
    memset(stbuf, 0, sizeof(*stbuf));
    // check if path is to the root directory
    if (path == "/") {
        // gives root directory file permissions
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    // check if this is a local file directory
    if (!fileRemoteDirectory.empty()) {
        // determine the real path
        string rp = realFilePath(path);
        // .c_str converts a string to const char *
        if (stat(rp.c_str(), stbuf) == -1) {
            return -1;
        } else {
            return 0;
        }
    }
    // if looking at http directory
    if (httpMode) {
        bool isDirectory;
        off_t fsize;
        if (!get_file_info(path, isDirectory, fsize))
            // return error if file info is not found
            return -1;
        if (isDirectory) {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
        } else {
            stbuf->st_mode = S_IFREG | 0644;
            stbuf->st_nlink = 1;
            stbuf->st_size = fsize;
        }
        return 0;
    }
    // check if the file has been cached
    if (!cache_has_valid_entry(path)) {
        char tmp;
        if (dataBackend->download(path, &tmp, 0, 0) < 0) {
            return -1;
        }
    }
    // get the cache entry pointer
    cache_entry* cacheEntry = cache_get_entry(path);
    if (!cacheEntry) {
        // return error if there is an error with the cache entry
        return -1;
    }
    // gives regular file permissions
    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = cacheEntry->size;
    return 0;

}

static int getStats(const char*, struct statvfs* stbuf) {

    if (statvfs(cacheDirectory.c_str(), stbuf) == -1) {
        // returns error
        return -1;
    } else {
        return 0;
    }

}

static int readDirectory(const char* path, void* buf, fuse_fill_dir_t filler, off_t, struct fuse_file_info*, enum fuse_readdir_flags) {

    // if the file path is corrupt, return an error
    if (path != "/") {
        return -1;
    }

    // fills in buffer with the following directories
    // current directories
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    // parent directories
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    // set of files
    set<string> directories;

    // directory entry used in loops
    struct dirent* directoryEntry;

    // add any remote directories to the list
    if (fileRemoteDirectory.empty() == false) {
        // open the directory
        DIR* remoteDirectory = opendir(fileRemoteDirectory.c_str());
        // if the remote directory exists
        if (remoteDirectory) {
            // read each directory
            while ((directoryEntry = readdir(remoteDirectory))) {
                string directoryName = directoryEntry->d_name;
                if (directoryName != "." && directoryName != "..") {
                    directories.insert(directoryName);
                }
            }
            // close the remote directory when finished
            closedir(remoteDirectory);
        }
    }

    // convert directory to html index
    else if (httpMode) {
        // buffer is 64 kilobytes
        vector<char> jsonBuffer(64 * 1024);
        string directoryPath = string("/list") + path;
        // bytes stores the downloaded bytes
        ssize_t bytes = apiBackend->download(directoryPath, jsonBuffer.data(), jsonBuffer.size(), 0);
        if (bytes > 0) {
            string json(jsonBuffer.data(), jsonBuffer.data() + bytes);
            findJsonNames(json, directories);
        }
    }

    // add any cache directories
    DIR* specificCacheDirectory = opendir(cacheDirectory.c_str());
    // if the cache directory exists
    if (specificCacheDirectory) {
        // read each directory
        while ((directoryEntry = readdir(specificCacheDirectory))) {
            string directoryName = directoryEntry->d_name;
            if (directoryName != "." && directoryName != "..") {
                directories.insert(directoryName);
            }
        }
        // close the cache directory when finished
        closedir(specificCacheDirectory);
    }

    for (auto &n : directories) {
        filler(buf, n.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

    return 0;

}

static int makeDirectory(const char* path, mode_t mode) {

    // get real cache path and then perform the make directory function
    if (mkdir(realCachePath(path).c_str(), mode) == -1) {
        return -1;
    }
    return 0;

}
static int removeDirectory(const char* path) {

    // get real cache path and then perform the read directory function
    if (rmdir(realCachePath(path).c_str()) == -1) {
        return -1;
    }
    return 0;
    
}
static int removeFile(const char* path) {

    // get real cache path and then perform unlink
    if (unlink(realCachePath(path).c_str()) == -1) {
        return -1;
    }
    return 0;

}

static int openFile(const char*, struct fuse_file_info*) {
    return 0;
}

static int createFile(const char*, mode_t, struct fuse_file_info*) {
    return 0;
}

static int readFile(const char* path, char* buf, size_t sz, off_t off, struct fuse_file_info*) {

    if (!fileRemoteDirectory.empty()) {
        // remove slashes, etc.
        string rp = realFilePath(path);
        // open file and store file descriptor
        int file = open(rp.c_str(), O_RDONLY);
        if (file < 0) {
            return -1;
        }
        // read from file
        ssize_t numBytes = pread(file, buf, sz, off);
        // close file
        close(file);
        // cannot be less than zero bytes
        if (numBytes < 0) {
            return -1;
        }
        // cache it too
        cache_store_file(path, buf, numBytes, off);
        // return number of bytes from file
        return (int)numBytes;
    }
    // get number of bytes and download the contents of the file
    ssize_t numBytes = dataBackend->download(path, buf, sz, off);
    if (numBytes < 0) {
        return -1;
    } else {
        return (int)numBytes;
    }

}

static int writeFile(const char* path, const char* buf, size_t sz, off_t off, struct fuse_file_info*) {

    // make sure the directory is not empty
    if (!fileRemoteDirectory.empty()) {
        // remove slashes, etc.
        string realPath = realFilePath(path);
        // O_CREAT creates the file if it doesn't exist
        // O_WRONLY opens the file anyway even if it only has write only access
        int file = open(realPath.c_str(), O_CREAT | O_WRONLY, 0644);
        if (file < 0) {
            return -1;
        }
        // write to file
        ssize_t numBytes = pwrite(file, buf, sz, off);
        // close the file
        close(file);
        if (numBytes < 0) {
            return -1;
        } else {
            return (int)numBytes;
        }
    }
    // get the number of bytes from the file
    ssize_t numBytes = dataBackend->upload(path, buf, sz, off);
    if (numBytes < 0) {
        return -1;
    } else {
        return (int)numBytes;
    }

}

static int releaseFiles(const char*, struct fuse_file_info*) {

    // cleans files that are no longer used
    cache_apply_eviction();
    return 0;

}

int main(int argc, char* argv[]) {

    // fix cache directory path
    char realPath[4096];
    if (!realpath(argv[1], realPath)) {
        if (mkdir(argv[1], 0755) == -1) {
            return -1;
        }
        if (!realpath(argv[1], realPath)) {
            return -1;
        }
    }

    // path must have been correct
    cacheDirectory = realPath;
    // timeout cache at 60
    if (cache_init(cacheDirectory.c_str(), 60) != 0) {
        fprintf(stderr, "cache_init failed\n");
        return -1;
    }

    // parse URL scheme
    string url(argv[2]);
    // checks if file:// is at the beginning of the string (position 0)
    if (url.rfind("file://", 0) == 0) {
        fileRemoteDirectory = url.substr(7);
    // checks if http:// or https:// is at the beginning of the string (position 0)
    } else if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        // enable httpMode
        httpMode = true;
        // get position in string of /api/data
        auto pos = url.find("/api/data");
        string apiBase = "";
        if (pos != -1) {
            apiBase = url.substr(0, pos) + "/api";
        } else {
            apiBase = url;
        }
        //string api_base = (pos != -1 ? url.substr(0, pos) + "/api" : url);
        apiBackend = cache_fs::create_backend(apiBase);
        if (!apiBackend) {
            fprintf(stderr, "api backend init failed\n");
            return -1;
        }
    }

    // initializes the HTTP backend
    dataBackend = cache_fs::create_backend(url);
    if (!dataBackend) {
        fprintf(stderr, "data backend init failed\n");
        return -1;
    }

    // fuse operations
    static struct fuse_operations operations = {
        .getattr  = getAttribute,
        .mkdir    = makeDirectory,
        .unlink   = removeFile,
        .rmdir    = removeDirectory,
        .open     = openFile,
        .read     = readFile,
        .write    = writeFile,
        .statfs   = getStats,
        .release  = releaseFiles,
        .readdir  = readDirectory,
        .create   = createFile,
    };

    // declare fuse arguments
    struct fuse_args args = FUSE_ARGS_INIT(argc - 2, argv + 2);
    int ret = fuse_main(args.argc, args.argv, &operations, nullptr);

    // cleanup cache at the end
    cache_cleanup();
    return ret;

}
