#include "renderer.h"
#include "console.h"
#include "glad/gl.h"
#include <sstream>

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

    // Initialize GLAD by passing GLFW's function to get process addresses
    int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    if (!version) {
        console.log("Failed to initialize GLAD.\n");
        return false;
    }

    std::stringstream vss;
    vss << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version);
    console.log("Initialized OpenGL with GLAD. OpenGL version: " + vss.str());

    return true;
}

bool Renderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Renderer::update() {
    /* Render here */
    glClear(GL_COLOR_BUFFER_BIT);

    /* Swap front and back buffers */
    glfwSwapBuffers(window);

    /* Poll for and process events */
    glfwPollEvents();
}