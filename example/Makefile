CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP
CPPFLAGS := -I..
LDFLAGS := -L.. -lniniBUS

TARGET := niniBUS_example
SRCS := main.cpp
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)
LIB := ../libniniBUS.a

.PHONY: all build clean clean-meta run debug

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

debug: CXXFLAGS += -g -O0
debug: clean build
