//==============================================================================
// File: CompareScan.cpp
// Version: 3.10
//
// Purpose:
//   Compare N point files (same points, same ordering) and, for each point,
//   compute the Z range across files:
//
//       Zmin(i) = min_k Z_k(i)
//       Zmax(i) = max_k Z_k(i)
//       Range(i) = Zmax(i) - Zmin(i)
//
//   Then:
//     • Plot a 2D XY scatter where each point is colored by Range(i) in µm
//     • Draw a 1D histogram of Range(i) in µm
//     • Optionally label each point on the scatter with its Range(i) [µm]
//     • Optionally write a CSV with per-point statistics across files.
//
// Usage:
//   ./CompareScan [--labels] [--stats [stats.csv]] file1 file2 ... fileN [out.root]
//
//   - file1..fileN : point files to compare (N ≥ 2), each line: index X Y Z ...
//   - out.root     : optional ROOT output file (default: CompareScan.root)
//   - --labels     : draw integer µm range labels above each point
//   - --stats      : enable statistics CSV output
//       * If followed by a filename, use that (e.g. --stats myStats.csv)
//       * Otherwise use: <root_output_basename>_stats.csv
//
// Output:
//   - ROOT file (out.root) with:
//       * XY scatter canvas (left pad)
//       * Range histogram (right pad)
//   - PNG snapshot of the canvas.
//   - If --stats is used, a CSV with per-point statistics:
//       index, X_mm, Y_mm, meanZ_mm, sigmaZ_um, rangeZ_um, Zmin_mm, Zmax_mm
//
// Notes:
//   - X,Y are taken from the first file.
//   - All files must have at least as many points as the shortest one; extra
//     points in longer files are ignored (a warning is printed).
//==============================================================================

#define COMPARESCAN_VERSION "v3.10"

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>

#include "Points.h"

// ROOT includes
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TMarker.h"
#include "TLatex.h"
#include "TROOT.h"
#include "TColor.h"

using namespace std;

//------------------------------------------------------------------------------
// Simple statistics container (used for global summaries)
//------------------------------------------------------------------------------
struct DiffStats {
    double mean  = 0.0;
    double sigma = 0.0;
    size_t n     = 0;
};

DiffStats computeStats(const vector<double>& v) {
    DiffStats s;
    if (v.empty()) return s;
    double sum = 0.0, sum2 = 0.0;
    for (double x : v) {
        sum  += x;
        sum2 += x*x;
    }
    s.n    = v.size();
    s.mean = sum / s.n;
    s.sigma = std::sqrt(sum2 / s.n - s.mean * s.mean);
    return s;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    //--------------------------------------------------------------------------
    // Parse command-line arguments
    //--------------------------------------------------------------------------
    bool drawLabels        = false;
    bool doStats           = false;
    bool statsCsvExplicit  = false;
    string statsCsvName;
    string outRoot = "CompareScan.root";

    vector<string> positional;

    if (argc < 3) {
        cerr << "Usage:\n  " << argv[0]
             << " [--labels] [--stats [stats.csv]] file1 file2 ... fileN [out.root]\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "--labels") {
            drawLabels = true;
            continue;
        }

        if (arg == "--stats") {
            doStats = true;
            // Optional CSV filename following --stats
            if (i + 1 < argc) {
                string next = argv[i+1];
                if (next.rfind("--", 0) != 0) { // does not start with "--"
                    statsCsvName    = next;
                    statsCsvExplicit = true;
                    ++i; // consume filename
                }
            }
            continue;
        }

        // Otherwise, positional argument (file or out.root)
        positional.push_back(arg);
    }

    if (positional.size() < 2) {
        cerr << "Error: need at least two input files.\n";
        cerr << "Usage:\n  " << argv[0]
             << " [--labels] [--stats [stats.csv]] file1 file2 ... fileN [out.root]\n";
        return 1;
    }

    // Last positional argument may be a ROOT filename
    {
        const string& last = positional.back();
        if (last.size() >= 5 && last.substr(last.size()-5) == ".root") {
            outRoot = last;
            positional.pop_back();
        }
    }

    if (positional.size() < 2) {
        cerr << "Error: after removing ROOT output, fewer than two input files remain.\n";
        return 1;
    }

    // Determine default stats CSV name if needed
    if (doStats && !statsCsvExplicit) {
        string base = outRoot;
        size_t dotPos = base.find_last_of('.');
        if (dotPos != string::npos) {
            base = base.substr(0, dotPos);
        }
        statsCsvName = base + "_stats.csv";
    }

    //--------------------------------------------------------------------------
    // Print basic info
    //--------------------------------------------------------------------------
    cout << "\n====================================\n";
    cout << " CompareScan " << COMPARESCAN_VERSION << " — multi-file Z range\n";
    cout << " Built: " << __DATE__ << " " << __TIME__ << "\n";
    cout << "====================================\n";

    cout << "Number of input files: " << positional.size() << "\n";
    for (size_t i = 0; i < positional.size(); ++i) {
        cout << "  File " << (i+1) << ": " << positional[i] << "\n";
    }
    cout << "Output ROOT file : " << outRoot << "\n";
    if (doStats) {
        cout << "Stats CSV output : " << statsCsvName << "\n";
    }
    cout << "Labels on scatter: " << (drawLabels ? "ON" : "OFF") << "\n";

    //--------------------------------------------------------------------------
    // Read all point files
    //--------------------------------------------------------------------------
    int nFiles = static_cast<int>(positional.size());
    vector< vector<Point> > allPoints(nFiles);

    for (int k = 0; k < nFiles; ++k) {
        allPoints[k] = readPoints(positional[k], 3);
        if (allPoints[k].empty()) {
            cerr << "Error: file " << positional[k] << " produced no points.\n";
            return 1;
        }
    }

    // Check point counts (use minimum across files)
    size_t nPoints = allPoints[0].size();
    bool sizeMismatch = false;
    for (int k = 1; k < nFiles; ++k) {
        if (allPoints[k].size() != nPoints) {
            sizeMismatch = true;
            nPoints = std::min(nPoints, allPoints[k].size());
        }
    }
    if (sizeMismatch) {
        cerr << "Warning: input files have different numbers of points.\n"
             << "         Using first " << nPoints << " points common to all files.\n";
    }

    if (nPoints == 0) {
        cerr << "Error: no common points to compare.\n";
        return 1;
    }

    //--------------------------------------------------------------------------
    // Per-point statistics across files
    //--------------------------------------------------------------------------
    vector<double> X(nPoints), Y(nPoints);
    vector<double> meanZ_mm(nPoints), sigmaZ_um(nPoints), rangeZ_um(nPoints);
    vector<double> Zmin_mm(nPoints), Zmax_mm(nPoints);

    for (size_t i = 0; i < nPoints; ++i) {
        // Use coordinates from first file
        X[i] = allPoints[0][i].coords[0];
        Y[i] = allPoints[0][i].coords[1];

        double zmin =  1e99;
        double zmax = -1e99;
        double sumZ  = 0.0;
        double sumZ2 = 0.0;

        for (int k = 0; k < nFiles; ++k) {
            double z = allPoints[k][i].coords[2];
            if (z < zmin) zmin = z;
            if (z > zmax) zmax = z;
            sumZ  += z;
            sumZ2 += z*z;
        }

        double meanZ = sumZ / nFiles;
        double varZ  = 0.0;
        if (nFiles > 1) {
            varZ = sumZ2 / nFiles - meanZ * meanZ;
            if (varZ < 0) varZ = 0; // numerical safety
        }
        double sigma_mm = (nFiles > 1) ? std::sqrt(varZ) : 0.0;
        double range_mm = zmax - zmin;

        meanZ_mm[i]   = meanZ;
        sigmaZ_um[i]  = sigma_mm * 1000.0;
        rangeZ_um[i]  = range_mm * 1000.0;
        Zmin_mm[i]    = zmin;
        Zmax_mm[i]    = zmax;
    }

    // Global summaries
    DiffStats statsSigmaUm = computeStats(sigmaZ_um);
    DiffStats statsRangeUm = computeStats(rangeZ_um);

    double maxSigmaUm = 0.0;
    double maxRangeUm = 0.0;
    for (size_t i = 0; i < nPoints; ++i) {
        if (sigmaZ_um[i] > maxSigmaUm) maxSigmaUm = sigmaZ_um[i];
        if (rangeZ_um[i] > maxRangeUm) maxRangeUm = rangeZ_um[i];
    }

    cout << "\nGlobal Z statistics across all points (" << nPoints
         << " points, " << nFiles << " files):\n";
    cout << "  Mean σ(Z)     = " << statsSigmaUm.mean << " µm\n";
    cout << "  RMS spread σ(Z) across points = " << statsSigmaUm.sigma << " µm\n";
    cout << "  Max  σ(Z)     = " << maxSigmaUm << " µm\n";
    cout << "  Mean Range(Z) = " << statsRangeUm.mean << " µm\n";
    cout << "  RMS spread Range(Z) across points = " << statsRangeUm.sigma << " µm\n";
    cout << "  Max  Range(Z) = " << maxRangeUm << " µm\n";

    //--------------------------------------------------------------------------
    // ROOT startup and histogram of Range(Z)
    //--------------------------------------------------------------------------
    TApplication app("app", &argc, argv);
    gROOT->SetBatch(false);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(64);

    TFile outF(outRoot.c_str(), "RECREATE");

    auto findRange = [&](const vector<double>& v, double& lo, double& hi) {
        auto [it1, it2] = minmax_element(v.begin(), v.end());
        lo = *it1;
        hi = *it2;
        double r = hi - lo;
        if (r <= 0) r = std::fabs(hi) * 0.1;
        double m = 0.10 * r;
        lo -= m;
        hi += m;
    };

    double rMin, rMax;
    findRange(rangeZ_um, rMin, rMax);

    auto hRange = new TH1D("hRangeZ",
                           "Z range across files;Range [#mum];Counts",
                           100, rMin, rMax);

    for (size_t i = 0; i < nPoints; ++i) {
        hRange->Fill(rangeZ_um[i]);
    }
    hRange->Write();

    //--------------------------------------------------------------------------
    // Canvas: left = 2D scatter of Range(Z), right = histogram
    //--------------------------------------------------------------------------
    TCanvas* c = new TCanvas("cFlat", "CompareScan: Z range map + histogram",
                             1200, 600);
    c->Divide(2, 1, 0.001, 0.001);

    //==========================================================================
    // LEFT PAD — 2D scatter, colored by Range(Z), optional integer labels
    //==========================================================================
    c->cd(1);
    gPad->SetRightMargin(0.05);
    gPad->SetLeftMargin(0.12);
    gPad->SetBottomMargin(0.12);
    gPad->SetTopMargin(0.10);

    gPad->SetFillColor(kWhite);
    gPad->SetFrameFillColor(kWhite);
    gPad->SetFrameFillStyle(0);

    // Compute XY ranges with small margins
    double xminA =  1e99, xmaxA = -1e99;
    double yminA =  1e99, ymaxA = -1e99;
    for (size_t i = 0; i < nPoints; ++i) {
        if (X[i] < xminA) xminA = X[i];
        if (X[i] > xmaxA) xmaxA = X[i];
        if (Y[i] < yminA) yminA = Y[i];
        if (Y[i] > ymaxA) ymaxA = Y[i];
    }
    double dxA = xmaxA - xminA;
    double dyA = ymaxA - yminA;
    if (dxA <= 0) dxA = 1.0;
    if (dyA <= 0) dyA = 1.0;
    xminA -= 0.05 * dxA;  xmaxA += 0.05 * dxA;
    yminA -= 0.05 * dyA;  ymaxA += 0.05 * dyA;

    // Range(Z) for color mapping
    double ZminRange = *min_element(rangeZ_um.begin(), rangeZ_um.end());
    double ZmaxRange = *max_element(rangeZ_um.begin(), rangeZ_um.end());
    double ZrangeRange = ZmaxRange - ZminRange;
    if (ZrangeRange <= 0) ZrangeRange = 1.0;

    int nColors = gStyle->GetNumberOfColors();
    if (nColors < 2) nColors = 64;

    // Frame: axes + grid
    TH2D* frameXY = new TH2D("frame_xy", ";X [mm];Y [mm]",
                             100, xminA, xmaxA,
                             100, yminA, ymaxA);
    frameXY->SetStats(false);
    frameXY->SetFillStyle(0);
    frameXY->Draw("AXIS");
    frameXY->Draw("AXIG SAME");  // grid

    // Draw colored markers (+ optional labels)
    double ySpan = ymaxA - yminA;
    double yOffset = 0.02 * ySpan;  // vertical offset for labels

    for (size_t i = 0; i < nPoints; ++i) {
        double xx = X[i];
        double yy = Y[i];
        double zz = rangeZ_um[i]; // in µm

        // Normalize to [0,1]
        double norm = (zz - ZminRange) / ZrangeRange;
        if (norm < 0.0) norm = 0.0;
        if (norm > 1.0) norm = 1.0;

        int ci = gStyle->GetColorPalette(int(norm * (nColors - 1)));

        // Colored marker
        TMarker* m = new TMarker(xx, yy, 20);
        m->SetMarkerColor(ci);
        m->SetMarkerSize(1.8);
        m->Draw("SAME");

        // Optional label: integer µm range
        if (drawLabels) {
            int zu = static_cast<int>(std::llround(zz));
            std::string label = std::to_string(zu);

            TLatex* t = new TLatex(xx, yy + yOffset, label.c_str());
            t->SetTextSize(0.025);
            t->SetTextColor(kBlack);  // strong contrast
            t->SetTextFont(42);
            t->SetTextAlign(21);      // centered horizontally
            t->Draw("SAME");
        }
    }

    gPad->Modified();
    gPad->Update();

    //==========================================================================
    // RIGHT PAD — histogram of Z range
    //==========================================================================
    c->cd(2);

    gPad->SetLeftMargin(0.12);
    gPad->SetRightMargin(0.05);
    gPad->SetTopMargin(0.10);
    gPad->SetBottomMargin(0.12);

    hRange->SetFillColor(kAzure+7);
    hRange->SetLineColor(kBlue+3);
    hRange->SetLineWidth(2);
    hRange->Draw("HIST");

    gPad->Modified();
    gPad->Update();

    //--------------------------------------------------------------------------
    // Stats CSV output (if requested)
    //--------------------------------------------------------------------------
    if (doStats) {
		ofstream csv(statsCsvName.c_str());
		if (!csv) {
			cerr << "Error: could not open stats CSV file: " << statsCsvName << "\n";
		} else {
			csv << "index,label,X_mm,Y_mm,meanZ_mm,sigmaZ_um,rangeZ_um,Zmin_mm,Zmax_mm\n";
			csv << std::setprecision(10);

			for (size_t i = 0; i < nPoints; ++i) {
				csv << (i+1) << ","
					<< allPoints[0][i].label << ","
					<< X[i] << ","
					<< Y[i] << ","
					<< meanZ_mm[i] << ","
					<< sigmaZ_um[i] << ","
					<< rangeZ_um[i] << ","
					<< Zmin_mm[i] << ","
					<< Zmax_mm[i] << "\n";
			}

			csv.close();
			cout << "\nWrote per-point statistics to " << statsCsvName << "\n";
		}
	}


    //--------------------------------------------------------------------------
    // Save canvas and finish
    //--------------------------------------------------------------------------
    string pngOut = outRoot.substr(0, outRoot.find_last_of(".")) + ".png";
    c->SaveAs(pngOut.c_str());
    cout << "Saved canvas image as " << pngOut << "\n";

    c->Write("CompareScanCanvas");

    cout << "\nWrote ROOT output file: " << outRoot << "\n";
    cout << "Close the canvas to exit.\n";

    app.Run();
    outF.Close();
    return 0;
}
