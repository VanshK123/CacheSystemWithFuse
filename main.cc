#define FUSE_USE_VERSION 30

#include <iostream>
#include <string>
#include <memory>
#include <cstring>

#include "cache/cache_manager.h"
#include "cache/block_store.h"

#include "cache/policy/lru_policy.h"
#include "cache/policy/time_policy.h"

#include "cache/policy/metadata/metadata_store.h"

#include "backend/backend.h"

#include "fuse/fuse.h"
#include "fuse/fuse_ops.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <mountpoint> <backend_url>" << std::endl;
        return 1;
    }

    const std::string mountpoint = argv[1];
    const std::string backend_url = argv[2];

    if (cache_init(mountpoint.c_str(), 3600) != 0) {
        std::cerr << "cache_init failed on '" << mountpoint << "'" << std::endl;
        return 1;
    }

    BlockStore blockStore(mountpoint + "/blocks", 4096);
    if (!blockStore.init()) {
        std::cerr << "BlockStore::init() failed" << std::endl;
        return 1;
    }

    LruPolicy  evictionPolicy(1000);
    TimePolicy timePolicy(3600);

    MetadataStore metaStore(mountpoint + "/cache_meta.db");
    if (!metaStore.init()) {
        std::cerr << "MetadataStore::init() failed" << std::endl;
        return 1;
    }

    auto backendPtr = cache_fs::create_backend(backend_url);
    if (!backendPtr) {
        std::cerr << "create_backend('" << backend_url << "') returned nullptr" << std::endl;
        return 1;
    }
    cache_fs::Backend* httpBackend = backendPtr.get();

    struct fuse_operations ops;
    std::memset(&ops, 0, sizeof(ops));

    if (!init_fuse_ops(&ops,
                       &blockStore,
                       &evictionPolicy,
                       &timePolicy,
                       &metaStore,
                       httpBackend))
    {
        std::cerr << "init_fuse_ops() failed" << std::endl;
        return 1;
    }

    int ret = fuse_main(argc, argv, &ops, nullptr);

    cache_cleanup();
    blockStore.cleanup();
    metaStore.cleanup();

    return ret;
}
