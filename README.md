# CompareScan v2.1.0

**Author:** Luciano Ristori  
**Date:** October 2025  

---

## Overview

`CompareScan` compares two sets of CMM surface measurements to identify differences between scans.  
It computes point-to-point residuals (ΔZ, ΔR), generates statistical summaries, and produces ROOT-based visualizations (histograms, scatter plots, 2D color maps).

This version introduces integration with the shared `common` module, which now provides unified point handling and optional label support.

---

## Features

- Reads two input files with 3D points (`X Y Z` or `label X Y Z`)
- Automatically aligns and compares corresponding points
- Computes mean and sigma of ΔZ and other residuals
- Produces ROOT histograms and 2D visual plots
- Compatible with labeled point data via `common v1.2.1`

---

## Dependencies

- **common module:** v1.2.1  
  Provides the `Points` structure and `readPoints()` function with label and CSV support  
  (Tag: `v1.2.1` in the `common` repository)

- **ROOT Framework:** v6.30+  
  Required for histogramming, plotting, and visualization  
  [https://root.cern](https://root.cern)

- **C++17 compiler (clang++)**

---

## Build Instructions

```bash
make clean
make
