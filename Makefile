CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -march=native -Wall -Wextra -pthread
SRC = src/main.cpp

engine: $(SRC) src/*.h
	$(CXX) $(CXXFLAGS) $(SRC) -o engine

clean:
	rm -f engine
