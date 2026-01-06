# Makefile for args - Standalone RAW processing pipeline
#
# Usage:
#   make            - build the pipeline
#   make run        - process default test image
#   make test       - process both test images
#   make clean      - remove build artifacts

CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -O2 -I inc

BUILD := tmp/build
VAR := tmp/var

.PHONY: all run test clean

all: $(BUILD)/args

$(BUILD)/args: src/main/main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< -lm

run: $(BUILD)/args
	@mkdir -p $(VAR)
	./$(BUILD)/args src/test/raws/DSC00458.ARW $(VAR)/out.png

test: $(BUILD)/args
	@mkdir -p $(VAR)
	./$(BUILD)/args src/test/raws/DSC00458.ARW $(VAR)/out_458.png
	./$(BUILD)/args src/test/raws/DSC00202.ARW $(VAR)/out_202.png

clean:
	rm -rf tmp
