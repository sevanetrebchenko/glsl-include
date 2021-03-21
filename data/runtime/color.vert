#version 450 core
layout (location = 0) in vec3 vertexPosition;
uniform mat4 modelTransform;
uniform mat4 cameraTransform;
void main() {
    gl_Position = cameraTransform * modelTransform * vec4(vertexPosition, 1.0);
}
