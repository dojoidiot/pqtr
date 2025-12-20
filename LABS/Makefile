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
      -I$(SRC_DIR)/desk \
      -I$(SRC_DIR)/desk/part \
      -I$(SRC_DIR)/desk/pane \
      -I$(SRC_DIR)/desk/tile \
      -I$(SRC_DIR)/gear

OUT_JS = $(OUT_DIR)/labs.js
OUT_HTML = $(OUT_DIR)/labs.html
HTML_SRC = src/html/labs.html

# === Native Test Build (g++ + Dawn) ===
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

DAWN_DIR = lib/dawn/install
DAWN_BUILD = lib/dawn/build
DAWN_INC = -I$(DAWN_DIR)/include
DAWN_LIB = $(DAWN_DIR)/lib/libwebgpu_dawn.a
DAWN_NATIVE_LIBS = $(DAWN_BUILD)/src/dawn/native/libdawn_native.a \
                   $(DAWN_BUILD)/src/dawn/libdawn_proc.a

TEST_DIR = tmp/test
TEST_INC = -Iinc -Isrc/test/gear -Isrc/test/pipe -Isrc/test/vibe $(DAWN_INC)

# Test executables
TEST_GEAR = $(TEST_DIR)/test_gear
TEST_PIPE = $(TEST_DIR)/test_pipe
TEST_VIBE = $(TEST_DIR)/test_vibe
TEST_FLOW = $(TEST_DIR)/test_flow
TEST_LUTE = $(TEST_DIR)/test_lute

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
test: $(TEST_FLOW)
	@echo "=== FLOW Test ==="
	@cd $(shell pwd) && $(TEST_FLOW) || true

# GEAR test
$(TEST_GEAR): src/test/gear/test_dawn.cpp src/test/gear/pipe.cpp src/main/wgpu/wgpu.cpp | $(DAWN_LIB)
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $^ $(DAWN_LIB) -lpthread -ldl -o $@

# Check Dawn is built
$(DAWN_LIB):
	@echo "Dawn not built. Run: cd lib && ./dawn.sh"
	@exit 1

# FLOW test
FLOW_SRCS = src/test/flow/flow.cpp \
            src/main/flow/flow.cpp \
            src/main/flow/part/tree.cpp \
            src/main/flow/part/load.cpp \
            src/main/flow/part/swap.cpp \
            src/main/flow/part/png.cpp \
            src/main/flow/part/jpg.cpp \
            src/main/flow/part/sony.cpp \
            src/main/flow/part/warp.cpp \
            src/main/flow/part/head.cpp \
            src/main/flow/part/lute.cpp \
            src/main/lute/lute.cpp \
            src/main/wgpu/wgpu.cpp \
            src/main/pipe/pipe.cpp \
            src/main/pipe/part/pipe/node.cpp \
            src/main/pipe/part/pipe/jpeg.cpp

DAWN_NATIVE_INC = -Ilib/dawn/include

$(TEST_FLOW): $(FLOW_SRCS) | $(DAWN_LIB)
	@mkdir -p $(TEST_DIR)
	@mkdir -p tmp/var/flow
	$(CXX) $(CXXFLAGS) -Iinc $(DAWN_INC) $(DAWN_NATIVE_INC) $^ $(DAWN_NATIVE_LIBS) $(DAWN_LIB) -lpthread -ldl -o $@

# LUTE test (camera profile learning)
LUTE_SRCS = src/test/lute/lute.cpp \
            src/main/lute/lute.cpp \
            src/main/flow/flow.cpp \
            src/main/flow/part/tree.cpp \
            src/main/flow/part/load.cpp \
            src/main/flow/part/swap.cpp \
            src/main/flow/part/png.cpp \
            src/main/flow/part/jpg.cpp \
            src/main/flow/part/sony.cpp \
            src/main/flow/part/warp.cpp \
            src/main/flow/part/head.cpp \
            src/main/flow/part/lute.cpp \
            src/main/wgpu/wgpu.cpp \
            src/main/pipe/pipe.cpp \
            src/main/pipe/part/pipe/node.cpp \
            src/main/pipe/part/pipe/jpeg.cpp

$(TEST_LUTE): $(LUTE_SRCS) | $(DAWN_LIB)
	@mkdir -p $(TEST_DIR)
	@mkdir -p tmp/var/flow
	$(CXX) $(CXXFLAGS) -Iinc $(DAWN_INC) $(DAWN_NATIVE_INC) $^ $(DAWN_NATIVE_LIBS) $(DAWN_LIB) -lpthread -ldl -o $@

lute: $(TEST_LUTE)
	@echo "=== LUTE Test ==="
	@cd $(shell pwd) && $(TEST_LUTE) || true

tidy:
	rm -rf tmp
