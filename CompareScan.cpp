//
// File: CompareScan.cpp
//
// Description:
//   This program compares two sets of 3D point measurements of the same surface.
//   Each input file must contain one point per line in the format:
//
//       index, X, Y, Z
//
//   The program matches points by their index, computes the coordinate
//   differences (ΔX, ΔY, ΔZ, ΔR), and reports statistics of these differences.
//   Optionally, ROOT histograms can be created for further analysis.
//
// Usage:
//   ./CompareScan file1.csv file2.csv
//
//------------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdlib>
#include "Points.h"

using std::cout;
using std::endl;
using std::vector;
using std::string;

// Structure to hold difference statistics
struct DiffStats {
    double mean = 0.0;
    double sigma = 0.0;
    size_t n = 0;
};

//------------------------------------------------------------------------------
// Compute mean and RMS (sigma) for a vector of values
//------------------------------------------------------------------------------
DiffStats computeStats(const vector<double>& v) {
    DiffStats s;
    if (v.empty()) return s;

    double sum = 0.0, sum2 = 0.0;
    for (double x : v) {
        sum += x;
        sum2 += x * x;
    }

    s.n = v.size();
    s.mean = sum / s.n;
    s.sigma = std::sqrt(sum2 / s.n - s.mean * s.mean);
    return s;
}

//------------------------------------------------------------------------------
// Main program
//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " file1.csv file2.csv" << endl;
        return 1;
    }

    string file1 = argv[1];
    string file2 = argv[2];

    cout << "\nCompareScan: comparing measurements" << endl;
    cout << "  File 1: " << file1 << endl;
    cout << "  File 2: " << file2 << endl;

    // Read both files
    int nCols = 4;
    vector<Point> A = readPoints(file1, nCols);
    vector<Point> B = readPoints(file2, nCols);

    if (A.empty() || B.empty()) {
        std::cerr << "Error: one or both files are empty or invalid." << endl;
        return 1;
    }

    if (A.size() != B.size()) {
        std::cerr << "Warning: number of points differ ("
                  << A.size() << " vs " << B.size() << "). "
                  << "Will compare up to the smaller size." << endl;
    }

    size_t n = std::min(A.size(), B.size());

    vector<double> dX, dY, dZ, dR;
    dX.reserve(n); dY.reserve(n); dZ.reserve(n); dR.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        double dx = B[i].coords[1] - A[i].coords[1];
        double dy = B[i].coords[2] - A[i].coords[2];
        double dz = B[i].coords[3] - A[i].coords[3];
        double dr = std::sqrt(dx*dx + dy*dy + dz*dz);

        dX.push_back(dx);
        dY.push_back(dy);
        dZ.push_back(dz);
        dR.push_back(dr);
    }

    DiffStats sx = computeStats(dX);
    DiffStats sy = computeStats(dY);
    DiffStats sz = computeStats(dZ);
    DiffStats sr = computeStats(dR);

    cout << std::fixed << std::setprecision(4);
    cout << "\nComparison summary (" << n << " matched points):\n";
    cout << "--------------------------------------------------\n";
    cout << "ΔX: mean = " << sx.mean*1000 << " µm,  σ = " << sx.sigma*1000 << " µm\n";
    cout << "ΔY: mean = " << sy.mean*1000 << " µm,  σ = " << sy.sigma*1000 << " µm\n";
    cout << "ΔZ: mean = " << sz.mean*1000 << " µm,  σ = " << sz.sigma*1000 << " µm\n";
    cout << "ΔR: mean = " << sr.mean*1000 << " µm,  σ = " << sr.sigma*1000 << " µm\n";
    cout << "--------------------------------------------------\n";

    return 0;
}
