# CompareScan

## Overview
**CompareScan** compares two 4-column point data files of the form:
```
iPoint   X   Y   Z
```
Each line corresponds to a single measurement point, identified by its index `iPoint` and its coordinates `(X, Y, Z)` in millimeters.

The program compares the two files **point by point (same index)** and computes the coordinate differences:

- **dX = X₂ − X₁**  
- **dY = Y₂ − Y₁**  
- **dZ = Z₂ − Z₁**  
- **dR = √(dX² + dY² + dZ²)**  

It then produces:
1. A **3D color-coded scatter plot** showing dZ (in µm) as a function of (X, Y, dZ), using ROOT’s `TGraph2D`.  
2. A **1D histogram** of dZ values (in µm) displayed next to the 3D map.  
3. Additional histograms of dX and dY (in µm), stored in the ROOT output file (but not displayed).  
4. Optionally, a saved copy of the displayed canvas (`CompareScanCanvas`) and a static PNG image.

## Features
- Reads both files as lists of 4-column points (`iPoint, X, Y, Z`).
- Compares corresponding points **by index**, not by position.
- Computes ΔX, ΔY, ΔZ, and ΔR for all points.
- Displays:
  - Left panel: 3D scatter plot color-coded by dZ [µm].
  - Right panel: Histogram of dZ distribution [µm].
- Saves results to a `.root` file:
  - Histograms: `hDX`, `hDY`, `hDZ`
  - Canvas: `CompareScanCanvas`
- Automatically exports a PNG image of the final canvas.
- dX, dY, dZ histograms have ±10% range expansion for better visibility.

## Input Format
Both input files must have the same structure and number of points.  
Each line should contain:
```
iPoint   X   Y   Z
```
Example:
```
1  100.123  50.456  400.789
2  100.223  50.556  400.692
3  100.325  50.654  400.734
...
```
Values are read as floating-point numbers in **millimeters**.

## Usage
```
./CompareScan file1.txt file2.txt [output.root]
```
- `file1.txt` — reference data  
- `file2.txt` — comparison data  
- `[output.root]` — optional name for output file (default: `CompareScan.root`)

Example:
```
./CompareScan scan_A.txt scan_B.txt results.root
```

## Output
### ROOT file contents
```
hDX   →  ΔX distribution [µm]
hDY   →  ΔY distribution [µm]
hDZ   →  ΔZ distribution [µm]
CompareScanCanvas →  full canvas (3D + histogram)
```
### Image export
- A PNG image is automatically saved as `CompareScan.png` (or matching the output file name).

## Build Instructions (macOS / Linux)
Dependencies:
- [ROOT Framework](https://root.cern/)
- Standard C++17 compiler (clang++ / g++)

To compile:
```
make clean && make
```
### Example Makefile
```
CXX      = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-c++17-extensions -Wno-c++14-compat -mmacosx-version-min=13.0
ROOTCONF = $(shell root-config --cflags --libs | sed 's/-std=c++14//g' | sed 's/-std=c++17//g')
SOURCES  = CompareScan.cpp Points.cpp
TARGET   = CompareScan

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) $(ROOTCONF) -lGui -std=c++17 -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
```

## Typical Console Output
```
Comparison (1024 points)
dX mean=0.0123 µm σ=5.6789
dY mean=-0.0345 µm σ=5.4321
dZ mean=12.3456 µm σ=9.8765
dR mean=13.2100 µm σ=10.1020
zMin=-177.0000 µm, zMax=76.0000 µm
Wrote results.root and saved canvas image.
```

## Notes
- Input and output coordinates are assumed to be in **mm**, differences are converted to **µm**.
- The dZ color map automatically scales to the actual range of dZ values.
- You can adjust visual style (marker size, color palette, etc.) directly in the source code.
- The code is compatible with ROOT 6.20+ and macOS / Linux.

## Author
**Luciano Ristori**  
Fermilab — SIDET / CMS precision metrology and CMM analysis  
October 2025

