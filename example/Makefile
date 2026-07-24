CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP
CPPFLAGS := -I..
LDFLAGS := -L.. -lniniBUS

TARGET := niniBUS_test
CFIFO_TARGET := cfifo_test
SRCS := niniBUS_test.cpp
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)
LIB := ../libniniBUS.a

.PHONY: all build clean clean-meta run test cfifo-test debug

all: $(TARGET) $(CFIFO_TARGET)
	rm -f $(OBJS) $(DEPS)

build: $(TARGET) $(CFIFO_TARGET)

$(TARGET): $(OBJS) $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(CFIFO_TARGET): cfifo_test.cpp ../cfifo.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ cfifo_test.cpp

$(LIB):
	$(MAKE) -C ..

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) $(CFIFO_TARGET) cfifo_test.d

clean-meta:
	rm -f $(OBJS) $(DEPS)

run: all
	./$(TARGET)

test: run cfifo-test

cfifo-test: $(CFIFO_TARGET)
	./$(CFIFO_TARGET)

debug: CXXFLAGS += -g -O0
debug: clean build
