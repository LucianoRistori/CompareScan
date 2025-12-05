# CompareScan – Multi-File Surface Scan Comparison Tool  
Version: v3.13.0

CompareScan is a C++/ROOT program for comparing multiple CMM-style point-measurement files and analyzing Z-coordinate variation across repeated measurements of the same points.

-------------------------------------------------------------------------------
PURPOSE
-------------------------------------------------------------------------------
Given N input files, all containing the same points in the same order, the
program computes for each point i:

    Zmin(i)  =  min_k Z_k(i)
    Zmax(i)  =  max_k Z_k(i)
    Range(i) =  Zmax(i) – Zmin(i)           [reported in µm]
    meanZ(i) =  average Z over all files    [reported in mm]
    sigmaZ(i)=  RMS Z spread                [reported in µm]

It produces:

  • A 2D XY scatter plot (left pad):
        - Points drawn at (X,Y)
        - Colored according to Range(i) in µm (blue→yellow→red)
        - Optional integer labels for Range(i) above each point
        - Optional red marking of outliers via --outl

  • A 1D histogram (right pad):
        - 1 µm per bin
        - Non-outliers in blue
        - Outliers in red
        - A custom statistics box showing only non-outlier statistics

  • An optional CSV file containing per-point statistics

  • A ROOT output file containing both pads

-------------------------------------------------------------------------------
USAGE
-------------------------------------------------------------------------------
    ./CompareScan [--labels] [--stats [stats.csv]] [--outl N] \
                  file1 file2 ... fileN [out.root]

Positional arguments:
    file1..fileN   Input point files (N ≥ 2). Each line: index X Y Z [label]
    out.root       Optional ROOT output file (default: CompareScan.root)

Options:
    --labels       Draw integer Range(i) labels above scatter points
    --stats        Write per-point statistics CSV
    --stats name   Write CSV to the given filename
    --outl N       Mark points with Range(i) > N µm as outliers:
                       • Excluded from global statistics
                       • Excluded from histogram statistics
                       • Colored red in scatter + histogram
                       • Marked isOut=1 in CSV

-------------------------------------------------------------------------------
OUTPUT FILES
-------------------------------------------------------------------------------
1. ROOT file  (default: CompareScan.root)
   Contains both pads (scatter + histogram).

2. PNG file  
   Snapshot of the full canvas (same basename as ROOT file).

3. CSV file (if --stats is used)
   Columns:
       index,label,X_mm,Y_mm,meanZ_mm,sigmaZ_um,rangeZ_um,
       Zmin_mm,Zmax_mm,isOut

   Formatting:
       X_mm, Y_mm, meanZ_mm, Zmin_mm, Zmax_mm → 3 decimals
       sigmaZ_um, rangeZ_um                   → 1 decimal

-------------------------------------------------------------------------------
GLOBAL STATISTICS (printed to terminal)
-------------------------------------------------------------------------------
Reported in microns with one decimal:
   • Total points
   • Outlier count and percentage (if --outl is used)
   • Mean σ(Z), RMS σ(Z), Max σ(Z)
   • Mean Range(Z), RMS Range(Z), Max Range(Z)

Outliers are excluded from all these calculations.

-------------------------------------------------------------------------------
SCATTER PLOT DETAILS
-------------------------------------------------------------------------------
   • Axes: X [mm], Y [mm]
   • Point color encodes Range(i) using ROOT’s kBird palette
   • Outliers forcibly colored red
   • Optional labels via --labels
   • Legend showing outlier threshold appears automatically

-------------------------------------------------------------------------------
HISTOGRAM DETAILS
-------------------------------------------------------------------------------
   • 1 μm per bin, integer bin edges
   • Blue bars = non-outliers
   • Red bars = outliers
   • Custom TPaveText statistics box for non-outliers only:
         Entries, Mean, StdDev (all in µm)
   • If no non-outliers exist, histogram will contain only red bars

-------------------------------------------------------------------------------
CSV FORMAT (if --stats is used)
-------------------------------------------------------------------------------
Row fields:
   index       sequential point number
   label       point label from input file
   X_mm        3 decimals
   Y_mm        3 decimals
   meanZ_mm    3 decimals
   sigmaZ_um   1 decimal
   rangeZ_um   1 decimal
   Zmin_mm     3 decimals
   Zmax_mm     3 decimals
   isOut       1 if Range > outlier threshold, else 0

-------------------------------------------------------------------------------
COMPILATION
-------------------------------------------------------------------------------
Requires ROOT 6.

Typical compile command:
    clang++ -std=c++17 CompareScan.cpp ../common/Points.cpp \
            `root-config --cflags --libs` -o CompareScan

-------------------------------------------------------------------------------
EXAMPLE COMMANDS
-------------------------------------------------------------------------------
Compare three files:
    ./CompareScan A.txt B.txt C.txt

Show labels:
    ./CompareScan --labels A.txt B.txt

Mark outliers at 50 µm:
    ./CompareScan --outl 50 A.txt B.txt C.txt

Write CSV statistics:
    ./CompareScan --stats results.csv A.txt B.txt C.txt

Full featured run:
    ./CompareScan --labels --stats --outl 30 scan1 scan2 scan3 out.root

-------------------------------------------------------------------------------
VERSION HISTORY
-------------------------------------------------------------------------------
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
