#pragma once

#include <Minuit2/FunctionMinimum.h>
#include <Minuit2/MnScan.h>

#include <memory>

#include <goofit/GlobalCudaDefines.h>
#include <goofit/fitting/FCN.h>

namespace GooFit {

class PdfBase;

enum class FitErrors : int { Valid = 0, NotRun = 50, InValid };

class FitManagerMinuit2 {
  public:
    FitManagerMinuit2(PdfBase *dat);

    /// This runs the fit
    ROOT::Minuit2::FunctionMinimum fit();

    /*
     * Scans the values of FCN as a function of one parameter.
     * and plots the resulting values as a curve using MNPLOT.
     * It may be called to scan one parameter or all parameters.
     * retains the best function and parameter values found.
     */


    /// Set the maximum number of calls. 0 for Minuit2 default.
    void setMaxCalls(unsigned int max_calls = 0) { maxfcn_ = max_calls; }

    /// Get a pointer to the params
    Params *getParams() { return &upar_; }

    /// Get a pointer to the fcn
    FCN *getFCN() { return &fcn_; }

    /// Check to see if fit is valid
    operator bool() const { return retval_ == FitErrors::Valid; }

    /// Return value for program
    operator int() const { return static_cast<int>(retval_); }

    /// Set the fitting verbosity
    void setVerbosity(int value) { verbosity = value; }

    /// Get the fitting verbosity
    int getVerbosity() const { return verbosity; }

  private:
    Params upar_;
    FCN fcn_;
    unsigned int maxfcn_{0};
    FitErrors retval_{FitErrors::NotRun};
    int verbosity{3};
};
} // namespace GooFit
