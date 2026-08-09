#include "renderer.h"
#include "console.h"
#include "glad/gl.h"

#include <GLFW/glfw3.h>
#include <sstream>
#include <fstream>

void pos2d::mapScreenPositions(const double& xpos, const double& ypos, const int& m_width, const int& m_height) {
    // Alrighty. Got a snippet
    // ss << "Mouse position: " << (2 * xpos - renderer->m_width) / renderer->m_height << ", " << (2 * ypos - renderer->m_height) / renderer->m_height;
    
    // Maps 0..m_width -> -1.77..+1.77
    x = (2.0f * xpos - m_width) / m_height; 
    
    // Maps 0..m_height -> +1.0..-1.0 (inverted for OpenGL Y-axis)
    y = (m_height - 2.0f * ypos) / m_height;
}

Renderer::Renderer(int height, Console& con) : m_width(height * 1.77f), m_height(height), m_title("NeuralNet Monitor"), console(con) {

}

Renderer::~Renderer() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);

    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool Renderer::init() {
    if (!glfwInit()) {
        console.log("Failed to initialize GLFW", DEBUG_FAIL);
        return false;
    }

    // Disable window resizing entirely
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!window) {
        // Creating window failed
        console.log("Failed to create GLFW window", DEBUG_FAIL);
        return false;
    }
    // glfwSetWindowAspectRatio(window, 16, 9);
    // glfwSetWindowSizeLimits(window, 320, 180, GLFW_DONT_CARE, GLFW_DONT_CARE);
    
    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, this); // For static objects to access the renderer instance

    // Initialize GLAD by passing GLFW's function to get process addresses
    int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    if (!version) {
        console.log("Failed to initialize GLAD.", DEBUG_FAIL);
        return false;
    }

    std::stringstream vss;
    vss << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version);
    console.log("Initialized OpenGL with GLAD. OpenGL version: " + vss.str());

    // Init setups.
    // Set key callback for keyboard input
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseCallback);
    glfwSetScrollCallback(window, mouseScrollCallback); 
    glfwSetCursorPosCallback(window, mouseMoveCallback);

    // 1. Generate VAO and VBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    // 2. Bind VAO first
    glBindVertexArray(m_vao);

    // 3. Bind VBO and allocate initial GPU capacity (e.g., reserve room for 100 floats)
    m_buffersize = 300; // room for 100 3D vertices
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_buffersize * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // 4. Configure attribute layout WHILE m_vbo IS BOUND
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Process shaders n shit
    if (!loadShaders()) {
        console.log("Failed to initialize shaders.", DEBUG_FAIL);
        return false;
    } else {
        console.log("Shaders loaded!", DEBUG_OK);
    }

    return true;
}

bool Renderer::loadShaders() {
    console.log("Setting up shaders...");

    // Processing VERTEX SHADER
    console.log(std::string("Reading Vertex Shader from ") + VERT_SHADER_PATH);
    std::ifstream vertexShaderSource(VERT_SHADER_PATH);
    if (!vertexShaderSource.is_open()) {
        console.log("Failure reading Vertex Shader source.", DEBUG_FAIL);
        return false;
    }

    std::stringstream vertexShaderBuffer;
    vertexShaderBuffer << vertexShaderSource.rdbuf();
    std::string vertexShaderDataTemp = vertexShaderBuffer.str();
    const char* vertexShaderData = vertexShaderDataTemp.c_str();
    
    console.log("Compiling Vertex Shader...");
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderData, nullptr);
    glCompileShader(vertexShader);

    // Processing FRAGMENT SHADER
    console.log(std::string("Reading Fragment Shader from ") + FRAG_SHADER_PATH);
    std::ifstream fragShaderSource(FRAG_SHADER_PATH);
    if (!fragShaderSource.is_open()) {
        console.log("Failure reading Fragment Shader source.", DEBUG_FAIL);
        return false;
    }

    std::stringstream fragShaderBuffer;
    fragShaderBuffer << fragShaderSource.rdbuf();
    std::string fragShaderDataTemp = fragShaderBuffer.str();
    const char *fragShaderData = fragShaderDataTemp.c_str();

    console.log("Compiling Fragment Shader...");
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragShaderData, nullptr);
    glCompileShader(fragmentShader);

    // Good. Now we link the whole thing
    console.log("Linking shaders...");
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    // Set up coordinate space mapping
    setOrthographicProjection(m_shaderProgram);

    // Clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

// Gemini wrote this. I don't understand a thing.
void Renderer::setOrthographicProjection(GLuint shaderProgram) {
    float left   = -1.77f;
    float right  =  1.77f;
    float bottom = -1.00f;
    float top    =  1.00f;
    float near   = -1.00f;
    float far    =  1.00f;

    // Standard Orthographic Projection Matrix (Column-Major for OpenGL)
    float orthoMatrix[16] = {
        2.0f / (right - left), 0.0f,                  0.0f,                 0.0f,
        0.0f,                  2.0f / (top - bottom), 0.0f,                 0.0f,
        0.0f,                  0.0f,                 -2.0f / (far - near),  0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f
    };

    // Find the location of the uniform in shader
    GLint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    
    // Upload matrix data to GPU
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, orthoMatrix);
}

bool Renderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Renderer::update() {
    /* Clear screen background */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Render */
    if (!m_vertices.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        // If current vector size exceeds GPU capacity, reallocate GPU memory (reserve new size)
        if (m_vertices.size() > m_buffersize) {
            m_buffersize = m_vertices.size() * 2; // Double capacity like std::vector
            glBufferData(GL_ARRAY_BUFFER, m_buffersize * sizeof(float), m_vertices.data(), GL_DYNAMIC_DRAW);
        } else {
            // Fast path: Update existing allocated GPU memory without reallocating
            glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertices.size() * sizeof(float), m_vertices.data());
        }

        // Actually using the shader program
        glUseProgram(m_shaderProgram);
        glBindVertexArray(m_vao);

        glDrawArrays(GL_TRIANGLES, 0, m_vertices.size() / 3);
    }

    /* Processing realtime input instead of callback cuz im not typing or using any notable key combs */
    processInput();

    /* Swap front and back buffers */
    glfwSwapBuffers(window);

    /* Poll for and process events */
    glfwPollEvents();
}

void Renderer::processInput() {
    // Whatever we do? This should be automatically removed once the compiler knows that this is USELESS
}

void Renderer::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Convert the pointer of the renderer instance into a pointer of class 'Renderer' so that we can use its methods.
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_Q) {
            renderer->console.log("Ctrl+Q. Exit the monitor");
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

void Renderer::mouseCallback(GLFWwindow *window, int button, int action, int mods) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            // Context menu
            // renderer->console.log("Context menu called!");
        } else if (button == GLFW_MOUSE_BUTTON_LEFT) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            pos2d mousePoint;
            mousePoint.mapScreenPositions(xpos, ypos, renderer->m_width, renderer->m_height);

            std::stringstream ss;
            ss << "Mouse position: " << mousePoint.x << ", " << mousePoint.y;

            // xpos: 0 -> m_width
            // ypos: 0 -> m_height
            // Therefore, to get a range from -1.0f to 1.0f, we need to divide m_width by 2.
            // Then do something like: (xpos - m_width) / (m_width / 2)

            renderer->console.log(ss.str());

            renderer->m_vertices.insert(
                renderer->m_vertices.end(),
                {
                    mousePoint.x - 0.1f, mousePoint.y - 0.1f, 0.0f,
                    mousePoint.x - 0.1f, mousePoint.y + 0.1f, 0.0f,
                    mousePoint.x + 0.1f, mousePoint.y + 0.1f, 0.0f,
                }
            );
        }
    }
}

void Renderer::mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));

    if (yoffset > 0) {
        // renderer->console.log("Scrolling up!");
    } else {
        // renderer->console.log("Scrolling down!");
    }
}

void Renderer::mouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));

    /*
    std::stringstream ss;
    ss << "Mouse position: " << xpos << "," << ypos;
    renderer->console.log(ss.str());
    */
}