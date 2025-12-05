//==============================================================================
// File: CompareScan.cpp
// Version: 3.11
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
//         - Exclude them from global statistics and histogram
//         - Mark them in red on the scatter
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
//           · Excluded from the histogram
//           · Shown in red in the scatter plot
//           · Flagged in the CSV (isOut = 1)
//
// Output:
//   - ROOT file (out.root) with:
//       * XY scatter canvas (left pad)
//       * Range histogram (right pad, only non-outliers if --outl used)
//   - PNG snapshot of the canvas.
//   - If --stats is used, a CSV with per-point statistics:
//       index, label, X_mm, Y_mm, meanZ_mm, sigmaZ_um, rangeZ_um, Zmin_mm, Zmax_mm, isOut
//
// Notes:
//   - X,Y are taken from the first file.
//   - All files must have at least as many points as the shortest one; extra
//     points in longer files are ignored (a warning is printed).
//==============================================================================
//==============================================================================
// File: CompareScan.cpp
// Version: 3.12
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
//     • Draw a 1D histogram of Range(i) in µm (non-outliers only if --outl used)
//     • Optionally label each point on the scatter with its Range(i) [µm]
//     • Optionally write a CSV with per-point statistics across files.
//     • Optionally define outliers via --outl N, excluding them from global
//       statistics + histogram, marking them in red, and flagging in CSV.
//
// Usage:
//   ./CompareScan [--labels] [--stats [stats.csv]] [--outl N]
//                 file1 file2 ... fileN [out.root]
//
//==============================================================================

#define COMPARESCAN_VERSION "v3.12"

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
    s.sigma = sqrt(sum2 / s.n - s.mean * s.mean);
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
    bool drawLabels = false;
    bool doStats = false;
    bool statsCsvExplicit = false;
    bool useOutlierCut = false;
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
                if (next.rfind("--",0) != 0) {
                    statsCsvName = next;
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
            try { outlierThresholdUm = stod(next); }
            catch (...) {
                cerr << "Error: invalid outlier threshold.\n";
                return 1;
            }
            useOutlierCut = true;
            continue;
        }

        positional.push_back(arg);
    }

    if (positional.size() < 2) {
        cerr << "Error: need >=2 input files.\n";
        return 1;
    }

    // last positional arg may be .root
    {
        const string &last = positional.back();
        if (last.size() > 5 && last.substr(last.size()-5) == ".root") {
            outRoot = last;
            positional.pop_back();
        }
    }

    if (positional.size() < 2) {
        cerr << "Error: fewer than 2 input files remain.\n";
        return 1;
    }

    // default stats filename
    if (doStats && !statsCsvExplicit) {
        string base = outRoot.substr(0, outRoot.find_last_of("."));
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
        cout << "  ["<<i+1<<"] " << positional[i] << "\n";

    cout << "Output ROOT : " << outRoot << "\n";
    cout << "Labels      : " << (drawLabels ? "ON" : "OFF") << "\n";
    if (doStats)
        cout << "Stats CSV   : " << statsCsvName << "\n";
    if (useOutlierCut)
        cout << "Outlier cut : > " << outlierThresholdUm << " µm\n";

    //--------------------------------------------------------------------------
    // Read files
    //--------------------------------------------------------------------------
    int nFiles = positional.size();
    vector<vector<Point>> allPoints(nFiles);

    for (int k = 0; k < nFiles; ++k) {
        allPoints[k] = readPoints(positional[k], 3);
        if (allPoints[k].empty()) {
            cerr << "Error: file " << positional[k] << " yielded no points.\n";
            return 1;
        }
    }

    // find common number of points
    size_t nPoints = allPoints[0].size();
    bool sizeMismatch = false;

    for (int k = 1; k < nFiles; ++k)
        if (allPoints[k].size() != nPoints) {
            sizeMismatch = true;
            nPoints = min(nPoints, allPoints[k].size());
        }

    if (sizeMismatch)
        cerr << "Warning: different file sizes; using first "
             << nPoints << " points.\n";

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
    vector<int>    isOut(nPoints,0);

    for (size_t i=0;i<nPoints;++i) {
        X[i] = allPoints[0][i].coords[0];
        Y[i] = allPoints[0][i].coords[1];

        double zmin =  1e99;
        double zmax = -1e99;
        double sumZ=0,sumZ2=0;

        for (int k=0;k<nFiles;++k) {
            double z = allPoints[k][i].coords[2];
            zmin = min(zmin, z);
            zmax = max(zmax, z);
            sumZ  += z;
            sumZ2 += z*z;
        }

        double meanZ = sumZ/nFiles;
        double varZ  = (nFiles>1) ? (sumZ2/nFiles - meanZ*meanZ) : 0;
        if (varZ < 0) varZ = 0;

        meanZ_mm[i]  = meanZ;
        sigmaZ_um[i] = sqrt(varZ)*1000;
        rangeZ_um[i] = (zmax - zmin)*1000;
        Zmin_mm[i]   = zmin;
        Zmax_mm[i]   = zmax;
    }

    //--------------------------------------------------------------------------
    // Outliers
    //--------------------------------------------------------------------------
    size_t nOutliers = 0;
    if (useOutlierCut) {
        for (size_t i=0;i<nPoints;++i)
            if (rangeZ_um[i] > outlierThresholdUm) {
                isOut[i] = 1;
                nOutliers++;
            }
    }
    size_t nGood = nPoints - nOutliers;

    // Build filtered arrays
    vector<double> sigmaUsed, rangeUsed;
    if (useOutlierCut) {
        sigmaUsed.reserve(nGood);
        rangeUsed.reserve(nGood);
        for (size_t i=0;i<nPoints;++i)
            if (!isOut[i]) {
                sigmaUsed.push_back(sigmaZ_um[i]);
                rangeUsed.push_back(rangeZ_um[i]);
            }
    } else {
        sigmaUsed = sigmaZ_um;
        rangeUsed = rangeZ_um;
    }

    //--------------------------------------------------------------------------
    // Global statistics (microns with 1 decimal)
    //--------------------------------------------------------------------------
    cout << fixed << setprecision(1);
    cout << "\nGlobal Z statistics:\n";
    cout << "  Total points      = " << nPoints << "\n";
    if (useOutlierCut) {
        cout << "  Outliers (> " << outlierThresholdUm << " µm) = "
             << nOutliers << " ("
             << (100.0*nOutliers/nPoints) << "%)\n";
        cout << "  Used (non-outliers) = " << nGood << "\n";
    }

    if (rangeUsed.empty()) {
        cout << "  WARNING: No non-outlier points — statistics empty.\n";
    } else {
        DiffStats sSigma = computeStats(sigmaUsed);
        DiffStats sRange = computeStats(rangeUsed);

        double maxSig = *max_element(sigmaUsed.begin(), sigmaUsed.end());
        double maxRan = *max_element(rangeUsed.begin(), rangeUsed.end());

        cout << "  Mean σ(Z)      = " << sSigma.mean  << " µm\n";
        cout << "  RMS σ(Z)       = " << sSigma.sigma << " µm\n";
        cout << "  Max σ(Z)       = " << maxSig       << " µm\n";
        cout << "  Mean Range(Z)  = " << sRange.mean  << " µm\n";
        cout << "  RMS Range(Z)   = " << sRange.sigma << " µm\n";
        cout << "  Max Range(Z)   = " << maxRan       << " µm\n";
    }

    //--------------------------------------------------------------------------
    // ROOT + histogram (1 µm binning)
    //--------------------------------------------------------------------------
    TApplication app("app",&argc,argv);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(64);
    TFile outF(outRoot.c_str(),"RECREATE");

    double rMin=0, rMax=0;
    if (!rangeUsed.empty()) {
        auto [mn,mx] = minmax_element(rangeUsed.begin(), rangeUsed.end());
        rMin = floor(*mn);
        rMax = ceil(*mx);
    }
    int nBins = max(1, int(rMax - rMin));

    TH1D* hRange = new TH1D("hRangeZ",
                            useOutlierCut ?
                              "Z range (non-outliers only);Range [#mum];Counts" :
                              "Z range;Range [#mum];Counts",
                            nBins, rMin, rMax);

    for (double rv : rangeUsed) hRange->Fill(rv);

    hRange->Write();

    //--------------------------------------------------------------------------
    // Canvas: scatter + histogram
    //--------------------------------------------------------------------------
    TCanvas *c = new TCanvas("cFlat","CompareScan: Z range map + histogram",
                              1200,600);
    c->Divide(2,1,0.001,0.001);

    //==========================================================================
    // LEFT — scatter
    //==========================================================================
    c->cd(1);
    gPad->SetRightMargin(0.05);
    gPad->SetLeftMargin(0.12);
    gPad->SetBottomMargin(0.12);

    double xminA=1e99,xmaxA=-1e99,yminA=1e99,ymaxA=-1e99;
    for (size_t i=0;i<nPoints;++i) {
        xminA=min(xminA,X[i]);
        xmaxA=max(xmaxA,X[i]);
        yminA=min(yminA,Y[i]);
        ymaxA=max(ymaxA,Y[i]);
    }
    double dxA=xmaxA-xminA, dyA=ymaxA-yminA;
    if (dxA<=0) dxA=1;
    if (dyA<=0) dyA=1;
    xminA-=0.05*dxA; xmaxA+=0.05*dxA;
    yminA-=0.05*dyA; ymaxA+=0.05*dyA;

    double ZminRange=*min_element(rangeZ_um.begin(),rangeZ_um.end());
    double ZmaxRange=*max_element(rangeZ_um.begin(),rangeZ_um.end());
    double ZrangeRange=ZmaxRange-ZminRange;
    if (ZrangeRange<=0) ZrangeRange=1;

    int nColors=gStyle->GetNumberOfColors();

    TH2D* frameXY = new TH2D("frame_xy",";X [mm];Y [mm]",
                             100,xminA,xmaxA,100,yminA,ymaxA);
    frameXY->SetStats(false);
    frameXY->Draw("AXIS");
    frameXY->Draw("AXIG SAME");

    double ySpan=ymaxA-yminA;
    double yOffset=0.02*ySpan;

    // draw markers
    for (size_t i=0;i<nPoints;++i) {
        double xx=X[i], yy=Y[i], zz=rangeZ_um[i];
        double norm=(zz - ZminRange)/ZrangeRange;
        norm=max(0.0,min(1.0,norm));
        int ci=gStyle->GetColorPalette(int(norm*(nColors-1)));

        if (useOutlierCut && isOut[i]) ci=kRed+1;

        TMarker *m=new TMarker(xx,yy,20);
        m->SetMarkerColor(ci);
        m->SetMarkerSize(1.8);
        m->Draw("SAME");

        if (drawLabels) {
            int zu=int(llround(zz));
            TLatex* t=new TLatex(xx,yy+yOffset,to_string(zu).c_str());
            t->SetTextColor(kBlack);        // always black (your choice)
            t->SetTextSize(0.025);
            t->SetTextAlign(21);
            t->Draw("SAME");
        }
    }

    // Outlier legend (top-right)
    if (useOutlierCut && nOutliers>0) {
        double xLeg=xmaxA - 0.05*dxA;
        double yLeg=ymaxA - 0.05*dyA;
        ostringstream oss;
        oss<<"Outliers (> "<<outlierThresholdUm<<" #mum)";
        TLatex *leg=new TLatex(xLeg,yLeg,oss.str().c_str());
        leg->SetTextAlign(33); // top-right corner
        leg->SetTextColor(kRed+1);
        leg->SetTextSize(0.03);
        leg->Draw("SAME");
    }

    //==========================================================================
    // RIGHT — histogram
    //==========================================================================
    c->cd(2);
    gPad->SetLeftMargin(0.12);
    gPad->SetBottomMargin(0.12);

    hRange->SetFillColor(kAzure+7);
    hRange->SetLineColor(kBlue+3);
    hRange->SetLineWidth(2);
    hRange->Draw("HIST");

    //--------------------------------------------------------------------------
    // Save outputs
    //--------------------------------------------------------------------------
    string pngOut = outRoot.substr(0,outRoot.find_last_of('.')) + ".png";
    c->SaveAs(pngOut.c_str());
    c->Write("CompareScanCanvas");

    //--------------------------------------------------------------------------
    // Stats CSV
    //--------------------------------------------------------------------------
    // Stats CSV output
if (doStats) {
    ofstream csv(statsCsvName.c_str());
    if (!csv) {
        cerr << "Error: could not open stats CSV file: " << statsCsvName << "\n";
    } else {

        csv << "index,label,X_mm,Y_mm,meanZ_mm,sigmaZ_um,rangeZ_um,"
               "Zmin_mm,Zmax_mm,isOut\n";

        for (size_t i = 0; i < nPoints; ++i) {

            // mm-format: 3 decimals
            csv << std::fixed << std::setprecision(3);

            double x_mm     = X[i];
            double y_mm     = Y[i];
            double meanZ    = meanZ_mm[i];
            double zmin_mm  = Zmin_mm[i];
            double zmax_mm  = Zmax_mm[i];

            // µm-format: 1 decimal
            csv << std::fixed << std::setprecision(1);

            double sigma_um = sigmaZ_um[i];
            double range_um = rangeZ_um[i];

            // Write line
            csv  << (i + 1) << ","
                 << allPoints[0][i].label << ","
                 << std::fixed << std::setprecision(3) << x_mm << ","
                 << std::fixed << std::setprecision(3) << y_mm << ","
                 << std::fixed << std::setprecision(3) << meanZ << ","
                 << std::fixed << std::setprecision(1) << sigma_um << ","
                 << std::fixed << std::setprecision(1) << range_um << ","
                 << std::fixed << std::setprecision(3) << zmin_mm << ","
                 << std::fixed << std::setprecision(3) << zmax_mm << ","
                 << isOut[i]
                 << "\n";
        }

        csv.close();
        cout << "\nWrote per-point statistics to " << statsCsvName << "\n";
    }
}


    cout<<"\nSaved PNG: "<<pngOut<<"\n";
    cout<<"Saved ROOT: "<<outRoot<<"\n";
    cout<<"Close canvas to exit.\n";

    app.Run();
    outF.Close();
    
    return 0;
}
