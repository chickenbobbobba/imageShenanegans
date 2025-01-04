#include <string>

namespace util {
std::string readFile(const std::string& filename);
}
namespace glutil {

}
namespace uniformHandler {
    unsigned int createUniform(unsigned int shader, const char* name);
    void updateUniforms(unsigned int shader);
    void assignUniformValue(unsigned int shader, const char* name);
}