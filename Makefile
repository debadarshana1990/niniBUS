
# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP
LDFLAGS :=

# Target
TARGET := niniBUS

# Sources and objects
SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all build niniBUS clean run debug

all: build

# build produces the executable and keeps object/dependency files
build: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $^ $(LDFLAGS)

# make niniBUS will build then remove intermediate meta files, leaving only the executable
niniBUS: build
	@echo "Removing meta files (.o .d), keeping $(TARGET)"
	rm -f $(OBJS) $(DEPS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include dependency files if they exist
-include $(DEPS)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

run: all
	./$(TARGET)

debug: CXXFLAGS += -g -O0
debug: clean all

