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

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

format:
	clang-format -i ./main.cpp

clean:
	rm -rf $(BUILDDIR)/* $(BINDIR)/*
