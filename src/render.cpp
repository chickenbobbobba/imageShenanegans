#include "../src/glad/glad.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <future>
#include <gmp.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cassert>
#include <cmath>
#include <thread>
#include <vector>
#include "utils.h"
#include "render.h"
#include "main.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
mpf_t offsetx;
mpf_t offsety;
mpf_t zoom;
double gammaval = 0.01;
long long iters = 10000;
bool computeNewFrame;
long long unsigned int mandelFrameID = 0;

void runGraphicsEngine()
{
    mpf_init_set_d(offsetx, -0.75);
    mpf_init_set_d(offsety, 0);
    mpf_init_set_d(zoom, 4);
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        std::runtime_error("GLFW");
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        std::runtime_error("GLAD");
    }

    // build and compile our shader zprogram
    // -----------------------glfwSetWindowShouldClose(window, true);-------------
    Shader ourShader("../src/shaders/vertex.glsl", "../src/shaders/fragment.glsl");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
         1.0f, -1.0f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left 
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);


    // load and create a texture 
    // -------------------------
    unsigned int texture1, texture2;
    // texture 1
    // ---------
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1); 
     // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load image, create texture and generate mipmaps
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
    // The FileSystem::getPath(...) is part of the GitHub repository so we can find files on any IDE/platform; replace it with your own image path.
    unsigned char *data = stbi_load("../resources/container.jpg", &width, &height, &nrChannels, 0);
    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    // texture 2
    // ---------
    glGenTextures(1, &texture2);
    glBindTexture(GL_TEXTURE_2D, texture2);
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load image, create texture
    int scrwidth, scrheight;
    glfwGetWindowSize(window, &scrwidth, &scrheight);
    std::vector<colour8> data1(scrwidth * scrheight);

    int oldscrwidth, oldscrheight;

    ThreadPool pool(std::thread::hardware_concurrency());

    std::promise<bool> promise;
    std::future<bool> completed = promise.get_future();
    promise.set_value(true);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scrwidth, scrheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data1.data());

    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    // -------------------------------------------------------------------------------------------
    ourShader.use(); // don't forget to activate/use the shader before setting uniforms!
    ourShader.setInt("texture1", 0);
    ourShader.setInt("texture2", 1);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {   
        glfwGetWindowSize(window, &scrwidth, &scrheight);
        auto status = completed.wait_for(std::chrono::seconds(0));
        if (computeNewFrame == true) {
            computeNewFrame = false;
            if (!completed.get() == true) break;
            // At this point, ensure no tasks are still writing to data1.
            oldscrwidth = scrwidth;
            oldscrheight = scrheight;
            pool.purge();
            mandelFrameID++;
            completed = std::async(std::launch::async, [=, &data1, &pool]() {
                data1.resize(oldscrwidth*oldscrheight);
                auto result = computeMandel(oldscrwidth, oldscrheight, iters, data1, offsetx, offsety, zoom, gammaval, true, pool, mandelFrameID);
                return result;
            });
        }
        //dataCopy = data1;
        //dataCopy.resize(oldscrwidth*oldscrheight);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, oldscrwidth, oldscrheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data1.data());
        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // bind textures on corresponding texture units
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        // render container
        ourShader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
        computeNewFrame = true;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        iters *= 1.1;
        std::cout << "max iters: " << iters << std::endl;
        computeNewFrame = true;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        std::cout << "max iters: " << iters << std::endl;
        iters /= 1.1;
        computeNewFrame = true;
    }

}
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    mpf_t temp3, temp4, zoomFactor;
    mpf_init_set_d(zoomFactor, 10);
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mousexpos, mouseypos;
        int windowx, windowy;
        glfwGetCursorPos(window, &mousexpos, &mouseypos);
        glfwGetWindowSize(window, &windowx, &windowy);
        double temp1 = (mousexpos-windowx/2.0)/windowx;
        double temp2 = (mouseypos-windowy/2.0)/windowx;
        mpf_init_set_d(temp3, temp1);
        mpf_init_set_d(temp4, temp2);
        mpf_mul(temp3, zoom, temp3);
        mpf_mul(temp4, zoom, temp4);
        mpf_add(offsetx, offsetx, temp3);
        mpf_sub(offsety, offsety, temp4);

        mpf_div(zoom, zoom, zoomFactor);
        unsigned long prec_bits = mpf_get_prec(zoom);
        unsigned long digits = (unsigned long)std::ceil(prec_bits * 0.30103);
        gmp_printf("%.*Ff | %.*Ff | %#Fe\n", digits, offsetx, digits, offsety, zoom);
        computeNewFrame = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        mpf_mul(zoom, zoom, zoomFactor);
        unsigned long prec_bits = mpf_get_prec(zoom);
        unsigned long digits = (unsigned long)std::ceil(prec_bits * 0.30103);
        gmp_printf("%.*Ff | %.*Ff | %#Fe\n", digits, offsetx, digits, offsety, zoom);
        computeNewFrame = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        computeNewFrame = true;
    }
    double zoomd = mpf_get_d(zoom);
    long bitsl = -std::log2(zoomd);
    bitsl = (bitsl >= 0) * bitsl;
    bitsl = bitsl/32 * 32 + 64;
    mpf_set_default_prec(bitsl);

    mpf_set_prec(offsetx, bitsl);
    mpf_set_prec(offsety, bitsl);
    mpf_set_prec(zoom, bitsl);

    std::cout << "bits: " << bitsl << "\n";
}
void scroll_callback(GLFWwindow* window, double scrollxoffset, double scrollyoffset) {
    gammaval *= std::exp(scrollyoffset/20);
    std::cout << gammaval << std::endl;
}
// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
    computeNewFrame = true;
}
