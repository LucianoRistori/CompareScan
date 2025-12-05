//==============================================================================
// File: CompareScan.cpp
// Version: 3.13
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
//     • Optionally define outliers by a range threshold and:
//         - Exclude them from global statistics
//         - Color them red in the scatter
//         - Color their histogram bars red
//         - Flag them in the CSV
//
// Usage:
//   ./CompareScan [--labels] [--stats [stats.csv]] [--outl N] \
//                 file1 file2 ... fileN [out.root]
//
//   - file1..fileN : point files to compare (N ≥ 2), each line: index X Y Z ...
//   - out.root     : optional ROOT output file (default: CompareScan.root)
//   - --labels     : draw integer µm range labels above each point
//   - --stats      : enable statistics CSV output
//       * If followed by a filename, use that (e.g. --stats myStats.csv)
//       * Otherwise use: <root_output_basename>_stats.csv
//   - --outl N     : define outliers as points with Range(i) > N µm
//       * Outliers are:
//           · Excluded from global statistics
//           · Shown in red in the scatter and histogram
//           · Flagged in the CSV (isOut = 1)
//
// Output:
//   - ROOT file (out.root) with:
//       * XY scatter canvas (left pad)
//       * Range histogram (right pad, blue=good, red=outliers)
//   - PNG snapshot of the canvas.
//   - If --stats is used, a CSV with per-point statistics:
//       index, label, X_mm, Y_mm, meanZ_mm, sigmaZ_um, rangeZ_um,
//       Zmin_mm, Zmax_mm, isOut
//
// Notes:
//   - X,Y are taken from the first file.
//   - All files must have at least as many points as the shortest one; extra
//     points in longer files are ignored (a warning is printed).
//==============================================================================

#define COMPARESCAN_VERSION "v3.13"

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <sstream>

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
#include "TPaveText.h"

using namespace std;

//------------------------------------------------------------------------------
// Simple statistics container
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
    s.n     = v.size();
    s.mean  = sum / s.n;
    s.sigma = std::sqrt(sum2 / s.n - s.mean * s.mean);
    return s;
}

//==============================================================================
// MAIN
//==============================================================================
int main(int argc, char* argv[])
{
    //--------------------------------------------------------------------------
    // Parse arguments
    //--------------------------------------------------------------------------
    bool drawLabels        = false;
    bool doStats           = false;
    bool statsCsvExplicit  = false;
    bool useOutlierCut     = false;
    double outlierThresholdUm = 0.0;

    string statsCsvName;
    string outRoot = "CompareScan.root";
    vector<string> positional;

    if (argc < 3) {
        cerr << "Usage:\n  " << argv[0]
             << " [--labels] [--stats [stats.csv]] [--outl N]"
             << " file1 file2 ... fileN [out.root]\n";
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
            if (i + 1 < argc) {
                string next = argv[i+1];
                if (next.rfind("--", 0) != 0) {
                    statsCsvName    = next;
                    statsCsvExplicit = true;
                    ++i;
                }
            }
            continue;
        }

        if (arg == "--outl") {
            if (i + 1 >= argc) {
                cerr << "Error: --outl requires a numeric threshold.\n";
                return 1;
            }
            string next = argv[++i];
            try {
                outlierThresholdUm = std::stod(next);
            } catch (...) {
                cerr << "Error: invalid outlier threshold.\n";
                return 1;
            }
            useOutlierCut = true;
            continue;
        }

        // Positional argument (file or out.root)
        positional.push_back(arg);
    }

    if (positional.size() < 2) {
        cerr << "Error: need >= 2 input files.\n";
        return 1;
    }

    // Last positional arg may be a ROOT filename
    {
        const string& last = positional.back();
        if (last.size() > 5 && last.substr(last.size() - 5) == ".root") {
            outRoot = last;
            positional.pop_back();
        }
    }

    if (positional.size() < 2) {
        cerr << "Error: fewer than 2 input files remain.\n";
        return 1;
    }

    // Default stats filename if needed
    if (doStats && !statsCsvExplicit) {
        string base = outRoot.substr(0, outRoot.find_last_of('.'));
        statsCsvName = base + "_stats.csv";
    }

    //--------------------------------------------------------------------------
    // Print info summary
    //--------------------------------------------------------------------------
    cout << "\n====================================\n";
    cout << " CompareScan " << COMPARESCAN_VERSION << "\n";
    cout << " Built: " << __DATE__ << " " << __TIME__ << "\n";
    cout << "====================================\n";

    cout << "Input files:\n";
    for (size_t i = 0; i < positional.size(); ++i)
        cout << "  [" << (i+1) << "] " << positional[i] << "\n";

    cout << "Output ROOT : " << outRoot << "\n";
    cout << "Labels      : " << (drawLabels ? "ON" : "OFF") << "\n";
    if (doStats)
        cout << "Stats CSV   : " << statsCsvName << "\n";
    if (useOutlierCut)
        cout << "Outlier cut : > " << outlierThresholdUm << " µm\n";

    //--------------------------------------------------------------------------
    // Read files
    //--------------------------------------------------------------------------
    int nFiles = static_cast<int>(positional.size());
    vector<vector<Point>> allPoints(nFiles);

    for (int k = 0; k < nFiles; ++k) {
        allPoints[k] = readPoints(positional[k], 3);
        if (allPoints[k].empty()) {
            cerr << "Error: file " << positional[k] << " yielded no points.\n";
            return 1;
        }
    }

    // Find common number of points
    size_t nPoints = allPoints[0].size();
    bool sizeMismatch = false;

    for (int k = 1; k < nFiles; ++k) {
        if (allPoints[k].size() != nPoints) {
            sizeMismatch = true;
            nPoints = std::min(nPoints, allPoints[k].size());
        }
    }

    if (sizeMismatch) {
        cerr << "Warning: different file sizes; using first "
             << nPoints << " points.\n";
    }

    if (nPoints == 0) {
        cerr << "Error: no common points.\n";
        return 1;
    }

    //--------------------------------------------------------------------------
    // Compute per-point statistics
    //--------------------------------------------------------------------------
    vector<double> X(nPoints), Y(nPoints);
    vector<double> meanZ_mm(nPoints), sigmaZ_um(nPoints), rangeZ_um(nPoints);
    vector<double> Zmin_mm(nPoints), Zmax_mm(nPoints);
    vector<int>    isOut(nPoints, 0);

    for (size_t i = 0; i < nPoints; ++i) {
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
        double varZ  = (nFiles > 1) ? (sumZ2 / nFiles - meanZ * meanZ) : 0.0;
        if (varZ < 0) varZ = 0.0;

        meanZ_mm[i]  = meanZ;
        sigmaZ_um[i] = std::sqrt(varZ) * 1000.0;
        rangeZ_um[i] = (zmax - zmin) * 1000.0;
        Zmin_mm[i]   = zmin;
        Zmax_mm[i]   = zmax;
    }

    //--------------------------------------------------------------------------
    // Outliers
    //--------------------------------------------------------------------------
    size_t nOutliers = 0;
    if (useOutlierCut) {
        for (size_t i = 0; i < nPoints; ++i) {
            if (rangeZ_um[i] > outlierThresholdUm) {
                isOut[i] = 1;
                nOutliers++;
            }
        }
    }
    size_t nGood = nPoints - nOutliers;

    // Build filtered arrays for global stats (non-outliers only if cut used)
    vector<double> sigmaUsed, rangeUsed;
    if (useOutlierCut) {
        sigmaUsed.reserve(nGood);
        rangeUsed.reserve(nGood);
        for (size_t i = 0; i < nPoints; ++i) {
            if (!isOut[i]) {
                sigmaUsed.push_back(sigmaZ_um[i]);
                rangeUsed.push_back(rangeZ_um[i]);
            }
        }
    } else {
        sigmaUsed = sigmaZ_um;
        rangeUsed = rangeZ_um;
    }

    //--------------------------------------------------------------------------
    // Global statistics (microns with 1 decimal)
    //--------------------------------------------------------------------------
    cout << std::fixed << std::setprecision(1);
    cout << "\nGlobal Z statistics:\n";
    cout << "  Total points      = " << nPoints << "\n";
    if (useOutlierCut) {
        double frac = (nPoints > 0) ? (100.0 * nOutliers / nPoints) : 0.0;
        cout << "  Outliers (> " << outlierThresholdUm << " µm) = "
             << nOutliers << " (" << frac << "%)\n";
        cout << "  Used (non-outliers) = " << nGood << "\n";
    }

    if (rangeUsed.empty()) {
        cout << "  WARNING: No non-outlier points — statistics empty.\n";
    } else {
        DiffStats sSigma = computeStats(sigmaUsed);
        DiffStats sRange = computeStats(rangeUsed);

        double maxSig = *std::max_element(sigmaUsed.begin(), sigmaUsed.end());
        double maxRan = *std::max_element(rangeUsed.begin(), rangeUsed.end());

        cout << "  Mean σ(Z)      = " << sSigma.mean  << " µm\n";
        cout << "  RMS σ(Z)       = " << sSigma.sigma << " µm\n";
        cout << "  Max σ(Z)       = " << maxSig       << " µm\n";
        cout << "  Mean Range(Z)  = " << sRange.mean  << " µm\n";
        cout << "  RMS Range(Z)   = " << sRange.sigma << " µm\n";
        cout << "  Max Range(Z)   = " << maxRan       << " µm\n";
    }

    //--------------------------------------------------------------------------
    // ROOT + output file
    //--------------------------------------------------------------------------
    TApplication app("app", &argc, argv);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(64);
    TFile outF(outRoot.c_str(), "RECREATE");

    //--------------------------------------------------------------------------
    // Canvas: scatter + histogram
    //--------------------------------------------------------------------------
    TCanvas* c = new TCanvas("cFlat",
                             "CompareScan: Z range map + histogram",
                             1200, 600);
    c->Divide(2, 1, 0.001, 0.001);

    //==========================================================================
    // LEFT — scatter plot (XY colored by rangeZ_um)
    //==========================================================================
    c->cd(1);
    gPad->SetRightMargin(0.05);
    gPad->SetLeftMargin(0.12);
    gPad->SetBottomMargin(0.12);
    gPad->SetTopMargin(0.10);

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

    double ZminRange = *std::min_element(rangeZ_um.begin(), rangeZ_um.end());
    double ZmaxRange = *std::max_element(rangeZ_um.begin(), rangeZ_um.end());
    double ZrangeRange = ZmaxRange - ZminRange;
    if (ZrangeRange <= 0) ZrangeRange = 1.0;

    int nColors = gStyle->GetNumberOfColors();

    TH2D* frameXY = new TH2D("frame_xy", ";X [mm];Y [mm]",
                             100, xminA, xmaxA,
                             100, yminA, ymaxA);
    frameXY->SetStats(false);
    frameXY->SetFillStyle(0);
    frameXY->Draw("AXIS");
    frameXY->Draw("AXIG SAME");

    double ySpan   = ymaxA - yminA;
    double yOffset = 0.02 * ySpan;

    // Draw markers
    for (size_t i = 0; i < nPoints; ++i) {
        double xx = X[i];
        double yy = Y[i];
        double zz = rangeZ_um[i];

        double norm = (zz - ZminRange) / ZrangeRange;
        if (norm < 0.0) norm = 0.0;
        if (norm > 1.0) norm = 1.0;

        int ci = gStyle->GetColorPalette(int(norm * (nColors - 1)));

        if (useOutlierCut && isOut[i]) {
            ci = kRed+1;   // outliers red
        }

        TMarker* m = new TMarker(xx, yy, 20);
        m->SetMarkerColor(ci);
        m->SetMarkerSize(1.8);
        m->Draw("SAME");

        if (drawLabels) {
            int zu = static_cast<int>(std::llround(zz));
            TLatex* t = new TLatex(xx, yy + yOffset,
                                   std::to_string(zu).c_str());
            t->SetTextColor(kBlack);      // always black
            t->SetTextSize(0.025);
            t->SetTextAlign(21);
            t->Draw("SAME");
        }
    }

    // Outlier legend (top-right)
    if (useOutlierCut && nOutliers > 0) {
        double xLeg = xmaxA - 0.05 * dxA;
        double yLeg = ymaxA - 0.05 * dyA;
        std::ostringstream oss;
        oss << "Outliers (> " << outlierThresholdUm << " #mum)";
        TLatex* leg = new TLatex(xLeg, yLeg, oss.str().c_str());
        leg->SetTextAlign(33); // top-right corner
        leg->SetTextColor(kRed+1);
        leg->SetTextSize(0.03);
        leg->Draw("SAME");
    }

    //==========================================================================
    // RIGHT — histogram of Z range (blue=good, red=outliers)
    //==========================================================================
    c->cd(2);

    gPad->SetLeftMargin(0.12);
    gPad->SetRightMargin(0.05);
    gPad->SetTopMargin(0.10);
    gPad->SetBottomMargin(0.12);

    // Use ALL points to define histogram limits (so outliers are visible)
    double rAllMin = 0.0;
    double rAllMax = 1.0;
    if (!rangeZ_um.empty()) {
        auto [mnAll, mxAll] = std::minmax_element(rangeZ_um.begin(),
                                                  rangeZ_um.end());
        rAllMin = std::floor(*mnAll);
        rAllMax = std::ceil(*mxAll);
        if (rAllMax <= rAllMin) rAllMax = rAllMin + 1.0;
    }

    int nBinsHist = static_cast<int>(rAllMax - rAllMin);

    TH1D* hGood = new TH1D("hRangeGood",
                           "Z range across files;Range [#mum];Counts",
                           nBinsHist, rAllMin, rAllMax);

    TH1D* hOutl = new TH1D("hRangeOutl",
                           "Z range outliers",
                           nBinsHist, rAllMin, rAllMax);

    // Fill good and outlier histograms
    for (size_t i = 0; i < nPoints; ++i) {
        if (useOutlierCut && isOut[i]) {
            hOutl->Fill(rangeZ_um[i]);
        } else {
            hGood->Fill(rangeZ_um[i]);
        }
    }

    // Style good histogram (blue)
    hGood->SetFillColor(kAzure+7);
    hGood->SetLineColor(kBlue+3);
    hGood->SetLineWidth(2);
    hGood->SetStats(0);   // custom stats box

    // Style outlier histogram (solid red)
    hOutl->SetFillColor(kRed-4);
    hOutl->SetFillStyle(1001);
    hOutl->SetLineColor(kRed+2);
    hOutl->SetLineWidth(1);
    hOutl->SetStats(0);

    // Draw histograms: good first, then outliers
    hGood->Draw("HIST");
    hOutl->Draw("HIST SAME");

    // Custom stats box (GOOD POINTS ONLY)
    int entries = static_cast<int>(hGood->GetEntries());
    double mean  = hGood->GetMean();
    double rms   = hGood->GetStdDev();

    double x1 = 0.68, x2 = 0.94;
    double y1 = 0.70, y2 = 0.88;

    TPaveText* statsBox = new TPaveText(x1, y1, x2, y2, "NDC");
    statsBox->SetFillColor(kWhite);
    statsBox->SetLineColor(kBlack);
    statsBox->SetTextAlign(12);
    statsBox->SetTextFont(42);
    statsBox->SetTextSize(0.028);

    char sbuf[128];

    std::snprintf(sbuf, sizeof(sbuf), "Entries: %d", entries);
    statsBox->AddText(sbuf);

    std::snprintf(sbuf, sizeof(sbuf), "Mean: %.1f #mum", mean);
    statsBox->AddText(sbuf);

    std::snprintf(sbuf, sizeof(sbuf), "StdDev: %.1f #mum", rms);
    statsBox->AddText(sbuf);

    statsBox->Draw("SAME");

    gPad->Modified();
    gPad->Update();

    // Write histograms to file as well
    hGood->Write();
    hOutl->Write();

    //--------------------------------------------------------------------------
    // Save canvas and PNG
    //--------------------------------------------------------------------------
    string pngOut = outRoot.substr(0, outRoot.find_last_of('.')) + ".png";
    c->SaveAs(pngOut.c_str());
    c->Write("CompareScanCanvas");

    //--------------------------------------------------------------------------
    // Stats CSV output
    //--------------------------------------------------------------------------
    if (doStats) {
        ofstream csv(statsCsvName.c_str());
        if (!csv) {
            cerr << "Error: could not open stats CSV file: "
                 << statsCsvName << "\n";
        } else {
            csv << "index,label,X_mm,Y_mm,meanZ_mm,sigmaZ_um,rangeZ_um,"
                   "Zmin_mm,Zmax_mm,isOut\n";

            for (size_t i = 0; i < nPoints; ++i) {
                // mm-format: 3 decimals
                double x_mm    = X[i];
                double y_mm    = Y[i];
                double meanZ   = meanZ_mm[i];
                double zmin_mm = Zmin_mm[i];
                double zmax_mm = Zmax_mm[i];

                // µm-format: 1 decimal
                double sigma_um = sigmaZ_um[i];
                double range_um = rangeZ_um[i];

                csv << std::fixed
                    << std::setprecision(0) << (i + 1) << ",";
                csv << allPoints[0][i].label << ",";
                csv << std::fixed << std::setprecision(3) << x_mm << ",";
                csv << std::fixed << std::setprecision(3) << y_mm << ",";
                csv << std::fixed << std::setprecision(3) << meanZ << ",";
                csv << std::fixed << std::setprecision(1) << sigma_um << ",";
                csv << std::fixed << std::setprecision(1) << range_um << ",";
                csv << std::fixed << std::setprecision(3) << zmin_mm << ",";
                csv << std::fixed << std::setprecision(3) << zmax_mm << ",";
                csv << isOut[i] << "\n";
            }

            csv.close();
            cout << "\nWrote per-point statistics to " << statsCsvName << "\n";
        }
    }

    cout << "\nSaved PNG: "  << pngOut  << "\n";
    cout << "Saved ROOT: "  << outRoot << "\n";
    cout << "Close canvas to exit.\n";

    // Keep file open while GUI runs, then close cleanly
    app.Run();
    outF.Close();

    return 0;
}
