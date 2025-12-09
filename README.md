# CompareScan – Multi-File Surface Scan Comparison Tool  
Version: v3.14.0

CompareScan is a C++/ROOT program for comparing multiple CMM-style point-measurement files and analyzing Z-coordinate variation across repeated measurements of the same points.  
As of v3.14, it also computes and stores **X-range** and **Y-range** histograms in the ROOT output file.

-------------------------------------------------------------------------------
PURPOSE
-------------------------------------------------------------------------------
Given N input files, all containing the same points in the same order, the
program computes for each point i:

    Zmin(i)   =  min_k Z_k(i)
    Zmax(i)   =  max_k Z_k(i)
    RangeZ(i) =  Zmax(i) – Zmin(i)         [reported in µm]
    meanZ(i)  =  average Z across files    [reported in mm]
    sigmaZ(i) =  RMS Z spread              [reported in µm]

Starting with v3.14, it also computes:

    Xmin(i), Xmax(i), RangeX(i)            [reported in µm]
    Ymin(i), Ymax(i), RangeY(i)            [reported in µm]

The program produces:

  • **A 2D XY scatter plot (left pad)**  
        - Points drawn at (X,Y)  
        - Colored according to RangeZ(i) in µm (blue→yellow→red)  
        - Optional integer labels for RangeZ(i)  
        - Optional red marking of outliers (--outl)

  • **A 1D histogram (right pad)** of Z-range  
        - 1 µm bins  
        - Blue = non-outliers  
        - Red = outliers  
        - Custom stats box for non-outliers only

  • **An optional CSV file** containing per-point statistics

  • **A ROOT output file containing:**
        - Both displayed pads (scatter + Z-range histogram)  
        - *New in v3.14:* stored histograms of RangeX(i) and RangeY(i),
          written as `hRangeX` and `hRangeY`.  
          *(These are saved to the ROOT file but not drawn on the canvas.)*

-------------------------------------------------------------------------------
USAGE
-------------------------------------------------------------------------------
```
./CompareScan [--labels] [--stats [stats.csv]] [--outl N] \
              file1 file2 ... fileN [out.root]
```

Positional arguments:
  - **file1..fileN**  Input point files (N ≥ 2). Each line: `index X Y Z [label]`
  - **out.root**      Optional ROOT file name (default `CompareScan.root`)

Options:
  - `--labels`       Draw integer Z-range labels above scatter points  
  - `--stats`        Write per-point CSV to default name  
  - `--stats name`   Write CSV to specified filename  
  - `--outl N`       Mark points with Z-range > N µm as outliers:
                        • Excluded from global statistics  
                        • Colored red in scatter and histogram  
                        • Counted in CSV as isOut = 1  

-------------------------------------------------------------------------------
OUTPUT FILES
-------------------------------------------------------------------------------
1. **ROOT file** (default `CompareScan.root`)  
   Contains:
     - 2D XY scatter colored by Z-range  
     - Z-range histogram (good + outliers)  
     - **hRangeX**: per-point X-range histogram (µm) — *added in v3.14*  
     - **hRangeY**: per-point Y-range histogram (µm) — *added in v3.14*  

2. **PNG file**  
   Snapshot of the full canvas.

3. **CSV file** (if `--stats` is used)  
   Columns:  
       index,label,X_mm,Y_mm,meanZ_mm,sigmaZ_um,rangeZ_um,  
       Zmin_mm,Zmax_mm,isOut

   Precision rules:  
       X_mm, Y_mm, meanZ_mm, Zmin_mm, Zmax_mm → 3 decimals  
       sigmaZ_um, rangeZ_um                   → 1 decimal  

-------------------------------------------------------------------------------
GLOBAL STATISTICS (printed to terminal)
-------------------------------------------------------------------------------
Reported in microns (1 decimal):
   • Total points  
   • Outlier count & percentage (if `--outl`)  
   • Mean σ(Z), RMS σ(Z), Max σ(Z)  
   • Mean Range(Z), RMS Range(Z), Max Range(Z)  

Only **non-outlier** points are included in these statistics.

-------------------------------------------------------------------------------
SCATTER PLOT DETAILS
-------------------------------------------------------------------------------
   • Axes: X [mm], Y [mm]  
   • Colors represent Z-range using ROOT’s *kBird* palette  
   • Outliers forcibly drawn in red  
   • Optional µm labels via `--labels`  
   • Automatic legend when outlier threshold is used  

-------------------------------------------------------------------------------
HISTOGRAM DETAILS
-------------------------------------------------------------------------------
Z-range histogram:
   • 1 µm bins  
   • Blue = non-outliers  
   • Red = outliers  
   • Custom TPaveText statistics box (good points only)  

Additional histograms stored in ROOT file (v3.14):
   • **hRangeX** — distribution of X-range across all files (µm)  
   • **hRangeY** — distribution of Y-range across all files (µm)  

These X/Y histograms are **saved to ROOT output only** and are not displayed.

-------------------------------------------------------------------------------
CSV file (if --stats is used)
-------------------------------------------------------------------------------
Columns:
    index,label,X_mm,Y_mm,meanZ_mm,sigmaZ_um,rangeZ_um,
    Zmin_mm,Zmax_mm,rangeX_um,rangeY_um,isOut

Formatting:
    X_mm, Y_mm, meanZ_mm, Zmin_mm, Zmax_mm → 3 decimals  
    sigmaZ_um, rangeZ_um, rangeX_um, rangeY_um → 1 decimal  

-------------------------------------------------------------------------------
COMPILATION
-------------------------------------------------------------------------------
Requires ROOT 6.

Example compile command:
```
clang++ -std=c++17 CompareScan.cpp ../common/Points.cpp \
        `root-config --cflags --libs` -o CompareScan
```

-------------------------------------------------------------------------------
EXAMPLE COMMANDS
-------------------------------------------------------------------------------
Compare three files:
```
./CompareScan A.txt B.txt C.txt
```

Show labels:
```
./CompareScan --labels A.txt B.txt
```

Mark outliers at 50 µm:
```
./CompareScan --outl 50 A.txt B.txt C.txt
```

Write CSV statistics:
```
./CompareScan --stats results.csv A.txt B.txt C.txt
```

Full featured run:
```
./CompareScan --labels --stats --outl 30 scan1 scan2 scan3 out.root
```

-------------------------------------------------------------------------------
VERSION HISTORY
-------------------------------------------------------------------------------
v3.14.1
   - Added RangeX and RangeY Histograms and to per-point CSV statistics
v3.14.0  
   - Added computation of X-range and Y-range across input files  
   - Added ROOT histograms: `hRangeX` and `hRangeY`  
   - Histograms stored in output file (not displayed on canvas)  
   - No changes to UI/output behavior  

v3.13.0  
   - Multi-file Z-range comparison  
   - Outlier detection and coloring  
   - Split histogram (blue good, red outliers)  
   - Custom stats box (good only)  
   - Improved argument parser  
   - Consistent formatting for CSV  
   - Robust marker and label drawing  

-------------------------------------------------------------------------------
END OF README
-------------------------------------------------------------------------------
