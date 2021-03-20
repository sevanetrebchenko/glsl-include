
#ifndef COLOR_VERT
#define COLOR_VERT

#version 450 core

#include "assets/shaders/color.vert"

layout (location = 0) in vec3 vertexPosition;

uniform mat4 modelTransform;
uniform mat4 cameraTransform;

void main() {
    gl_Position = cameraTransform * modelTransform * vec4(vertexPosition, 1.0);
}

#endif //COLOR_VERT