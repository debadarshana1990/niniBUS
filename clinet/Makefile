CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
TARGET := client
SRCS := client.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
