# JWTA - JWT Web Auth Service
# Static library build

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

# libsodium
SODIUM_INC = $(LIB_DIR)/libsodium/src/libsodium/include
SODIUM_LIB = $(LIB_DIR)/libsodium/src/libsodium/.libs/libsodium.a

# SQLite
SQLITE_DIR = $(LIB_DIR)/sqlite
SQLITE_OBJ = $(SQLITE_DIR)/sqlite3.o

# Includes
INCLUDES = -I$(INC_DIR) -I$(SODIUM_INC) -I$(SQLITE_DIR)

# Sources
SRCS = $(SRC_DIR)/jwta.cpp $(SRC_DIR)/sqlite_store.cpp
OBJS = $(SRCS:.cpp=.o)

# Output
TARGET = $(LIB_DIR)/jwta.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(SQLITE_OBJ)
	@mkdir -p $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(SQLITE_OBJ): $(SQLITE_DIR)/sqlite3.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(SQLITE_OBJ) $(TARGET)

# Dependencies
$(SRC_DIR)/jwta.o: $(INC_DIR)/jwta.hpp
$(SRC_DIR)/sqlite_store.o: $(INC_DIR)/sqlite_store.hpp $(INC_DIR)/jwta.hpp
