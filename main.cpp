#include <chrono>
#include <cmath>
#include <cstdlib>
#include <future>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <set>
#include <vector>
#include <unistd.h>

#include "threadPool.h"

std::uint32_t sizex;
std::uint32_t sizey;
int maxIter;
int printThreshold;

std::string GetCurrentWorkingDir() {
    char buff[FILENAME_MAX]; // or use _MAX_PATH for Windows
    getcwd(buff, FILENAME_MAX); // or _getcwd(buff, FILENAME_MAX) for Windows
    return std::string(buff);
}

struct colour8 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

void writeRawImg(std::int32_t width, std::int32_t height, const std::vector<colour8>& data, int iterations) {
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

float randOffset() {
    double randConst = (1-2*(double)rand()/(double)RAND_MAX);
    if (randConst > 0)
        return (sizex/600.0)*std::pow(3, randConst) - 1;
    else 
        return -(sizex/600.0)*std::pow(3, -randConst) + 1;
}

void mandelIterate(double& zr, double& zi, double cr, double ci) {
    double zr2 = zr * zr;
    double zi2 = zi * zi;

    zi = 2 * zr * zi + ci;
    zr = zr2 - zi2 + cr;
}

int computeMandel(int a, int b, int offset = sizex/75) {
    double cr = a / (sizex/4.0) - 0.5;
    double ci = b / (sizex/4.0);
    double zr = 0;
    double zi = 0;
    double z2r = 0;
    double z2i = 0;
    int iter = 1;

    while (iter < maxIter) {
        mandelIterate(zr, zi, cr, ci);
        if (zr*zr + zi*zi > 4) {
            return 255-255*std::exp(-0.01*iter);
        }
        iter++;
        mandelIterate(zr, zi, cr, ci);
        if (zr*zr + zi*zi > 4) {
            return 255-255*std::exp(-0.01*iter);
        }
        iter++;

        mandelIterate(z2r, z2i, cr, ci);
        if (z2r == zr && z2i == zi) break;
    }
    //return 0;
    return (int)(computeMandel(a-offset+0.2*randOffset(), b+randOffset(), offset * 1.33) * 0.8);
}

std::vector<int> computeMandelLine(int b) {
    std::vector<int> results;
    for (int i = 0; i < sizex; i++) {
        int x = i - sizex/2;
        int y = b - sizey/2;
        results.emplace_back(computeMandel(x, y, sizex / 75));
    }
    return results;
}

int main(int, char**) {
    std::cout << "Executing in " << GetCurrentWorkingDir() << "\n";

    char preset;
    std::cout << "preset (1/2/3/4/5/6/c): ";
    std::cin >> preset;
    switch (preset) {
        case ('1'): sizex = 854; sizey = 480; maxIter = 50; break;
        case ('2'): sizex = 1280; sizey = 720; maxIter = 100; break;
        case ('3'): sizex = 1920; sizey = 1080; maxIter = 1000; break;
        case ('4'): sizex = 3840; sizey = 2160; maxIter = 2000; break;
        case ('5'): sizex = 7680; sizey = 4320; maxIter = 5000; break;
        case ('6'): sizex = 15360; sizey = 8640; maxIter = 5000; break;
        default:
            std::cin.clear();
            std::cout << "(width/height/iterations):";
            std::cin >> sizex; std::cin >> sizey; std::cin >> maxIter;
            std::cin.clear();
    }

    std::cout << sizex << " | " << sizey << " | " << maxIter << std::endl;

    printThreshold = std::sqrt(sizey);

    std::vector<colour8> data(sizex * sizey, {255, 255, 255});
    ThreadPool pool(std::thread::hardware_concurrency());

    std::vector<std::future<std::vector<int>>> iterMap;
    iterMap.reserve(sizey); // Each future represents a line.

    auto start = std::chrono::high_resolution_clock::now();
    for (int y = 0; y < sizey; ++y) {
        auto future = pool.addTask(
            [y]() { return computeMandelLine(y); }
        );
        iterMap.emplace_back(std::move(future));
    }

    // Process the results
    for (int y = 0; y < sizey; ++y) {
        std::cout << "\rProgress: " << (100.0 * y / sizey) << "%                ";
        auto line = iterMap[y].get(); // Get the computed line
        for (int x = 0; x < sizex; ++x) {
            std::uint8_t val = line[x];
            data[y * sizex + x] = {val, val, val}; // Map to the 1D array
        }
    }
    std::cout << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << time.count()/1000 << "ms\nsave image? (y/n): ";
    char saveAns;
    std::cin >> saveAns;
    #ifdef __linux__ 
    if (saveAns == 'y') {
        writeRawImg(sizex, sizey, data, maxIter);
        system("cjxl mandel.ppm -d 0.0 -e 7 -v mandel.jxl && rm mandel.ppm");
    }
    #endif
}
