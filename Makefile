CXX        := g++
CXXFLAGS   := -std=c++17 -Wall -Wextra -DPREFETCH_WINDOW=4

# ---------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------
INCLUDES := \
    -I. \
    -I./cache -I./cache/policy -I./cache/policy/metadata \
    -I./backend -I./fuse

# ---------------------------------------------------------------
# External libs
# ---------------------------------------------------------------
LIBSQLITE := -lsqlite3
LIBCURL   := -lcurl
LIBPTHREAD:= -pthread
LIBFUSE   := -lfuse3 $(LIBPTHREAD)

# ---------------------------------------------------------------
# Source file groups
# ---------------------------------------------------------------
CACHE_SRCS := \
    cache/thread_pool.cc \
    cache/block_store.cc \
    cache/cache_manager.cc \
    cache/policy/lru_policy.cc \
    cache/policy/time_policy.cc \
    cache/policy/metadata/metadata_store.cc

BACKEND_SRCS := backend/http_backend.cc
FUSE_SRC     := fuse/fuse.cc

# ---------------------------------------------------------------
# Test + binary targets
# ---------------------------------------------------------------
TESTS := test_cache test_eviction test_read test_http
BIN    := remote_cache
LIB    := libcache.so

.PHONY: all test clean

all: $(BIN) $(TESTS) $(LIB)

# ---- unit tests ------------------------------------------------
test_cache:    $(CACHE_SRCS) $(BACKEND_SRCS) test_cache.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBCURL) $(LIBSQLITE) $(LIBPTHREAD) -o $@

test_eviction: $(CACHE_SRCS) $(BACKEND_SRCS) test_eviction.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBCURL) $(LIBSQLITE) $(LIBPTHREAD) -o $@

test_read:     $(CACHE_SRCS) $(BACKEND_SRCS) test_read.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBCURL) $(LIBSQLITE) $(LIBPTHREAD) -o $@

test_http: $(CACHE_SRCS) $(BACKEND_SRCS) test_http.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBCURL) $(LIBSQLITE) $(LIBPTHREAD) -o $@

# ---- main CLI/FUSE binary -------------------------------------
remote_cache: $(CACHE_SRCS) $(BACKEND_SRCS) $(FUSE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBCURL) $(LIBSQLITE) $(LIBFUSE) -o $@

# ---- shared library -------------------------------------------
libcache.so: $(CACHE_SRCS) $(BACKEND_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -shared $^ $(LIBCURL) $(LIBSQLITE) $(LIBPTHREAD) -o $@


# ---------------------------------------------------------------
# Convenience targets
# ---------------------------------------------------------------
test: all
	@echo "=== test_cache ==="
	-rm -rf cache_dir; ./test_cache
	@echo "\n=== test_eviction ==="
	-rm -rf cache_dir; ./test_eviction
	@echo "\n=== test_read ==="
	-rm -rf cache_dir; ./test_read
	@echo "\n=== test_http ==="
	-rm -rf cache_dir; ./test_http
	@echo "\n=== test_fuse ==="
	./test_fuse.sh

clean:
	-rm -f $(BIN) $(TESTS) $(LIB)
	-rm -rf cache_dir mnt/fuse_test

