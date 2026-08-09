#pragma once
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <vector>

#include "console.h"

#define FRAG_SHADER_PATH "shaders/shader.frag"
#define VERT_SHADER_PATH "shaders/shader.vert"

// #define ASPECT_RATIO 1.77f  // Or 16:9 in human terms

struct pos2d {
    float x, y;

    void mapScreenPositions(const double& xpos, const double& ypos, const int& m_width, const int& m_height);
};

/*
 * This is the central renderer that we are going with for this project.
 * Even though the name of the project is NeuralNet, its much more than just simple neural networks.
 * It's more "NEURAL network applications in infrastructure NETworks"
 */
class Renderer {
    public:
    Renderer(int height, Console& con);
    ~Renderer();

    // Prevent copying to prevent double-freeing resources
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Initialize the renderer
    bool init();

    // Check if we should close the OpenGL window or nah.
    bool shouldClose() const;

    // Swap buffers and poll events. Update the frame basically.
    void update();

    private:
    int m_width;
    int m_height;
    std::string m_title;
    Console& console;  // Borrowing this console, thanks
    GLFWwindow* window = nullptr;

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_shaderProgram = 0;

    size_t m_buffersize = 1000;
    std::vector<float> m_vertices;

    // Function to process input by polling
    void processInput();

    // Read, process, import shaders at runtime
    bool loadShaders();

    // Creates an Orthographic Projection Matrix for L=-1.77, R=1.77, B=-1.0, T=1.0
    void setOrthographicProjection(GLuint shaderProgram);

    // Function to process keyboard input by callback
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Function to process mouse input by callback
    static void mouseCallback(GLFWwindow *window, int button, int action, int mods);

    // Function to process mouse scroll by callback
    static void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Function to process mouse movement by callback
    static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
};