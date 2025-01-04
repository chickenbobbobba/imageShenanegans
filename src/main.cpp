//#include <chrono>
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

#include "threadPool.h"
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

void mandelIterate(double& zr, double& zi, double cr, double ci) {
    double zr2 = zr * zr;
    double zi2 = zi * zi;

    zi = 2 * zr * zi + ci;
    zr = zr2 - zi2 + cr;
}

int computeMandelPosition(int a, int b, int sizex, int maxIter, double offsetx, double offsety, double zoom, double gammaval) {
    double cr = a / (sizex/zoom) + offsetx;
    double ci = b / (sizex/zoom) + offsety;
    double zr = 0;
    double zi = 0;
    double z2r = 0;
    double z2i = 0;
    int iter = 1;

    while (iter < maxIter) {
        mandelIterate(zr, zi, cr, ci);
        if (zr*zr + zi*zi > 4) {
            return 255-255*std::exp(-gammaval*iter);
        }
        iter++;
        mandelIterate(zr, zi, cr, ci);
        if (zr*zr + zi*zi > 4) {
            return 255-255*std::exp(-gammaval*iter);
        }
        iter++;

        mandelIterate(z2r, z2i, cr, ci);
        if (z2r == zr && z2i == zi) return 0;
    }
    return 0;
    // return (int)(computeMandel(a-offset+0.2*randOffset(), b+randOffset(), offset * 1.33) * 0.8);
}

std::vector<int> computeMandelLine(int b, int sizex, int sizey, int maxIter, double offsetx, double offsety, double zoom, double gammaval) {
    std::vector<int> results;
    results.reserve(sizex);
    for (int i = 0; i < sizex; i++) {
        int x = i - sizex/2;
        int y = b - sizey/2;
        results.emplace_back(computeMandelPosition(x, y, sizex, maxIter, offsetx, offsety, zoom, gammaval));
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

bool computeMandel(int sizex, int sizey, int maxIter, std::vector<colour8>& data, double offsetx, double offsety, double zoom, double gammaval) {
    std::cout << "starting render...\n";
    ThreadPool pool(std::thread::hardware_concurrency());

    std::vector<std::future<std::vector<int>>> iterMap(sizey); // Store futures sequentially
    std::vector<int> yMapping(sizey); // Track line indices for mapping

    int jumpCount = findJumpSize(sizey); // This jump evenly distributes the tasks across priorities
                                         // meaning the percentage reading doesnt have pauses in 
                                         // heavier sections, such as along white edges
    for (int x = 0; x < sizey; x++) {
        int y = (x * jumpCount) % sizey;
        iterMap[x] = pool.addTask([y, sizex, sizey, maxIter, offsetx, offsety, zoom, gammaval]() { return computeMandelLine(y, sizex, sizey, maxIter, offsetx, offsety, zoom, gammaval); });
        yMapping[x] = y; // Track the mapping of task index to line index
    }   // std::cout << std::endl;

    std::cout << "tasks queued...\n";

    for (int i = 0; i < sizey; ++i) {
        // std::cout << "\rProgress: " << (100 - 100.0 * pool.getQueueSize() / sizey) << "%, " << pool.getQueueSize() << " tasks left          ";
        auto line = iterMap[i].get(); // Access futures sequentially
        int y = yMapping[i]; // Get the correct line index
        for (int x = 0; x < sizex; ++x) {
            std::uint8_t val = line[x];
            data[y * sizex + x] = {val, val, val}; // Map to the 1D array using the correct line index
        }
    }
    std::cout << "frame sent\n";
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
