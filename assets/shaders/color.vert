
#ifndef COLOR_VERT
#define COLOR_VERT

#pragma once

#version 450 core

#include <blinn_phong.frag>
#include "assets/shaders/helper.glsl"

layout (location = 0) in vec3 vertexPosition;

uniform mat4 modelTransform;
uniform mat4 cameraTransform;

void main() {
    gl_Position = cameraTransform /*Remove this*/* modelTransform * vec4(vertexPosition, 1.0);
}


#endif //COLOR_VERT

