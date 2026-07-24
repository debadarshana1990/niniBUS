
CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -MMD -MP
CPPFLAGS := -I.
AR := ar
ARFLAGS := rcs

LIB := libniniBUS.a

LIB_SRCS := niniBUS.cpp Lane.cpp
LIB_OBJS := $(LIB_SRCS:.cpp=.o)
LIB_HEADERS := cfifo.h

DEPS := $(LIB_OBJS:.o=.d)

.PHONY: all build lib library niniBUS clean clean-meta debug

all: $(LIB)

build: $(LIB)

lib library: all

$(LIB): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

niniBUS: lib

%.o: %.cpp $(LIB_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(LIB_OBJS) $(DEPS) $(LIB)

clean-meta:
	rm -f $(LIB_OBJS) $(DEPS)

debug: CXXFLAGS += -g -O0
debug: clean lib
