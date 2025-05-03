#THIS IS ONLY FOR CACHE TESTING RIGHT NOW
CXX        := g++
CXXFLAGS   := -std=c++17 -Wall -Wextra
INCLUDES   := -I./cache -I./cache/policy -I./cache/policy/metadata
LIBS       := -lsqlite3

CACHE_SRCS := \
	cache/block_store.cc \
	cache/cache_manager.cc \
	cache/policy/lru_policy.cc \
	cache/policy/time_policy.cc \
	cache/policy/metadata/metadata_store.cc

TESTS      := test_cache test_eviction test_read

.PHONY: all test clean

all: $(TESTS)

test_cache: $(CACHE_SRCS) test_cache.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

test_eviction: $(CACHE_SRCS) test_eviction.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

test_read: $(CACHE_SRCS) test_read.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

test: all
	@echo "=== Running test_cache ==="
	-@rm -rf cache_dir
	@./test_cache
	@echo "\n=== Running test_eviction ==="
	-@rm -rf cache_dir
	@./test_eviction
	@echo "\n=== Running test_read ==="
	-@rm -rf cache_dir
	@./test_read

clean:
	-rm -f $(TESTS)
	-@rm -rf cache_dir
