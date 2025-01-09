#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <gmp.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <vector>
#include <unistd.h>
//#include <mpfr.h>
#include <gmpxx.h>
#include "render.h"
#include "main.h"

int printThreshold;

void writeRawImg(std::int32_t width, std::int32_t height, const std::vector<colour8>& data) {
    std::stringstream header;
    header << "P6 " << width << " " << height << " 255\n";

    std::ofstream file("mandel.ppm");
    file << header.str();
    for (int i = 0; i < data.size(); i++) {
        file.put(data[i].r);
        file.put(data[i].g);
        file.put(data[i].b);
    }
    file.close();
}

float randOffset(int sizex) {
    double randConst = (1-2*(double)rand()/(double)RAND_MAX);
    if (randConst > 0)
        return (sizex/600.0)*std::pow(3, randConst) - 1;
    else 
        return -(sizex/600.0)*std::pow(3, -randConst) + 1;
}

void mandelIterate(mpf_t zr, mpf_t zi, const mpf_t cr, const mpf_t ci, mpf_t zrsqu, mpf_t zisqu, mpf_t temp) {
    mpf_mul(zrsqu, zr, zr);
    mpf_mul(zisqu, zi, zi);

    mpf_add(temp, zr, zi);
    mpf_mul(temp, temp, temp);
    mpf_sub(temp, temp, zrsqu);
    mpf_sub(temp, temp, zisqu);

    mpf_sub(zr, zrsqu, zisqu);
    mpf_add(zr, zr, cr);
    mpf_add(zi, temp, ci);
}

HSVd computeMandelPosition(mpf_t cr, mpf_t ci, mpf_t zoom, long long maxIter, double gammaval, mpf_t temp) {
    mpf_t zr, zi, zrsqu, zisqu, z2rsqu, z2isqu, z2r, z2i, four, zero, inf, sixtyfour;
    double tempd;
    mpf_init_set_d(sixtyfour, 64);
    mpf_div(temp, zoom, sixtyfour);
    tempd = mpf_get_d(temp);
    long bits = -std::log2(tempd);
    bits = (bits >= 0) * bits;
    bits = bits/32 * 32 + 64;
    mpf_set_default_prec(bits);

    // Set initial values
    // mpf_init_set_d(cr, a / (sizex/zoom) + offsetx);
    // mpf_init_set_d(ci, b / (sizex/zoom) + offsety);
    mpf_init_set_d(zr, 0);
    mpf_init_set_d(zi, 0);
    mpf_init_set_d(zrsqu, 0);
    mpf_init_set_d(zisqu, 0);
    mpf_init_set_d(z2rsqu, 0);
    mpf_init_set_d(z2isqu, 0);
    mpf_init_set_d(z2r, 0);
    mpf_init_set_d(z2i, 0);
    mpf_init_set_d(four, 4);
    mpf_init_set_d(zero, 0);
    mpf_init_set_d(inf, 99999999999999999.9);
    
    long long iter = 0;
    HSVd result;
    
    while (iter < maxIter) {
        mandelIterate(zr, zi, cr, ci, zrsqu, zisqu, temp);
        mpf_add(temp, zrsqu, zisqu);
        if (mpf_cmp(temp, four) > 0) {
            if (mpf_cmp(zr, zero) != 0) mpf_div(temp, zi, zr);
            else mpf_set(temp, inf);
            tempd = mpf_get_d(temp);
            result = {std::atan(tempd), 0.5*std::exp(-gammaval*iter), 1-std::exp(-gammaval*iter)};
            goto cleanup;  // Jump to cleanup before returning
        }
        iter++;
        
        mandelIterate(zr, zi, cr, ci, zrsqu, zisqu, temp);
        mpf_add(temp, zrsqu, zisqu);
        if (mpf_cmp(temp, four) > 0) {
            if (mpf_cmp(zr, zero) != 0) mpf_div(temp, zi, zr);
            else mpf_set(temp, inf);
            tempd = mpf_get_d(temp);
            result = {std::atan(tempd), 0.5*std::exp(-gammaval*iter), 1-std::exp(-gammaval*iter)};
            goto cleanup;  // Jump to cleanup before returning
        }
        iter++;
        
        mandelIterate(z2r, z2i, cr, ci, z2rsqu, z2isqu, temp);
        if (mpf_cmp(z2r, zr) == 0 && mpf_cmp(z2i, zi) == 0) {
            if (mpf_cmp(z2r, zero) != 0) mpf_div(temp, z2i, z2r);
            else mpf_set(temp, inf);
            tempd = mpf_get_d(temp);
            result = {std::atan(tempd), 0, 1};
            goto cleanup;  // Jump to cleanup before returning
        }
    }
    
    if (mpf_cmp(zr, zero) != 0) mpf_div(temp, zi, zr);
    else mpf_set(temp, inf);
    tempd = mpf_get_d(temp);
    result = {std::atan(tempd), 0, 1};

cleanup:
    // Clear all MPFR variables in reverse order of initialization
    mpf_clear(four);
    mpf_clear(temp);
    mpf_clear(z2i);
    mpf_clear(z2r);
    mpf_clear(z2isqu);
    mpf_clear(z2rsqu);
    mpf_clear(zisqu);
    mpf_clear(zrsqu);
    mpf_clear(zi);
    mpf_clear(zr);
    mpf_clear(ci);
    mpf_clear(cr);
    
    return result;
}

colour8 HSVtoRGB(float h, float s, float v) {
    // Ensure h is within the range [0, 360)
    h = fmod(h * 360 / 3.14159265, 360.0f);
    if (h < 0) h += 360.0f;

    float c = v * s; // Chroma
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1)); // Second largest component
    float m = v - c; // Match value for adjustment

    float r, g, b;
    if (h < 60) {
        r = c; g = x; b = 0;
    } else if (h < 120) {
        r = x; g = c; b = 0;
    } else if (h < 180) {
        r = 0; g = c; b = x;
    } else if (h < 240) {
        r = 0; g = x; b = c;
    } else if (h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    // Convert to 8-bit and apply match value
    uint8_t red = static_cast<uint8_t>((r + m) * 255);
    uint8_t green = static_cast<uint8_t>((g + m) * 255);
    uint8_t blue = static_cast<uint8_t>((b + m) * 255);

    // Return the colour8 struct with full alpha (255)
    return { red, green, blue, 255 };
}

colour8 computeColour(HSVd hsv) {
    // static_cast<uint8_t>(255-255*std::exp(-gammaval*iter)),
    return {
        HSVtoRGB(hsv.h, hsv.s, hsv.v)
    };
}

std::vector<colour8> computeMandelLine(int b, int sizex, int sizey, long long maxIter, mpf_t offsetx, mpf_t offsety, mpf_t zoom, double gammaval) {
    std::vector<colour8> results;
    mpf_t x, y, temp;
    mpf_init_set_si(temp, 0);
    for (int i = 0; i < sizex; i++) {
        mpf_init_set_si(x, i - sizex/2);
        mpf_init_set_si(y, b - sizey/2);
        mpf_init_set_si(temp, sizex);
        mpf_div(temp, temp, zoom);
        mpf_div(x, x, temp);
        mpf_div(y, y, temp);
        mpf_add(x, x, offsetx);
        mpf_add(y, y, offsety);
        results.emplace_back(computeColour(computeMandelPosition(x, y, zoom, maxIter, gammaval, temp)));
    }
    return results;
}

int findJumpSize(int x) {
    int n = std::sqrt(x);
    while (std::gcd(n, x) != 1) {
        n++;
    }
    return n;
}

bool computeMandel(int sizex, int sizey, long long maxIter, std::vector<colour8>& data, mpf_t offsetx, mpf_t offsety, mpf_t zoom, double gammaval, ThreadPool& pool) {
    double zoomd = mpf_get_d(zoom);
    long bitsl = -std::log2(zoomd);
    bitsl = (bitsl >= 0) * bitsl;
    bitsl = bitsl/32 * 32 + 64;
    std::cout << "bits: " << bitsl << "\n";
    
    std::vector<std::future<std::vector<colour8>>> iterMap(sizey); // Store futures sequentially
    std::vector<int> yMapping(sizey); // Track line indices for mapping

    int jumpCount = findJumpSize(sizey); // This jump evenly distributes the tasks across priorities
                                         // meaning the percentage reading doesnt have pauses in 
                                         // heavier sections, such as along white edges
    for (int x = 0; x < sizey; x++) {
        int y = (x * jumpCount) % sizey;
        iterMap[x] = pool.addTask([y, sizex, sizey, maxIter, offsetx, offsety, zoom, gammaval]() { return computeMandelLine(y, sizex, sizey, maxIter, offsetx, offsety, zoom, gammaval); });
        yMapping[x] = y; // Track the mapping of task index to line index
    }   // std::cout << std::endl;

    for (int i = 0; i < sizey; ++i) {
        // std::cout << "\rProgress: " << (100 - 100.0 * pool.getQueueSize() / sizey) << "%, " << pool.getQueueSize() << " tasks left          ";
        auto line = iterMap[i].get(); // Access futures sequentially
        int y = yMapping[i]; // Get the correct line index
        for (int x = 0; x < sizex; ++x) {
            colour8 val = line[x];
            data[y * sizex + x] = val; // Map to the 1D array using the correct line index
        }
    }
    return true;
}

int main(int, char**) {
    std::cout << "Executing in " << std::filesystem::current_path() << "\n";

    std::uint32_t sizex;
    std::uint32_t sizey;
    int maxIter;
    std::cout << sizex << " | " << sizey << " | " << maxIter << std::endl;

    std::thread renderThread(runGraphicsEngine);
/*
    std::cout << "compressing and saving image..." << std::endl;
    #ifdef __linux__ // windows users can do it themselves
    writeRawImg(sizex, sizey, data, maxIter);
    system("cjxl mandel.ppm -d 0.0 -e 7 -v mandel.jxl && rm mandel.ppm");
    #endif
*/
    renderThread.join();
}
