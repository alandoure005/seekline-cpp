CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
.PHONY: build test clean
build:
	mkdir -p build
	$(CXX) $(CXXFLAGS) src/main.cpp -o build/seekline
test: build
	python3 -m unittest discover -s tests -v
clean:
	rm -rf build
