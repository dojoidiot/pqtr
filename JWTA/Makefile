# JWTA - JWT Web Auth Service
# Static library and server build

CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CFLAGS = -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_FTS5
AR = ar
ARFLAGS = rcs

# Directories
SRC_DIR = src/main
INC_DIR = inc
LIB_DIR = lib
BIN_DIR = bin
TMP_DIR = tmp

# libsodium
SODIUM_INC = $(LIB_DIR)/libsodium/src/libsodium/include
SODIUM_LIB = $(LIB_DIR)/libsodium/src/libsodium/.libs/libsodium.a

# SQLite
SQLITE_DIR = $(LIB_DIR)/sqlite
SQLITE_OBJ = $(TMP_DIR)/sqlite3.o

# cpp-httplib
HTTPLIB_INC = $(LIB_DIR)/cpp-httplib

# Includes
INCLUDES = -I$(INC_DIR) -I$(SODIUM_INC) -I$(SQLITE_DIR) -I$(HTTPLIB_INC)

# Library sources - objects go to tmp/
LIB_SRCS = $(SRC_DIR)/jwta.cpp $(SRC_DIR)/sqlite_store.cpp
LIB_OBJS = $(TMP_DIR)/jwta.o $(TMP_DIR)/sqlite_store.o

# Output
LIB_TARGET = $(TMP_DIR)/jwta.a
SERVER_TARGET = $(BIN_DIR)/jwta

.PHONY: all clean server lib

all: lib server

lib: $(LIB_TARGET)

server: $(SERVER_TARGET)

$(TMP_DIR):
	@mkdir -p $(TMP_DIR)

$(LIB_TARGET): $(LIB_OBJS) $(SQLITE_OBJ) | $(TMP_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(SERVER_TARGET): $(SRC_DIR)/server.cpp $(LIB_TARGET) $(SODIUM_LIB)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(LIB_TARGET) $(SODIUM_LIB) -lpthread -ldl -lkeyutils -o $@

$(SQLITE_OBJ): $(SQLITE_DIR)/sqlite3.c | $(TMP_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TMP_DIR)/jwta.o: $(SRC_DIR)/jwta.cpp $(INC_DIR)/jwta.hpp | $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TMP_DIR)/sqlite_store.o: $(SRC_DIR)/sqlite_store.cpp $(INC_DIR)/sqlite_store.hpp $(INC_DIR)/jwta.hpp | $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(TMP_DIR) $(SERVER_TARGET)
