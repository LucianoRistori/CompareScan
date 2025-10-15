# Makefile for CompareScan
CXX      = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
ROOTCONF = $(shell root-config --cflags --libs)
SOURCES  = CompareScan.cpp Points.cpp
TARGET   = CompareScan

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) $(ROOTCONF) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
