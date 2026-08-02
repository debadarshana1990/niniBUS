CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS := -I..
LDFLAGS := -L.. -lniniBUS

TARGET := niniBUS_test
CFIFO_TARGET := cfifo_test
PERFORMANCE_TARGET := performance_test
THREAD_TARGET := niniBUS_thread_test
THREAD_PERFORMANCE_TARGET := thread_performance_test
PROFILING_TARGET := profiling
LIB := ../libniniBUS.a

.PHONY: all build clean clean-meta run test cfifo-test performance-test \
	thread-performance-test profiling-test thread-test thread-test-tsan debug

all: $(TARGET) $(CFIFO_TARGET) $(PERFORMANCE_TARGET) $(THREAD_TARGET) \
	$(THREAD_PERFORMANCE_TARGET) $(PROFILING_TARGET)

build: all

$(TARGET): niniBUS_test.cpp $(LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ niniBUS_test.cpp $(LDFLAGS)

$(CFIFO_TARGET): cfifo_test.cpp ../cfifo.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ cfifo_test.cpp

$(PERFORMANCE_TARGET): performance_test.cpp $(LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ performance_test.cpp $(LDFLAGS)

$(THREAD_TARGET): niniBUS_thread_test.cpp $(LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -o $@ niniBUS_thread_test.cpp $(LDFLAGS)

$(THREAD_PERFORMANCE_TARGET): thread_performance_test.cpp $(LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -o $@ thread_performance_test.cpp $(LDFLAGS)

$(PROFILING_TARGET): profiling.cpp $(LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -o $@ profiling.cpp $(LDFLAGS)

$(LIB):
	$(MAKE) -C ..

clean:
	rm -f $(TARGET) $(CFIFO_TARGET) $(PERFORMANCE_TARGET) \
		$(THREAD_TARGET) $(THREAD_TARGET)_tsan \
		$(THREAD_PERFORMANCE_TARGET) $(PROFILING_TARGET) *.o *.d
	rm -rf $(THREAD_TARGET)_tsan.dSYM

clean-meta:
	rm -f *.o *.d

run: all
	./$(TARGET)

test: cfifo-test thread-test

cfifo-test: $(CFIFO_TARGET)
	./$(CFIFO_TARGET)

performance-test: $(PERFORMANCE_TARGET)
	./$(PERFORMANCE_TARGET)

thread-performance-test: $(THREAD_PERFORMANCE_TARGET)
	./$(THREAD_PERFORMANCE_TARGET)

profiling-test: $(PROFILING_TARGET)
	./$(PROFILING_TARGET)

thread-test: $(TARGET) $(THREAD_TARGET)
	./$(TARGET)
	./$(THREAD_TARGET)

thread-test-tsan:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -fsanitize=thread -g \
		-o $(THREAD_TARGET)_tsan niniBUS_thread_test.cpp ../niniBUS.cpp ../Lane.cpp
	./$(THREAD_TARGET)_tsan

debug: CXXFLAGS += -g -O0
debug: clean build
