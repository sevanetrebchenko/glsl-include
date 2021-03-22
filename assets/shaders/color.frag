
#version 450 core

#include "assets/shaders/helper.glsl"

uniform vec3 surfaceColor;

layout(location = 0) out vec4 fragColor;

void main(void) {
    fragColor = vec4(surfaceColor, 1.0f);
}