# #THIS IS ONLY FOR CACHE TESTING RIGHT NOW
# CXX        := g++
# CXXFLAGS   := -std=c++17 -Wall -Wextra
# INCLUDES   := -I./cache -I./cache/policy -I./cache/policy/metadata
# LIBS       := -lsqlite3

# CACHE_SRCS := \
# 	cache/block_store.cc \
# 	cache/cache_manager.cc \
# 	cache/policy/lru_policy.cc \
# 	cache/policy/time_policy.cc \
# 	cache/policy/metadata/metadata_store.cc

# TESTS      := test_cache test_eviction test_read

# .PHONY: all test clean

# all: $(TESTS)

# test_cache: $(CACHE_SRCS) test_cache.cc
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

# test_eviction: $(CACHE_SRCS) test_eviction.cc
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

# test_read: $(CACHE_SRCS) test_read.cc
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

# test: all
# 	@echo "=== Running test_cache ==="
# 	-@rm -rf cache_dir
# 	@./test_cache
# 	@echo "\n=== Running test_eviction ==="
# 	-@rm -rf cache_dir
# 	@./test_eviction
# 	@echo "\n=== Running test_read ==="
# 	-@rm -rf cache_dir
# 	@./test_read

# clean:
# 	-rm -f $(TESTS)
# 	-@rm -rf cache_dir


CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra
INCLUDES  := -I. -I./cache -I./cache/policy -I./cache/policy/metadata -I./backend

# Libraries needed: curl + sqlite3 + threading
LIBS      := -lcurl -lsqlite3 -pthread

CACHE_SRCS   := \
  cache/block_store.cc \
  cache/cache_manager.cc \
  cache/policy/lru_policy.cc \
  cache/policy/time_policy.cc \
  cache/policy/metadata/metadata_store.cc

BACKEND_SRCS := backend/http_backend.cc

TESTS        := test_cache test_eviction test_read test_http

.PHONY: all test clean

all: $(TESTS)

test_cache: $(CACHE_SRCS) test_cache.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -lsqlite3 -o $@

test_eviction: $(CACHE_SRCS) test_eviction.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -lsqlite3 -o $@

test_read: $(CACHE_SRCS) test_read.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -lsqlite3 -o $@

test_http: $(CACHE_SRCS) $(BACKEND_SRCS) test_http.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) -o $@

test: all
	@echo "=== test_cache ==="
	-rm -rf cache_dir; ./test_cache
	@echo "\n=== test_eviction ==="
	-rm -rf cache_dir; ./test_eviction
	@echo "\n=== test_read ==="
	-rm -rf cache_dir; ./test_read
	@echo "\n=== test_http ==="
	-rm -rf cache_dir; ./test_http

clean:
	-rm -f $(TESTS)
	-rm -f */*.o *.o
	-rm -rf cache_dir
