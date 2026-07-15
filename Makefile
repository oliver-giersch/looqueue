CXX := c++
INC := -I./include
LIB := -lpthread

CXXFLAGS := -std=c++20 -O2

all: test/test test/test_stress

test/test: test/test.cpp
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

test/test_stress: test/test_stress.cpp
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

clean:
	$(RM) test/test test/test_stress 2> /dev/null || true

.PHONY: all clean

