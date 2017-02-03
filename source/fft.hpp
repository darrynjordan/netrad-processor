#ifndef FFT_HPP
#define FFT_HPP

#include <fftw3.h>
#include "main.hpp"

void fftDopplerData(void);
void fftRefData(void);
void fftRangeData(void);
void ifftMatchedData(void);
void hilbertTransform(void);
void freePlanMemory(void);

extern fftw_plan rangePlan;
extern fftw_plan refPlan;
extern fftw_plan resultPlan;

extern fftw_plan hilbertPlan;
extern fftw_plan dopplerPlan;

#endif


