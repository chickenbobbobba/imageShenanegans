#include <cstdint>
#include <string>
#include <vector>
#include "threadPool.h"
#include <gmpxx.h>


struct colour8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a = 1;
};

struct HSVd {
    double h;
    double s;
    double v;
};


bool computeMandel(int sizex, int sizey, long long maxIter, std::vector<colour8>& data, mpf_t offsetx, mpf_t offsety, mpf_t zoom, double gammaval, bool accurateColouring, ThreadPool& pool, long long unsigned int currentMandelFrameID);
std::string calcRawImg(int32_t width, int32_t height, const std::vector<colour8>& data);