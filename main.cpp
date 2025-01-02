#include <chrono>
#include <cmath>
#include <cstdlib>
#include <future>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "threadPool.h"

std::uint32_t sizex;
std::uint32_t sizey;
int maxIter;
int printThreshold;

std::string GetCurrentWorkingDir() {
    #ifdef __linux__
    char buff[FILENAME_MAX];
    getcwd(buff, FILENAME_MAX);
    #endif
    #ifdef __MINGW32__
    char buff[_MAX_PATH];
    getcwd(buff, FILENAME_MAX);
    #endif
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
    return 0;
    // return (int)(computeMandel(a-offset+0.2*randOffset(), b+randOffset(), offset * 1.33) * 0.8);
}

std::vector<int> computeMandelLine(int b) {
    std::vector<int> results;
    results.reserve(sizex);
    for (int i = 0; i < sizex; i++) {
        int x = i - sizex/2;
        int y = b - sizey/2;
        results.emplace_back(computeMandel(x, y, sizex / 75));
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

std::vector<colour8> computeMandel() {
    std::vector<colour8> data(sizex * sizey, {255, 255, 255});
    ThreadPool pool(std::thread::hardware_concurrency());

    std::vector<std::future<std::vector<int>>> iterMap(sizey); // Store futures sequentially
    std::vector<int> yMapping(sizey); // Track line indices for mapping

    int jumpCount = findJumpSize(sizey); // This jump evenly distributes the tasks across priorities
    std::cout << jumpCount << " gap\n";  // meaning the percentage reading doesnt have pauses in 
                                         // heavier sections, such as along white edges
    
    std::cout << "queueing tasks..." << std::endl;
    for (int x = 0; x < sizey; x++) {
        int y = (x * jumpCount) % sizey;
        iterMap[x] = pool.addTask([y]() { return computeMandelLine(y); });
        yMapping[x] = y; // Track the mapping of task index to line index
    }   std::cout << std::endl;

    for (int i = 0; i < sizey; ++i) {
        std::cout << "\rProgress: " << (100 - 100.0 * pool.getQueueSize() / sizey) << "%, " << pool.getQueueSize() << " tasks left          ";
        auto line = iterMap[i].get(); // Access futures sequentially
        int y = yMapping[i]; // Get the correct line index
        for (int x = 0; x < sizex; ++x) {
            std::uint8_t val = line[x];
            data[y * sizex + x] = {val, val, val}; // Map to the 1D array using the correct line index
        }
    }
    std::cout << std::endl;
    return data;
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
            std::cout << "(width/height/iterations): ";
            std::cin >> sizex; std::cin >> sizey; std::cin >> maxIter;
            std::cin.clear();
    }

    std::cout << sizex << " | " << sizey << " | " << maxIter << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto data = computeMandel();

    auto end = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "compressing and saving image..." << std::endl;
    #ifdef __linux__ // windows users can do it themselves
    writeRawImg(sizex, sizey, data, maxIter);
    system("cjxl mandel.ppm -d 0.0 -e 7 -v mandel.jxl && rm mandel.ppm");
    #endif
}
