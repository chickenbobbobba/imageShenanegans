#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <gmp.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <sstream>
#include <sys/types.h>
#include <thread>
#include <vector>
#include <unistd.h>
#include <gmpxx.h>
#include "render.h"
#include "main.h"

int printThreshold;
int errorcount = 0;

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

HSVd computeMandelPosition(mpf_t cr, mpf_t ci, mpf_t zoom, long long maxIter, double gammaval, mpf_t temp, bool accurateColouring) {
    mpf_t zr, zi, zrsqu, zisqu, z2rsqu, z2isqu, z2r, z2i, four, zero, inf, sixtyfour;
    double tempd;

    // calculate adequate bits for calculations 
    mpf_init_set_d(sixtyfour, 64);
    mpf_div(temp, zoom, sixtyfour);
    tempd = mpf_get_d(temp);
    long bits = -std::log2(tempd);
    bits = (bits >= 0) * bits;
    bits = bits/32 * 32 + 64;
    mpf_set_default_prec(bits);

    // Set initial values
    mpf_init_set_d(zr, 0.0);
    mpf_init_set_d(zi, 0.0);
    mpf_init_set_d(zrsqu, 0.0);
    mpf_init_set_d(zisqu, 0.0);
    mpf_init_set_d(z2rsqu, 0.0);
    mpf_init_set_d(z2isqu, 0.0);
    mpf_init_set_d(z2r, 0.0);
    mpf_init_set_d(z2i, 0.0);
    mpf_init_set_d(four, 4.0);
    mpf_init_set_d(zero, 0.0);
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
            goto cleanup;
        }
        iter++;
    }
    result = {0, 0, 0};

cleanup:
    // Clear all MPFR variables in reverse order of initialization
    mpf_clear(sixtyfour);
    mpf_clear(four);
    mpf_clear(inf);
    mpf_clear(zero);
    mpf_clear(z2i);
    mpf_clear(z2r);
    mpf_clear(z2isqu);
    mpf_clear(z2rsqu);
    mpf_clear(zisqu);
    mpf_clear(zrsqu);
    mpf_clear(zi);
    mpf_clear(zr);
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

void colourMandelScreenRegion(std::vector<colour8>& data, 
                            int boleftx, int bolefty, int toprightx, int toprighty, 
                            mpf_t zoom, int scrWidth, int scrHeight, double gammaval, 
                            bool accurateColouring, long long maxIter, ThreadPool& pool, 
                            mpf_t viewMidX, mpf_t viewMidY, int depth=0) {
    std::vector<colour8> results;
    mpf_t temp, cr, ci;
    mpf_init(cr);
    mpf_init(ci);
    mpf_init(temp);
    
    int midx = (boleftx + toprightx) / 2;
    int midy = (bolefty + toprighty) / 2;

    mpf_set_d(temp, (midx - scrWidth/2.0) / scrWidth);
    mpf_mul(temp, temp, zoom);
    mpf_add(cr, viewMidX, temp);
    mpf_set_d(temp, (midy - scrHeight/2.0) / scrWidth);
    mpf_mul(temp, temp, zoom);
    mpf_add(ci, viewMidY, temp);
    HSVd colour = computeMandelPosition(cr, ci, zoom, maxIter, gammaval, temp, accurateColouring);
    
    for (int i = bolefty; i < toprighty; i++) {
        for (int j = boleftx; j < toprightx; j++) {
            int index = i * scrWidth + j;
            if (index < data.size() && index >= 0) {  // Bounds check
                colour8 newColour = HSVtoRGB(colour.h, colour.s, colour.v);
                data[index].r = newColour.r;
                data[index].g = newColour.g;
                data[index].b = newColour.b;
            }
        }
    }
    
    int counts = 0;
    if (colour.v == 0 || accurateColouring) {
        int stepsx = std::pow(toprightx-boleftx, 0.5);
        int stepsy = std::pow(toprighty-bolefty, 0.5);
        for (int i = 0; i < stepsx; i++) {
            if (counts > 0) break;
            double scrxpos = std::lerp(boleftx, toprightx, (float)i/stepsx);
            mpf_set_d(temp, (scrxpos - scrWidth/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(cr, viewMidX, temp);
            mpf_set_d(temp, (bolefty - scrHeight/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(ci, viewMidY, temp);
            if (computeMandelPosition(cr, ci, zoom, maxIter, gammaval, temp, accurateColouring).v != colour.v) counts++;
        }
        for (int i = 0; i < stepsy; i++) {
            if (counts > 0) break;
            double scrypos = std::lerp(bolefty, toprighty, (float)i/stepsy);
            mpf_set_d(temp, (boleftx - scrWidth/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(cr, viewMidX, temp);
            mpf_set_d(temp, (scrypos - scrHeight/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(ci, viewMidY, temp);
            if (computeMandelPosition(cr, ci, zoom, maxIter, gammaval, temp, accurateColouring).v != colour.v) counts++;
        }
        for (int i = 0; i < stepsx; i++) {
            if (counts > 0) break;
            double scrxpos = std::lerp(boleftx, toprightx, (float)i/stepsx);
            mpf_set_d(temp, (scrxpos - scrWidth/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(cr, viewMidX, temp);
            mpf_set_d(temp, (toprighty - scrHeight/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(ci, viewMidY, temp);
            if (computeMandelPosition(cr, ci, zoom, maxIter, gammaval, temp, accurateColouring).v != colour.v) counts++;
        }
        for (int i = 0; i < stepsy; i++) {
            if (counts > 0) break;
            double scrypos = std::lerp(bolefty, toprighty, (float)i/stepsy);
            mpf_set_d(temp, (toprightx - scrWidth/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(cr, viewMidX, temp);
            mpf_set_d(temp, (scrypos - scrHeight/2.0) / scrWidth);
            mpf_mul(temp, temp, zoom);
            mpf_add(ci, viewMidY, temp);
            if (computeMandelPosition(cr, ci, zoom, maxIter, gammaval, temp, accurateColouring).v != colour.v) counts++;
        }
        if (counts == 0) {
            return;
        }
    }
    
    mpf_clear(cr);
    mpf_clear(ci);
    mpf_clear(temp);
    // Calculate midpoints using both boundaries
    depth++;
    if ((toprightx - boleftx) < 2 && (toprighty - bolefty) < 2) return;

    if (toprightx-boleftx >= 0.5*log(scrWidth * scrHeight)) {
        int priority = -depth;
        // bottom left
        pool.addTask([=, &data, &pool]() {
            colourMandelScreenRegion(data, boleftx, bolefty, midx, midy, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
        }, priority);
        //bottom right
        pool.addTask([=, &data, &pool]() {
            colourMandelScreenRegion(data, midx, bolefty, toprightx, midy, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
        }, priority);
        // top left
        pool.addTask([=, &data, &pool]() {
            colourMandelScreenRegion(data, boleftx, midy, midx, toprighty, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
        }, priority);
        //top right
        pool.addTask([=, &data, &pool]() {
            colourMandelScreenRegion(data, midx, midy, toprightx, toprighty, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
        }, priority);
    } else {
        if ((toprightx - boleftx) > (toprighty - bolefty)) {
            // Split vertically (left and right halves)
            colourMandelScreenRegion(data, boleftx, bolefty, midx, toprighty, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
            colourMandelScreenRegion(data, midx, bolefty, toprightx, toprighty, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
        } else {
            // Split horizontally (top and bottom halves)
            colourMandelScreenRegion(data, boleftx, bolefty, toprightx, midy, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
            colourMandelScreenRegion(data, boleftx, midy, toprightx, toprighty, zoom, scrWidth, scrHeight, 
                                    gammaval, accurateColouring, maxIter, pool, viewMidX, viewMidY, depth);
        }
    }
}

bool computeMandel(int sizex, int sizey, long long maxIter, std::vector<colour8>& data, mpf_t offsetx, mpf_t offsety, mpf_t zoom, double gammaval, bool accurateColouring, ThreadPool& pool) {
    double zoomd = mpf_get_d(zoom);
    long bitsl = -std::log2(zoomd);
    bitsl = (bitsl >= 0) * bitsl;
    bitsl = bitsl/32 * 32 + 64;
    std::cout << "bits: " << bitsl << "\n";

    for (auto i : data) {
        i = colour8{128, 128, 128};
    }

    colourMandelScreenRegion(data, 0, 0, sizex, sizey, zoom, sizex, sizey, gammaval, accurateColouring, maxIter, pool, offsetx, offsety);
    while (pool.getNumBusyThreads() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

int main(int, char**) {
    std::cout << "Executing in " << std::filesystem::current_path() << "\n";
    runGraphicsEngine();
}
