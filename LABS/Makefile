# LABS Makefile - Build LABS for WebAssembly
#
# Targets:
#   make      Build WASM application
#   make test Run tests (placeholder)
#   make tidy Clean build artifacts
#
# Requires: Emscripten SDK (first time setup)
#   cd lib/emsdk && ./emsdk install latest && ./emsdk activate latest

# Emscripten SDK paths
EMSDK = lib/emsdk
EMSCRIPTEN = $(EMSDK)/upstream/emscripten
export PATH := $(CURDIR)/$(EMSCRIPTEN):$(CURDIR)/$(EMSDK):$(PATH)

IMGUI = lib/imgui
GEAR = ../GEAR
PIPE = ../PIPE

SRC = src/main/labs.cpp \
      $(IMGUI)/imgui.cpp \
      $(IMGUI)/imgui_draw.cpp \
      $(IMGUI)/imgui_tables.cpp \
      $(IMGUI)/imgui_widgets.cpp \
      $(IMGUI)/backends/imgui_impl_glfw.cpp \
      $(IMGUI)/backends/imgui_impl_opengl3.cpp \
      $(PIPE)/src/main/part/pipe/node.cpp \
      $(GEAR)/src/main/part/sony/prepare_pure.cpp \
      $(GEAR)/src/main/sony_link.cpp

INC = -I$(IMGUI) -I$(IMGUI)/backends \
      -I$(GEAR)/inc \
      -I$(GEAR)/lib \
      -I$(GEAR)/src/main/part \
      -I$(PIPE)/inc \
      -Isrc/main

# Emscripten flags
# Note: Use -sUSE_GLFW=3 for older emsdk (3.x), --use-port=contrib.glfw3 for newer (4.x)
EMFLAGS = -std=c++17 \
          -sUSE_GLFW=3 \
          -sUSE_WEBGL2=1 \
          -sFULL_ES3=1 \
          -sWASM=1 \
          -sALLOW_MEMORY_GROWTH=1 \
          -sNO_EXIT_RUNTIME=1 \
          -sASSERTIONS=1 \
          -sFETCH

OUT = tmp/wasm

.PHONY: all test tidy help

all: $(OUT)/labs.js $(OUT)/labs.html

help:
	@echo "LABS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make      Build WASM application"
	@echo "  make test Run tests"
	@echo "  make tidy Clean build artifacts"

$(OUT)/labs.js: $(SRC) | $(OUT)
	emcc $(SRC) $(INC) $(EMFLAGS) -O1 -o $@

$(OUT)/labs.html: src/main/labs.html $(OUT)/labs.js
	sed 's|{{{ SCRIPT }}}|<script src="labs.js?v=$(shell date +%s)"></script>|' $< > $@

$(OUT):
	mkdir -p $(OUT)

test:
	@echo "LABS: No tests implemented yet"

tidy:
	rm -rf $(OUT)
