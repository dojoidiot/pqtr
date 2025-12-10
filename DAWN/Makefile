# DAWN Library Makefile
# Builds lib/dawn.a from Dawn WebGPU
#
# Prerequisites:
#   - CMake 3.13+
#   - Vulkan SDK (Linux) or Xcode (macOS)

DAWN_SRC = lib/dawn
DEPOT_TOOLS = lib/depot_tools
BUILD_DIR = $(DAWN_SRC)/out/Release
DAWN_A = lib/dawn.a
DAWN_O = tmp/dawn.o

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

# Dawn includes
DAWN_INC = -I./inc \
           -I$(DAWN_SRC)/include \
           -I$(BUILD_DIR)/gen/include \
           -I$(DAWN_SRC)/src

# Add depot_tools to PATH for gclient
export PATH := $(CURDIR)/$(DEPOT_TOOLS):$(PATH)

.PHONY: make tidy deps

make: $(DAWN_A)

# Compile our wrapper
$(DAWN_O): src/dawn.cpp inc/dawn.hpp | $(BUILD_DIR)/src/dawn/native/libdawn_native.a
	@mkdir -p tmp
	$(CXX) $(CXXFLAGS) $(DAWN_INC) -c $< -o $@

# Build Dawn static library
$(DAWN_A): $(DAWN_O) $(BUILD_DIR)/src/dawn/native/libdawn_native.a
	@echo "Collecting Dawn libraries..."
	@mkdir -p lib
	@# Combine relevant Dawn static libs with our wrapper
	ar -rcT $@ \
		$(DAWN_O) \
		$(BUILD_DIR)/src/dawn/native/libdawn_native.a \
		$(BUILD_DIR)/src/dawn/platform/libdawn_platform.a \
		$(BUILD_DIR)/src/dawn/common/libdawn_common.a
	@echo "Built: $@"

# Build Dawn via CMake
$(BUILD_DIR)/src/dawn/native/libdawn_native.a: | deps
	@echo "Building Dawn..."
	cd $(DAWN_SRC) && cmake -B out/Release \
		-DCMAKE_BUILD_TYPE=Release \
		-DDAWN_BUILD_SAMPLES=OFF \
		-DDAWN_BUILD_TESTS=OFF \
		-DDAWN_ENABLE_INSTALL=OFF \
		-DTINT_BUILD_SAMPLES=OFF \
		-DTINT_BUILD_TESTS=OFF
	cd $(DAWN_SRC) && cmake --build out/Release -j$(shell nproc)

# Fetch Dawn dependencies (run once)
deps:
	@if [ ! -f $(DAWN_SRC)/.gclient_done ]; then \
		echo "Fetching Dawn dependencies..."; \
		cd $(DAWN_SRC) && cp scripts/standalone.gclient .gclient && gclient sync; \
		touch $(DAWN_SRC)/.gclient_done; \
	fi

tidy:
	rm -rf $(BUILD_DIR) $(DAWN_A) tmp/

info:
	@echo "Dawn source: $(DAWN_SRC)"
	@echo "Build dir: $(BUILD_DIR)"
	@echo "Output: $(DAWN_A)"
