//==============================================================================
// File: CompareScan.cpp
// Version: 3.7
//
// Purpose:
//   Compare two 4-column point files of the form:
//
//       iPoint   X   Y   Z
//
//   and produce:
//
//     1) A 3D color-coded scatter plot of dZ = Z₂ − Z₁ (in µm)
//        drawn with TGraph2D as a function of (X, Y, dZ).
//        Each point’s color represents the value of dZ using a continuous
//        palette scaled in micrometers.
//
//     2) A 1D histogram of the same dZ distribution (in µm),
//        displayed side-by-side with the 3D plot for direct comparison.
//
//     3) Additional histograms of dX and dY (in µm), stored in the
//        output ROOT file but not displayed.
//
// Operation:
//   Points with the same index (iPoint) in both input files are compared.
//   The coordinate differences dX, dY, dZ, and the magnitude
//   dR = √(dX² + dY² + dZ²) are computed for each corresponding pair.
//
// Usage:
//   ./CompareScan file1.txt file2.txt [output.root]
//
//   - file1.txt and file2.txt must have identical structure and number of points.
//   - If [output.root] is not provided, output is saved as "CompareScan.root".
//
// Output:
//   - ROOT file containing histograms:
//       hDX, hDY, hDZ  →  ΔX, ΔY, ΔZ distributions [µm]
//   - Canvas showing:
//       Left  →  3D color-coded dZ(X,Y) scatter plot
//       Right →  1D histogram of dZ
//
// Notes:
//   • The dZ (color) range is automatically determined but can be overridden
//     using g2->SetMinimum() and g2->SetMaximum() before Draw().
//   • All Z-differences are expressed in micrometers (µm);
//     input coordinates remain in millimeters (mm).
//   • Points are compared strictly by index — no geometric matching is performed.
//
// Dependencies:
//   ROOT framework (https://root.cern/)
//   Custom point reader: Points.h / Points.cpp
//
// Compilation example (macOS):
//   clang++ -std=c++17 -O2 CompareScan.cpp Points.cpp \
//       $(root-config --cflags --libs) -lGui -o CompareScan
//
// Author: Luciano Ristori
// Date:   October 2025
//==============================================================================

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
#include "TGraph2D.h"
#include "TROOT.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TColor.h"
#include "TPaletteAxis.h"
#include "TLatex.h"

using namespace std;

//------------------------------------------------------------------------------
// Simple statistics container
//------------------------------------------------------------------------------
struct DiffStats { double mean=0, sigma=0; size_t n=0; };
DiffStats computeStats(const vector<double>& v){
    DiffStats s; if(v.empty())return s;
    double sum=0,sum2=0;
    for(double x:v){sum+=x;sum2+=x*x;}
    s.n=v.size(); s.mean=sum/s.n; s.sigma=sqrt(sum2/s.n-s.mean*s.mean);
    return s;
}

//------------------------------------------------------------------------------
int main(int argc,char*argv[]){
    if(argc<3){cerr<<"Usage: "<<argv[0]<<" file1 file2 [out.root]\n";return 1;}
    string f1=argv[1], f2=argv[2];
    string out=(argc>=4)?argv[3]:"CompareScan.root";
    if(out.rfind(".root")==string::npos) out+=".root";

    vector<Point>A=readPoints(f1,4),B=readPoints(f2,4);
    if(A.empty()||B.empty()){cerr<<"Error reading files\n";return 1;}
    size_t n=min(A.size(),B.size());
    vector<double>dX,dY,dZ,dR; dX.reserve(n);dY.reserve(n);dZ.reserve(n);dR.reserve(n);
    for(size_t i=0;i<n;++i){
        double dx=B[i].coords[1]-A[i].coords[1];
        double dy=B[i].coords[2]-A[i].coords[2];
        double dz=B[i].coords[3]-A[i].coords[3];
        dX.push_back(dx); dY.push_back(dy); dZ.push_back(dz);
        dR.push_back(sqrt(dx*dx+dy*dy+dz*dz));
    }

    DiffStats sx=computeStats(dX),sy=computeStats(dY),sz=computeStats(dZ),sr=computeStats(dR);
    cout<<fixed<<setprecision(4)
        <<"\nComparison ("<<n<<" points)\n"
        <<"dX mean="<<sx.mean*1000<<" µm σ="<<sx.sigma*1000
        <<"\ndY mean="<<sy.mean*1000<<" µm σ="<<sy.sigma*1000
        <<"\ndZ mean="<<sz.mean*1000<<" µm σ="<<sz.sigma*1000
        <<"\ndR mean="<<sr.mean*1000<<" µm σ="<<sr.sigma*1000<<endl;

    // ROOT setup
    TApplication app("app",&argc,argv);
    gROOT->SetBatch(false);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(64);

    TFile outF(out.c_str(),"RECREATE");

    //------------------------------------------------------------------------------
    // Histograms of dX, dY, and dZ in µm  (±10% margin restored)
    //------------------------------------------------------------------------------
    vector<double> dX_um(dX.size()), dY_um(dY.size()), dZ_um(dZ.size());
    transform(dX.begin(), dX.end(), dX_um.begin(), [](double v){ return v*1000.0; });
    transform(dY.begin(), dY.end(), dY_um.begin(), [](double v){ return v*1000.0; });
    transform(dZ.begin(), dZ.end(), dZ_um.begin(), [](double v){ return v*1000.0; });

    auto findRange = [](const vector<double>& v, double &minV, double &maxV) {
        auto [minIt,maxIt]=minmax_element(v.begin(),v.end());
        minV=*minIt; maxV=*maxIt;
        double range=maxV-minV;
        if(range<=0) range=fabs(maxV)*0.1;
        double margin=0.1*range;  // 10% margin on each side
        minV-=margin; maxV+=margin;
    };

    double xmin,xmax,ymin,ymax,zmin,zmax;
    findRange(dX_um,xmin,xmax);
    findRange(dY_um,ymin,ymax);
    findRange(dZ_um,zmin,zmax);

    auto hDX = new TH1D("hDX","dX distribution;dX [#mum];Counts",100,xmin,xmax);
    auto hDY = new TH1D("hDY","dY distribution;dY [#mum];Counts",100,ymin,ymax);
    auto hDZ = new TH1D("hDZ","dZ distribution;dZ [#mum];Counts",100,zmin,zmax);

    for (size_t i=0;i<n;++i){
        hDX->Fill(dX_um[i]);
        hDY->Fill(dY_um[i]);
        hDZ->Fill(dZ_um[i]);
    }

    hDX->Write();
    hDY->Write();
    hDZ->Write();

    //------------------------------------------------------------------------------
    // 3D map using TGraph2D
    //------------------------------------------------------------------------------
    vector<double> x,y,z;
    for(size_t i=0;i<n;++i){
        x.push_back(A[i].coords[1]);
        y.push_back(A[i].coords[2]);
        z.push_back(dZ[i]*1000.0); // µm
    }

    auto g2=new TGraph2D(n,&x[0],&y[0],&z[0]);
    g2->SetTitle("dZ map;X [mm];Y [mm];dZ [#mum]");
    g2->SetMarkerStyle(20);
    g2->SetMarkerSize(2.5);

    double zMin=*min_element(z.begin(),z.end());
    double zMax=*max_element(z.begin(),z.end());
    cout<<"zMin="<<zMin<<" µm, zMax="<<zMax<<" µm"<<endl;

    //------------------------------------------------------------------------------
    // Canvas with two pads (same look)
    //------------------------------------------------------------------------------
    TCanvas *c = new TCanvas("cFlat","CompareScan: dZ map + histogram",1200,600);
    c->Divide(2,1,0.001,0.001);

    // -------------------- LEFT PAD (3D scatter + color bar) --------------------
    c->cd(1);
    gPad->SetRightMargin(0.18);
    gPad->SetLeftMargin(0.12);
    gPad->SetBottomMargin(0.12);
    gPad->SetTopMargin(0.10);

    g2->SetMinimum(zMin - 0.1);
    g2->SetMaximum(zMax + 0.1);

    g2->Draw("PCOLZ");
    gPad->Update();

    TH2D *hist = g2->GetHistogram();
    if (hist) {
        hist->GetXaxis()->SetTitleOffset(1.6);
        hist->GetYaxis()->SetTitleOffset(2.0);
        hist->GetXaxis()->SetTitleSize(0.03);
        hist->GetYaxis()->SetTitleSize(0.03);
        hist->GetXaxis()->SetLabelSize(0.03);
        hist->GetYaxis()->SetLabelSize(0.03);
    }

    TPaletteAxis *pal = nullptr;
    if (g2->GetHistogram()) {
        pal = (TPaletteAxis*) g2->GetHistogram()->GetListOfFunctions()->FindObject("palette");
    }

    if (pal) {
        pal->SetX1NDC(0.86);
        pal->SetX2NDC(0.90);
        pal->SetY1NDC(0.20);
        pal->SetY2NDC(0.80);
        pal->SetLabelFont(42);
        pal->SetLabelSize(0.03);
        pal->SetLabelOffset(0.005);
        pal->SetTitle("");

        double mid = 0.5 * (pal->GetY1NDC() + pal->GetY2NDC());
        TLatex *lt = new TLatex();
        lt->SetTextAngle(90);
        lt->SetTextFont(42);
        lt->SetTextSize(0.03);
        lt->SetNDC();
        lt->DrawLatex(pal->GetX2NDC() + 0.065, mid, "dZ [#mum]");
    }

    gPad->Modified();
    gPad->Update();

    // -------------------- RIGHT PAD (1D histogram in µm) --------------------
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
    cout<<"\nWrote "<<out<<". Close the canvas to exit.\n";
    app.Run();
    
    outF.cd();            // make sure we’re writing into the ROOT file
	c->Write("CompareScanCanvas");  // save full canvas with both pads

    outF.Close();
    return 0;
}
