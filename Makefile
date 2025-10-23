# Makefile for CompareScan
CXX      = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-c++17-extensions -Wno-c++14-compat -mmacosx-version-min=13.0 -I../common
ROOTCONF = $(shell root-config --cflags --libs | sed 's/-std=c++14//g' | sed 's/-std=c++17//g')
SOURCES  = CompareScan.cpp ../common/Points.cpp
TARGET   = comparescan

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) $(ROOTCONF) -lGui -std=c++17 -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

