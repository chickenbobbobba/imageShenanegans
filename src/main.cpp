#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <mpfr.h>
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

void mandelIterate(double& zr, double& zi, double cr, double ci, double& zrsqu, double& zisqu) {
    zi = std::pow(zr + zi, 2) - zrsqu - zisqu;
    zi += ci;
    zr = zrsqu - zisqu + cr;
    zrsqu = zr * zr;
    zisqu = zi * zi;
}

HSVd computeMandelPosition(int a, int b, int sizex, int maxIter, double offsetx, double offsety, double zoom, double gammaval) {
    //mpfr_t cr ci zr zi zrsqu zisqu z2rsqu z2isqu z2r z2i;
    long bits = -std::log2(zoom/64);
    bits = bits/32 * 32 + 64;
    double cr = a / (sizex/zoom) + offsetx;
    double ci = b / (sizex/zoom) + offsety;
    double zr = 0;
    double zi = 0;
    double zrsqu;
    double zisqu;
    double z2rsqu;
    double z2isqu;
    double z2r = 0;
    double z2i = 0;
    int iter = 1;

    while (iter < maxIter) {
        mandelIterate(zr, zi, cr, ci, zrsqu, zisqu);
        if (zrsqu + zisqu > 4) {
            return {std::atan(zi/zr), 0.5*std::exp(-gammaval*iter), 1-std::exp(-gammaval*iter)};
        }
        iter++;
        
        mandelIterate(zr, zi, cr, ci, zrsqu, zisqu);
        if (zrsqu + zisqu > 4) {
            return {std::atan(zi/zr), 0.5*std::exp(-gammaval*iter), 1-std::exp(-gammaval*iter)};
        }
        iter++;

        mandelIterate(z2r, z2i, cr, ci, z2rsqu, z2isqu);

        if (z2r == zr && z2i == zi) return {std::atan(z2i/z2r), 0, 1};
    }
    return {std::atan(zi/zr), 0, 1};
    // return (int)(computeMandel(a-offset+0.2*randOffset(), b+randOffset(), offset * 1.33) * 0.8);
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

std::vector<colour8> computeMandelLine(int b, int sizex, int sizey, int maxIter, double offsetx, double offsety, double zoom, double gammaval) {
    std::vector<colour8> results;
    results.reserve(sizex);
    for (int i = 0; i < sizex; i++) {
        int x = i - sizex/2;
        int y = b - sizey/2;
        results.emplace_back(computeColour(computeMandelPosition(x, y, sizex, maxIter, offsetx, offsety, zoom, gammaval)));
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

bool computeMandel(int sizex, int sizey, int maxIter, std::vector<colour8>& data, double offsetx, double offsety, double zoom, double gammaval, ThreadPool& pool) {
    //std::cout << "starting render...\n";
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
