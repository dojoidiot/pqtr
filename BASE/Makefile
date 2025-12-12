# BASE - Web server + JWT Auth Service

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
TMP_DIR = tmp/make

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

# Objects
LIB_OBJS = $(TMP_DIR)/jwta.o $(TMP_DIR)/sqlite.o $(TMP_DIR)/mailgun.o

# Output
LIB_TARGET = $(TMP_DIR)/jwta.a
HOST_TARGET = bin/base

.PHONY: all clean

all: $(HOST_TARGET)

$(TMP_DIR):
	@mkdir -p $(TMP_DIR)

bin:
	@mkdir -p bin

$(LIB_TARGET): $(LIB_OBJS) $(SQLITE_OBJ) | $(TMP_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(HOST_TARGET): $(SRC_DIR)/host.cpp $(LIB_TARGET) $(SODIUM_LIB) | $(TMP_DIR) bin
	$(CXX) $(CXXFLAGS) -DCPPHTTPLIB_OPENSSL_SUPPORT $(INCLUDES) $< $(LIB_TARGET) $(SODIUM_LIB) -lpthread -ldl -lssl -lcrypto -o $@

$(SQLITE_OBJ): $(SQLITE_DIR)/sqlite3.c | $(TMP_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TMP_DIR)/jwta.o: $(SRC_DIR)/jwta.cpp $(INC_DIR)/jwta.hpp $(INC_DIR)/data.hpp $(INC_DIR)/mail.hpp | $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TMP_DIR)/sqlite.o: $(SRC_DIR)/plug/sqlite.cpp $(INC_DIR)/data.hpp | $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TMP_DIR)/mailgun.o: $(SRC_DIR)/plug/mailgun.cpp $(INC_DIR)/mail.hpp | $(TMP_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(TMP_DIR) bin/base
