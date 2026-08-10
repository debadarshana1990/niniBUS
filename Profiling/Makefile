CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS := -I..
BENCHMARK_DIR := ../../benchmarking/benchmark
BENCHMARK_CPPFLAGS := -isystem $(BENCHMARK_DIR)/include
BENCHMARK_LDFLAGS := -L$(BENCHMARK_DIR)/build/src
BENCHMARK_LDLIBS := -lbenchmark -lpthread
VECTOR_TARGET := cfifo_vector_bm
ARRAY_TARGET := cfifo_array_bm
RECLAIM_VECTOR_TARGET := reclaim_vector_bm
RECLAIM_ARRAY_TARGET := reclaim_array_bm

.PHONY: all clean

all: $(VECTOR_TARGET) $(ARRAY_TARGET) \
	$(RECLAIM_VECTOR_TARGET) $(RECLAIM_ARRAY_TARGET)

$(VECTOR_TARGET): cfifo_profiling.cpp ../cfifo.h
	$(CXX) $(CPPFLAGS) $(BENCHMARK_CPPFLAGS) $(CXXFLAGS) $< \
		$(BENCHMARK_LDFLAGS) $(BENCHMARK_LDLIBS) -o $@

$(ARRAY_TARGET): cfifo_profiling.cpp ../cfifo.h
	$(CXX) $(CPPFLAGS) $(BENCHMARK_CPPFLAGS) $(CXXFLAGS) \
		-DNBUS_CFIFO_USE_ARRAY $< $(BENCHMARK_LDFLAGS) \
		$(BENCHMARK_LDLIBS) -o $@

$(RECLAIM_VECTOR_TARGET): reclaim_profiling.cpp ../cfifo.h
	$(CXX) $(CPPFLAGS) $(BENCHMARK_CPPFLAGS) $(CXXFLAGS) $< \
		$(BENCHMARK_LDFLAGS) $(BENCHMARK_LDLIBS) -o $@

$(RECLAIM_ARRAY_TARGET): reclaim_profiling.cpp ../cfifo.h
	$(CXX) $(CPPFLAGS) $(BENCHMARK_CPPFLAGS) $(CXXFLAGS) \
		-DNBUS_CFIFO_USE_ARRAY $< $(BENCHMARK_LDFLAGS) \
		$(BENCHMARK_LDLIBS) -o $@

clean:
	rm -f $(VECTOR_TARGET) $(ARRAY_TARGET) \
		$(RECLAIM_VECTOR_TARGET) $(RECLAIM_ARRAY_TARGET) \
		cfifowrite_vector_bm cfifowrite_array_bm \
		cfiforead_vector_bm cfiforead_array_bm *.o
