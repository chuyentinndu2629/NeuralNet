#include "renderer.h"
#include "console.h"

#include <GLFW/glfw3.h>

Renderer::Renderer(int height, Console& con) : m_width(height * 1.77f), m_height(height), m_title("NeuralNet Monitor"), console(con) {

}

Renderer::~Renderer() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool Renderer::init() {
    if (!glfwInit()) {
        console.log("Failed to initialize GLFW\n", DEBUG_FAIL);
        return false;
    }

    // Disable window resizing entirely
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!window) {
        // Creating window failed
        console.log("Failed to create GLFW window\n", DEBUG_FAIL);
        return false;
    }
    // glfwSetWindowAspectRatio(window, 16, 9);
    // glfwSetWindowSizeLimits(window, 320, 180, GLFW_DONT_CARE, GLFW_DONT_CARE);
    
    glfwMakeContextCurrent(window);
    return true;
}

bool Renderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Renderer::update() {
    glClear(GL_COLOR_BUFFER_BIT); // Render clearing
    glfwSwapBuffers(window);  // Swap the buffers, get the new one in. C'mon!
    glfwPollEvents();
}