#include <cstdint>
#include <string>
#include <vector>


struct colour8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a = 1;
};


bool computeMandel(int sizex, int sizey, int maxIter, std::vector<colour8>& data, double offsetx, double offsety, double zoom, double gammaval);
std::string calcRawImg(int32_t width, int32_t height, const std::vector<colour8>& data);