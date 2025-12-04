//==============================================================================
// File: CompareScan.cpp
// Version: 3.9 (2D scatter, colored markers + dZ labels, no palette bar)
//==============================================================================

#define COMPARESCAN_VERSION "v1.0"

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
// Simple statistics container
//------------------------------------------------------------------------------
struct DiffStats { double mean=0, sigma=0; size_t n=0; };

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
int main(int argc, char* argv[])
{
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " file1 file2 [out.root]\n";
        return 1;
    }

    string f1  = argv[1];
    string f2  = argv[2];
    string out = (argc >= 4) ? argv[3] : "CompareScan.root";
    if (out.rfind(".root") == string::npos) out += ".root";

    cout << "\n====================================\n";
    cout << " CompareScan " << COMPARESCAN_VERSION << " — Luciano Ristori\n";
    cout << " Built: " << __DATE__ << " " << __TIME__ << endl;
    cout << "====================================\n";

    cout << "Input file 1: " << f1 << endl;
    cout << "Input file 2: " << f2 << endl;
    cout << "Output file : " << out << endl;

    // Read points
    vector<Point> A = readPoints(f1, 3);
    vector<Point> B = readPoints(f2, 3);
    if (A.empty() || B.empty()) {
        cerr << "Error reading files\n";
        return 1;
    }

    size_t n = std::min(A.size(), B.size());

    vector<double> dX, dY, dZ, dR;
    dX.reserve(n);
    dY.reserve(n);
    dZ.reserve(n);
    dR.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        double dx = B[i].coords[0] - A[i].coords[0];
        double dy = B[i].coords[1] - A[i].coords[1];
        double dz = B[i].coords[2] - A[i].coords[2];
        dX.push_back(dx);
        dY.push_back(dy);
        dZ.push_back(dz);
        dR.push_back(std::sqrt(dx*dx + dy*dy + dz*dz));
    }

    DiffStats sx = computeStats(dX);
    DiffStats sy = computeStats(dY);
    DiffStats sz = computeStats(dZ);
    DiffStats sr = computeStats(dR);

    cout << fixed << setprecision(4)
         << "\nComparison (" << n << " points)\n"
         << "dX mean=" << sx.mean*1000 << " µm σ=" << sx.sigma*1000
         << "\ndY mean=" << sy.mean*1000 << " µm σ=" << sy.sigma*1000
         << "\ndZ mean=" << sz.mean*1000 << " µm σ=" << sz.sigma*1000
         << "\ndR mean=" << sr.mean*1000 << " µm σ=" << sr.sigma*1000
         << endl;

    // ROOT startup
    TApplication app("app", &argc, argv);
    gROOT->SetBatch(false);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(64);

    TFile outF(out.c_str(), "RECREATE");

    //------------------------------------------------------------------------------
    // Convert deltas to micrometers
    //------------------------------------------------------------------------------
    vector<double> dX_um(n), dY_um(n), dZ_um(n);
    for (size_t i = 0; i < n; ++i) {
        dX_um[i] = dX[i] * 1000.0;
        dY_um[i] = dY[i] * 1000.0;
        dZ_um[i] = dZ[i] * 1000.0;
    }

    //------------------------------------------------------------------------------
    // Histogram ranges (±10% margin)
    //------------------------------------------------------------------------------
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

    double xmin, xmax, ymin, ymax, zmin, zmax;
    findRange(dX_um, xmin, xmax);
    findRange(dY_um, ymin, ymax);
    findRange(dZ_um, zmin, zmax);

    auto hDX = new TH1D("hDX", "dX distribution;dX [#mum];Counts", 100, xmin, xmax);
    auto hDY = new TH1D("hDY", "dY distribution;dY [#mum];Counts", 100, ymin, ymax);
    auto hDZ = new TH1D("hDZ", "dZ distribution;dZ [#mum];Counts", 100, zmin, zmax);

    for (size_t i = 0; i < n; ++i) {
        hDX->Fill(dX_um[i]);
        hDY->Fill(dY_um[i]);
        hDZ->Fill(dZ_um[i]);
    }

    hDX->Write();
    hDY->Write();
    hDZ->Write();

    //------------------------------------------------------------------------------
    // Canvas with left (2D scatter) and right (histogram)
    //------------------------------------------------------------------------------
    TCanvas* c = new TCanvas("cFlat", "CompareScan: dZ map + histogram", 1200, 600);
    c->Divide(2, 1, 0.001, 0.001);

    //==========================================================================
    // LEFT PAD — CLEAN 2D SCATTER, COLORED MARKERS + dZ LABELS (NO PALETTE BAR)
    //==========================================================================
    c->cd(1);
    gPad->SetRightMargin(0.05);
    gPad->SetLeftMargin(0.12);
    gPad->SetBottomMargin(0.12);
    gPad->SetTopMargin(0.10);

    gPad->SetFillColor(kWhite);
    gPad->SetFrameFillColor(kWhite);
    gPad->SetFrameFillStyle(0);

    // Compute XY ranges with margins
    double xminA = 1e99, xmaxA = -1e99;
    double yminA = 1e99, ymaxA = -1e99;
    for (size_t i = 0; i < n; ++i) {
        xminA = std::min(xminA, A[i].coords[0]);
        xmaxA = std::max(xmaxA, A[i].coords[0]);
        yminA = std::min(yminA, A[i].coords[1]);
        ymaxA = std::max(ymaxA, A[i].coords[1]);
    }
    double dxA = xmaxA - xminA;
    double dyA = ymaxA - yminA;
    if (dxA <= 0) dxA = 1.0;
    if (dyA <= 0) dyA = 1.0;
    xminA -= 0.05 * dxA;  xmaxA += 0.05 * dxA;
    yminA -= 0.05 * dyA;  ymaxA += 0.05 * dyA;

    // dZ range in micrometers for color mapping
    double Zmin = *min_element(dZ_um.begin(), dZ_um.end());
    double Zmax = *max_element(dZ_um.begin(), dZ_um.end());
    double Zrange = Zmax - Zmin;
    if (Zrange <= 0) Zrange = 1.0;

    int nColors = gStyle->GetNumberOfColors();
    if (nColors < 2) nColors = 64;

    // Frame: axes + optional grid, no fill, no palette
    TH2D* frame = new TH2D("frame_xy", ";X [mm];Y [mm]",
                           100, xminA, xmaxA,
                           100, yminA, ymaxA);
    frame->SetStats(false);
    frame->SetFillStyle(0);
    frame->Draw("AXIS");
    frame->Draw("AXIG SAME");  // grid

    // Draw colored markers + dZ labels
    for (size_t i = 0; i < n; ++i) {
        double xx = A[i].coords[0];
        double yy = A[i].coords[1];
        double zz = dZ_um[i];  // µm

        double norm = (zz - Zmin) / Zrange;
        norm = std::max(0.0, std::min(1.0, norm));
        int ci = gStyle->GetColorPalette(int(norm * (nColors - 1)));

        // Marker
        TMarker* m = new TMarker(xx, yy, 20);
        m->SetMarkerColor(ci);
        m->SetMarkerSize(1.8);
        m->Draw("SAME");

        // Small label with dZ in µm above the marker
        char buf[32];
       	snprintf(buf, sizeof(buf), "%d", (int) llround(zz));
        double yOffset = 0.02 * (ymaxA - yminA);      // 1% of Y range
        TLatex* t = new TLatex(xx, yy + yOffset, buf);
        t->SetTextSize(0.02);
        t->SetTextColor(1);                          // black
        t->SetTextAlign(21);                          // centered horizontally
        t->Draw("SAME");
    }

    gPad->Modified();
    gPad->Update();

    //==========================================================================
    // RIGHT PAD — dZ histogram
    //==========================================================================
    c->cd(2);

    gPad->SetLeftMargin(0.12);
    gPad->SetRightMargin(0.05);
    gPad->SetTopMargin(0.10);
    gPad->SetBottomMargin(0.12);

    hDZ->SetFillColor(kAzure+7);
    hDZ->SetLineColor(kBlue+3);
    hDZ->SetLineWidth(2);
    hDZ->Draw("HIST");

    gPad->Modified();
    gPad->Update();

    //------------------------------------------------------------------------------
    // Save outputs
    //------------------------------------------------------------------------------
    string pngOut = out.substr(0, out.find_last_of(".")) + ".png";
    c->SaveAs(pngOut.c_str());
    cout << "Saved canvas image as " << pngOut << endl;

    c->Write("CompareScanCanvas");

    cout << "\nWrote " << out << ". Close the canvas to exit.\n";

    app.Run();
    outF.Close();
    return 0;
}
