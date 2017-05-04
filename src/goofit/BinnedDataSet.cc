#include "goofit/BinnedDataSet.h"
#include "goofit/Variable.h"
#include "goofit/Error.h"
#include "goofit/Log.h"
#include <cassert>

// Special constructor for one variable
BinnedDataSet::BinnedDataSet(Variable* var, std::string n)
    : DataSet(var, n) {
    cacheNumBins();
    binvalues.resize(getNumBins());
}

BinnedDataSet::BinnedDataSet(std::vector<Variable*>& vars, std::string n)
    : DataSet(vars, n) {
    cacheNumBins();
    binvalues.resize(getNumBins());
}

BinnedDataSet::BinnedDataSet(std::set<Variable*>& vars, std::string n)
    : DataSet(vars, n) {
    cacheNumBins();
    binvalues.resize(getNumBins());
}

BinnedDataSet::BinnedDataSet(std::initializer_list<Variable*> vars, std::string n)
: DataSet(vars, n) {
    cacheNumBins();
    binvalues.resize(getNumBins());
}

BinnedDataSet::~BinnedDataSet() {}

void BinnedDataSet::addEvent() {
    size_t ibin = getBinNumber();
    binvalues.at(ibin) += 1;
}

void BinnedDataSet::addWeightedEvent(double weight) {
    size_t ibin = getBinNumber();
    binvalues.at(ibin) += weight;
    
}


void BinnedDataSet::cacheNumBins() {
    for(size_t i=0; i<size(); i++) {
        cachedNumBins[i] = variables[i]->numbins;
    }
}

size_t BinnedDataSet::getBinNumber() const {
    std::vector<fptype> vals = getCurrentValues();
    std::vector<size_t> locals = convertValuesToBins(vals);
    return localToGlobal(locals);
}

size_t BinnedDataSet::localToGlobal(const std::vector<size_t>& locals) const {
    unsigned int priorMatrixSize = 1;
    unsigned int ret = 0;

    for(size_t i=0; i<size(); i++) {
        unsigned int localBin = locals[i];
        ret += localBin * priorMatrixSize;
        priorMatrixSize *= cachedNumBins[i];
    }

    return ret;
}

std::vector<size_t> BinnedDataSet::globalToLocal(size_t global) const {
    std::vector<size_t> locals;

    // To convert global bin number to (x,y,z...) coordinates: For each dimension, take the mod
    // with the number of bins in that dimension. Then divide by the number of bins, in effect
    // collapsing so the grid has one fewer dimension. Rinse and repeat.
    for(size_t i=0; i<size(); i++) {
        int localBin = global % cachedNumBins[i];
        locals.push_back(localBin);
        global /= cachedNumBins[i];
    }
    return locals;
}

fptype BinnedDataSet::getBinCenter(size_t ivar, size_t bin) const {
    std::vector<size_t> locals = globalToLocal(bin);
    size_t localBin = locals.at(ivar);
    
    fptype ret = (variables[ivar]->upperlimit - variables[ivar]->lowerlimit) / cachedNumBins[ivar];
    ret       *= (localBin + 0.5);
    ret       += variables[ivar]->lowerlimit;
    return ret;
}

fptype BinnedDataSet::getBinCenter(Variable* var, size_t bin) const {
    size_t ivar = indexOfVariable(var);
    return getBinCenter(ivar, bin);
}


fptype BinnedDataSet::getBinVolume(size_t bin) const {
    fptype ret = 1;

    for(size_t i=0; i<size(); i++) {
        fptype step = (variables[i]->upperlimit - variables[i]->lowerlimit) / cachedNumBins[i];
        ret *= step;
    }

    return ret;
}

fptype BinnedDataSet::getBinError(size_t bin) const {
    if(0 == binerrors.size())
        return sqrt(binvalues.at(bin));

    return binerrors.at(bin);
}

void BinnedDataSet::setBinError(unsigned int bin, fptype error) {
    if(0 == binerrors.size())
        binerrors.resize(binvalues.size());

    binerrors.at(bin) = error;
}

size_t BinnedDataSet::getNumBins() const {
    unsigned int ret = 1;

    for(size_t i=0; i<size(); i++) {
        ret *= cachedNumBins[i];
    }

    return ret;
}

fptype BinnedDataSet::getNumEvents() const {
    fptype ret = 0;

    for(const fptype& bin : binvalues)
        ret += bin;

    return ret;
}

std::vector<size_t> BinnedDataSet::convertValuesToBins(const std::vector<fptype>& vals) const {
    if(vals.size() != size())
        throw GooFit::GeneralError("Incorrect number of bins {} for {} variables", vals.size(), size());

    std::vector<size_t> localBins;
    for(size_t i=0; i<size(); i++) {
        fptype currval = vals[i];
        fptype betval = std::min(std::max(currval, variables[i]->lowerlimit),variables[i]->upperlimit);
        if(currval != betval)
            GOOFIT_INFO("Warning: Value {} outside {} range [{},{}] - clamping",
                        currval, variables[i]->name, variables[i]->lowerlimit, variables[i]->upperlimit);
        fptype step = (variables[i]->upperlimit - variables[i]->lowerlimit) / cachedNumBins[i];
        localBins.push_back( (size_t) floor((currval - variables[i]->lowerlimit)/step));
    
    }
    

    return localBins;
}
