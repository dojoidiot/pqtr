# LABS Makefile - WebAssembly App + Native Tests
.PHONY: all test tidy

# === WASM Build (em++) ===
EMXX = em++
EMFLAGS = -std=c++17 -Wall -O2 -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sFULL_ES3=1 \
          -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sNO_EXIT_RUNTIME=1 -sASSERTIONS=1 -sFETCH

SRC_DIR = src/main
OBJ_DIR = tmp/obj
OUT_DIR = tmp/wasm

# Exclude wgpu/ (native Dawn code, not for WASM)
SOURCES = $(shell find $(SRC_DIR) -name '*.cpp' ! -path '*/wgpu/*')
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# ImGui sources
IMGUI_SRCS = lib/imgui/imgui.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_tables.cpp lib/imgui/imgui_widgets.cpp \
             lib/imgui/backends/imgui_impl_glfw.cpp lib/imgui/backends/imgui_impl_opengl3.cpp
IMGUI_OBJS = $(patsubst lib/%.cpp,$(OBJ_DIR)/lib/%.o,$(IMGUI_SRCS))

INC = -Ilib/imgui -Ilib/imgui/backends \
      -Iinc \
      -Iinc/gear \
      -Iinc/pipe \
      -Ilib \
      -Ilib/glfw/deps \
      -I$(SRC_DIR)/labs/part \
      -I$(SRC_DIR)/gear

OUT_JS = $(OUT_DIR)/labs.js
OUT_HTML = $(OUT_DIR)/labs.html
HTML_SRC = src/html/labs.html

# === Native Test Build (g++ + Dawn) ===
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

DAWN_DIR = lib/dawn/install
DAWN_INC = -I$(DAWN_DIR)/include
DAWN_LIB = $(DAWN_DIR)/lib/libwebgpu_dawn.a

TEST_DIR = tmp/test
TEST_INC = -Iinc -Isrc/test/gear -Isrc/test/pipe -Isrc/test/vibe $(DAWN_INC)

# Test executables
TEST_GEAR = $(TEST_DIR)/test_gear
TEST_PIPE = $(TEST_DIR)/test_pipe
TEST_VIBE = $(TEST_DIR)/test_vibe

# === Targets ===

all: $(OUT_JS) $(OUT_HTML)

# WASM output
$(OUT_JS): $(OBJS) $(IMGUI_OBJS)
	@mkdir -p $(OUT_DIR)
	$(EMXX) $(OBJS) $(IMGUI_OBJS) $(INC) $(EMFLAGS) -o $@

$(OUT_HTML): $(HTML_SRC)
	@mkdir -p $(OUT_DIR)
	sed 's|{{{ SCRIPT }}}|<script src="labs.js?v=$(shell date +%s)"></script>|' $< > $@

# WASM object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(EMXX) $(EMFLAGS) $(INC) -c $< -o $@

# ImGui object files
$(OBJ_DIR)/lib/%.o: lib/%.cpp
	@mkdir -p $(dir $@)
	$(EMXX) $(EMFLAGS) $(INC) -c $< -o $@

# === Tests (native) ===
test: $(TEST_GEAR)
	@echo "=== GEAR Tests ==="
	@cd $(shell pwd) && $(TEST_GEAR) || true

# GEAR test
$(TEST_GEAR): src/test/gear/test_dawn.cpp src/test/gear/pipe.cpp src/main/wgpu/wgpu.cpp | $(DAWN_LIB)
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $^ $(DAWN_LIB) -lpthread -ldl -o $@

# Check Dawn is built
$(DAWN_LIB):
	@echo "Dawn not built. Run: cd lib && ./dawn.sh"
	@exit 1

tidy:
	rm -rf tmp
