# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fopenmp

# Source files
SRCS = main.cc s_dmalloc.cc s_permissions.cc
OBJS = $(SRCS:.cc=.o)

# Output binary
TARGET = main

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile .cc to .o
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
