#version 450 core

layout (location = 0) out vec4 FragColor;

void main() {
    // Renders the triangles solid cyan/greenish blue
    FragColor = vec4(0.0, 1.0, 0.8, 1.0); 
}