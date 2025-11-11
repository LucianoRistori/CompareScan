# ================================================================
# CompareScan Makefile
# Author: Luciano Ristori
# Description:
#   Compares two sets of (X,Y,Z) coordinates, computes ΔX, ΔY, ΔZ, ΔR,
#   and visualizes differences with histograms and 2D color maps.
# ================================================================

CXX       = clang++
CXXFLAGS  = -O2 -Wall -Wextra -Wno-cpp -std=c++17 -stdlib=libc++ -pthread -m64 -mmacosx-version-min=13.0

ROOTCFLAGS := $(shell root-config --cflags)
ROOTLIBS   := $(shell root-config --libs)

INCLUDES   = -I../common -I.
LDFLAGS    = -stdlib=libc++ -pthread -lm -ldl

SRCS       = CompareScan.cpp ../common/Points.cpp
OBJS       = $(SRCS:.cpp=.o)
TARGET     = compareScan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(ROOTLIBS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(ROOTCFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning up..."
	rm -f $(OBJS) $(TARGET)
	find . -name "*.dSYM" -type d -exec rm -rf {} +
