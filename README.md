# CompareScan

## Overview
**CompareScan v3.7** compares two 4-column point data files of the form:
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
1. A **3D color-coded scatter plot** showing dZ (in µm) as a function of (X, Y, dZ) using ROOT’s `TGraph2D`.  
2. A **1D histogram** of the dZ distribution (in µm), displayed side-by-side with the 3D plot.  
3. Additional histograms of dX and dY (in µm), stored in the ROOT output file but not displayed.  
4. A static PNG image of the final canvas automatically saved.

---

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

---

## Usage
```
./CompareScan file1.txt file2.txt [output.root]
```

**Arguments**
- `file1.txt` — reference data  
- `file2.txt` — comparison data  
- `[output.root]` — optional name for output file (default: `CompareScan.root`)

Example:
```
./CompareScan scan_A.txt scan_B.txt results.root
```

---

## Output
### ROOT file contents
```
hDX, hDY, hDZ       →  ΔX, ΔY, ΔZ distributions [µm]
CompareScanCanvas   →  full 3D + 1D canvas
```

### External files
- `results.root` → ROOT file with histograms and canvas  
- `results.png` → static image of the displayed canvas  

---

## Typical Console Output
```
====================================
 CompareScan v3.7 — Luciano Ristori
 Built: Oct 21 2025 11:42:16
====================================
Input file 1: scan_A.txt
Input file 2: scan_B.txt
Output file : results.root

Comparison (1024 points)
dX mean = 0.0123 µm σ = 5.6789
dY mean = −0.0345 µm σ = 5.4321
dZ mean = 12.3456 µm σ = 9.8765
dR mean = 13.2100 µm σ = 10.1020
zMin = −177.0000 µm, zMax = 76.0000 µm
Saved canvas image as results.png
Wrote results.root. Close the canvas to exit.
```

---

## Visualization
The ROOT canvas displays:
- **Left panel:** 3D color-coded scatter plot of dZ(X,Y)  
- **Right panel:** 1D histogram of dZ distribution  

dZ values are in µm, with automatic scaling.  
Histogram and color ranges are padded by ±10 % for clarity.

---

## Build Instructions
Dependencies:
- [ROOT Framework](https://root.cern)
- C++17 compiler (clang++ or g++)

### Compile manually
```
clang++ -std=c++17 -O2 CompareScan.cpp Points.cpp \
    $(root-config --cflags --libs) -lGui -o CompareScan
```

### Using the provided Makefile
```
make clean && make
```

---

## Notes
- Input and output coordinates are in mm; all differences are reported in µm.  
- The dZ color map auto-scales to data range but can be fixed via `SetMinimum()` / `SetMaximum()`.  
- The program runs interactively with ROOT GUI (`gROOT->SetBatch(false)`).  
- Histograms hDX, hDY, hDZ are written to the ROOT file but only hDZ is displayed.  
- Tested on macOS and Linux with ROOT 6.20 and later.

---

## Author
**Luciano Ristori**  
Fermilab — SIDET / CMS precision metrology and CMM analysis  
Version 3.7  ·  October 2025
