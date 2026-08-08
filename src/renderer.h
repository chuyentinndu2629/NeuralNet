#pragma once
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include "console.h"

// #define ASPECT_RATIO 1.77f  // Or 16:9 in human terms

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

    bool init();
    bool shouldClose() const;
    void update(); // Swap buffers and poll events.

    private:
    int m_width;
    int m_height;
    std::string m_title;
    Console& console;  // Borrowing this console, thanks
    GLFWwindow* window = nullptr;
};