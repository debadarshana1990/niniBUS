CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP
CPPFLAGS := -I..
LDFLAGS := -L.. -lniniBUS

TARGET := hello
SRCS := hello.cpp
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)
LIB := ../libniniBUS.a

.PHONY: all build clean clean-meta run test debug

all: $(TARGET)
	rm -f $(OBJS) $(DEPS)

build: $(TARGET)

$(TARGET): $(OBJS) $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(LIB):
	$(MAKE) -C ..

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

clean-meta:
	rm -f $(OBJS) $(DEPS)

run: all
	./$(TARGET)

test: run

debug: CXXFLAGS += -g -O0
debug: clean build
