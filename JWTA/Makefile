# JWTA - JWT Web Auth Service
# Static library build

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
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

# Includes
INCLUDES = -I$(INC_DIR) -I$(SODIUM_INC)

# Sources
SRCS = $(SRC_DIR)/jwta.cpp
OBJS = $(SRCS:.cpp=.o)

# Output
TARGET = $(LIB_DIR)/jwta.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
$(SRC_DIR)/jwta.o: $(INC_DIR)/jwta.hpp
