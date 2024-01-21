CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
SRCDIR := .
BUILDDIR := build
BINDIR := bin

# Collect all source files
SRC := $(wildcard $(SRCDIR)/*.cpp)

# Create object file names by replacing src/ with build/ and .cpp with .o
OBJ := $(SRC:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# The final executable name
TARGET := $(BINDIR)/pacman
DEBUG_TARGET := $(BINDIR)/pacman_debug

all: $(TARGET)

debug: CXXFLAGS += -g
debug: $(DEBUG_TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(DEBUG_TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -g $^ -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

format:
	clang-format -i ./main.cpp

clean:
	rm -rf $(BUILDDIR)/* $(BINDIR)/*
