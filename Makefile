
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

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

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

