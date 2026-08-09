#version 450 core

layout (location = 0) in vec3 aPos;

layout (location = 0) uniform mat4 uProjection;

void main() {
    // Multiplies your (-1.77..1.77, -1.0..1.0) coordinates by the ortho matrix
    // mapping them directly into OpenGL's internal normalized clip space
    gl_Position = uProjection * vec4(aPos, 1.0);
}