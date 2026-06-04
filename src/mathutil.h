#ifndef MATHUTIL_H
#define MATHUTIL_H

#include "types.h"

#define PI 3.14159265359f
#define CLAMP(a,min,max) { if (a < min) a = min; if (a > max) a = max; }

void initSinTab(const int numSines, const int repeats, const int amplitude, int *mySinTab);
int getRand(int from, int to);

#endif
