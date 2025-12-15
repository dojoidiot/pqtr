# BASE Makefile - Native Build
.PHONY: test tidy

CXX = g++
CC = gcc
AR = ar
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CFLAGS = -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_FTS5
ARFLAGS = rcs

TMP_DIR = tmp/make
LIB_TARGET = $(TMP_DIR)/base.a
HOST_TARGET = tmp/base
SODIUM_LIB = lib/libsodium/src/libsodium/.libs/libsodium.a
SQLITE_OBJ = $(TMP_DIR)/sqlite3.o

INCLUDES = -Iinc -Ilib/libsodium/src/libsodium/include -Ilib/sqlite -Ilib/cpp-httplib
LIB_OBJS = $(TMP_DIR)/http.o $(TMP_DIR)/crypto.o $(TMP_DIR)/jwt.o $(TMP_DIR)/sqlite.o $(TMP_DIR)/mailgun.o $(TMP_DIR)/console.o

# Default Goal: Build the native executable.
$(HOST_TARGET): src/main/base.cpp $(LIB_TARGET) $(SODIUM_LIB)
	@mkdir -p tmp
	$(CXX) $(CXXFLAGS) -DCPPHTTPLIB_OPENSSL_SUPPORT $(INCLUDES) $< $(LIB_TARGET) $(SODIUM_LIB) -lpthread -ldl -lssl -lcrypto -o $@

$(LIB_TARGET): $(LIB_OBJS) $(SQLITE_OBJ)
	@mkdir -p $(TMP_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(SQLITE_OBJ): lib/sqlite/sqlite3.c
	@mkdir -p $(TMP_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TMP_DIR)/%.o: src/main/part/%.cpp
	@mkdir -p $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TMP_DIR)/%.o: src/main/plug/%.cpp
	@mkdir -p $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

tidy:
	rm -rf tmp

test:
	@echo "No tests for BASE."