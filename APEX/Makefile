# APEX Library Makefile
# Builds lib/apex.a static library
#
# APEX provides:
#   - pqtr::Hold (smart pointer) - header-only
#   - pqtr::Sink (buffer abstraction)
#   - pqtr::sink() - Sink factory

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
INCLUDES = -I./inc

SRC_DIR = src/main
OBJ_DIR = tmp/obj
LIB_DIR = lib

OBJS = $(OBJ_DIR)/sink.o
APEX_A = $(LIB_DIR)/apex.a

.PHONY: make tidy

make: $(APEX_A)

$(APEX_A): $(OBJS) | $(LIB_DIR)
	ar rcs $@ $(OBJS)
	@echo "Built: $@"

$(OBJ_DIR)/sink.o: $(SRC_DIR)/part/sink.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

tidy:
	rm -rf $(OBJ_DIR) $(APEX_A)
