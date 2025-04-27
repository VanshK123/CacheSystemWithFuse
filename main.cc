#include <iostream>
#include <string>
#include <fuse3/fuse.h>

#include "cache/cache_manager.h"
#include "cache/block_store.h"
#include "cache/policy/lru_policy.h"
#include "cache/metadata/metadata_store.h"
#include "backend/http_backend.h"
#include "fuse/fuse_ops.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mountpoint> [--cache-dir DIR] [--timeout SEC]" << std::endl;
        return 1;
    }

    std::string mountpoint = argv[1];
    std::string cacheDir    = "./.cache";
    int timeout             = 3600;
    size_t blockSize        = 4096;
    size_t policyCapacity   = 1000;      
    

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cache-dir" && i + 1 < argc) {
            cacheDir = argv[++i];
        } else if (arg == "--timeout" && i + 1 < argc) {
            timeout = std::stoi(argv[++i]);
        }
    }

    if (cache_init(cacheDir.c_str(), timeout) != 0) {
        std::cerr << "Failed to initialize cache manager" << std::endl;
        return 1;
    }

    BlockStore blockStore(cacheDir + "/blocks", blockSize);
    if (!blockStore.init()) {
        std::cerr << "Failed to initialize block store" << std::endl;
        return 1;
    }

    MetadataStore metaStore(cacheDir + "/metadata.db");
    if (!metaStore.init()) {
        std::cerr << "Failed to initialize metadata store" << std::endl;
        return 1;
    }

    LruPolicy evictionPolicy(policyCapacity);

    HttpBackend httpBackend;

    struct fuse_operations ops = {};
    if (!init_fuse_ops(&ops, &blockStore, &evictionPolicy, &metaStore, &httpBackend)) {
        std::cerr << "Failed to initialize FUSE operations" << std::endl;
        return 1;
    }

    int ret = fuse_main(argc, argv, &ops, nullptr);

    cache_cleanup();
    return ret;
}
