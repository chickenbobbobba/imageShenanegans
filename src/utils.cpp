#include <cassert>
#include <cstdarg>
#include <iostream>
#include <list>
#include <string>
#include <fstream>
#include <boost/any.hpp>
#include <vector>
#include "../src/glad/glad.h"

namespace util {
    std::string readFile(const std::string& filepath)
    {   
        std::ifstream file(filepath);
        std::string content;
        std::string line;

        while (std::getline(file, line)) {
            content += line + "\n";
        }

        file.close();
        return content;
    }
}

namespace glutil {
    
}

namespace uniformHandler {
    struct uniformType {
        unsigned int shader;
        const char* name;
        std::vector<boost::any> args;
    };

    std::list<uniformType> activeUniforms;

    unsigned int createUniform(unsigned int shader, const char* name) {
        int uniform = glGetUniformLocation(shader, name);
        // glGetUniformLocation returns -1 when a uniform cannot be found in a shader
        if (uniform == -1) {
            std::cout << "uniform " << std::to_string(*name) << " not found in shader " << std::to_string(shader) << std::endl;
            throw;
        }
        return uniform;
    }

    void assignUniformValue(unsigned int shader, const char* name, int count, ...) {
        assert(count > 0 && count <= 5);
        std::vector<boost::any> args;
        std::va_list list;
        va_start(list, count);

        // combine all args into a vector
        for (int i = 0; i < count; i++) {
            
            args.emplace_back(list[i]);
        }

        uniformType uniform = {shader, name, args};
        activeUniforms.push_back(uniform);
    }
};