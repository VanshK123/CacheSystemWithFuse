BUILD_DIR := build

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release

MOUNTPOINT ?= ./mnt/cache

.PHONY: all clean run test bench

all: $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR)

$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) ..

run: all
	@mkdir -p $(MOUNTPOINT)
	@$(BUILD_DIR)/remote_cache $(MOUNTPOINT)

test: all
	@cd $(BUILD_DIR) && ctest --output-on-failure

bench:
	@./benchmarks/run_benchmarks.sh

clean:
	rm -rf $(BUILD_DIR)
