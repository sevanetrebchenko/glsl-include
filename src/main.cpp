
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <shader.h>

int main() {
    // Initialize GLFW.
    int initializationCode = glfwInit();

    // Failed to initialize GLFW.
    if (!initializationCode) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return 1;
    }

    // Setting up OpenGL properties
    glfwWindowHint(GLFW_SAMPLES, 1); // change for anti-aliasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "OpenGL 4.6", nullptr, nullptr);

    // Failed to create GLFW window.
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        return 1;
    }

    // Initialize OpenGL.
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize Glad (OpenGL)." << std::endl;
        return 1;
    }

    // OpenGL properties.
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Initialize shader.
    GLSL::Shader* singleColorShader;

    try {
        singleColorShader = new GLSL::Shader("SingleColor", { "assets/shaders/color.vert", "assets/shaders/color.frag" });
    }
    catch (std::runtime_error& exception) {
        std::cerr << exception.what() << std::endl;
        return 1;
    }

    // Camera properties.
    glm::vec3 cameraEyePosition(0.0f, 2.0f, 4.0f);
    glm::vec3 upVector(0.0f, 1.0f, 0.0f);

    glm::mat4 cameraViewMatrix = glm::lookAt(cameraEyePosition, -cameraEyePosition, upVector);
    // Parameters: fov, aspect ratio, near plane distance, far plane distance
    glm::mat4 cameraPerspectiveMatrix = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    glm::mat4 cameraMatrix = cameraPerspectiveMatrix * cameraViewMatrix;

    // Initialize cube mesh.
    // Vertices.
    std::vector<glm::vec3> vertices {
        { -0.5f, -0.5f, -0.5f },
        { -0.5f, -0.5f, 0.5f },
        { -0.5f, 0.5f, -0.5f },
        { -0.5f, 0.5f, 0.5f },
        { 0.5f, -0.5f, -0.5f },
        { 0.5f, -0.5f, 0.5f },
        { 0.5f, 0.5f, -0.5f },
        { 0.5f, 0.5f, 0.5f },
    };
    // Faces.
    std::vector<unsigned> indices {
        // Left.
        0, 1, 3, 0, 3, 2,
        // Front.
        1, 5, 7, 1, 7, 3,
        // Top.
        3, 7, 6, 3, 6, 2,
        // Back.
        4, 0, 2, 4, 2, 6,
        // Right.
        5, 4, 6, 5, 6, 7,
        // Bottom.
        0, 4, 5, 0, 5, 1
    };

    // Initialize OpenGL buffers.
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    // Data for cube vertices.
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    // Vertex position attribute, location 0.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    // Data for cube indices (indexed drawing).
    unsigned indexCount = indices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // Cube transform.
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
    glm::mat4 scale = glm::scale(glm::vec3(1.0f));

    float rotationAngle = 0.0f;
    float previousFrameTime = 0;
    float dt, currentFrameTime;

    // Main loop.
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0) {
        glClearColor(0.0f, 20.0f / 255.0f, 40.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // dt calculations.
        currentFrameTime = (float)glfwGetTime();
        dt = currentFrameTime - previousFrameTime;
        previousFrameTime = currentFrameTime;

        // Update cube transform.
        rotationAngle += 20.0f * dt;
        glm::mat4 rotation = glm::rotate(glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 modelMatrix = translation * rotation * scale;

        // Pass shader uniforms.
        singleColorShader->Bind();
        singleColorShader->SetUniform("modelTransform", modelMatrix);
        singleColorShader->SetUniform("cameraTransform", cameraMatrix);
        singleColorShader->SetUniform("surfaceColor", glm::vec3(1.0f, 0.45f, 0.0f));

        // Render cube.
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        singleColorShader->Unbind();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup.
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);

    return 0;
}
