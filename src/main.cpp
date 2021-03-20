
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
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

    std::cout << "Vendor: " << (const char*)glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << (const char*)glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version: " << (const char*)glGetString(GL_VERSION) << std::endl;

    GLSL::Shader* singleColorShader;

    try {
        singleColorShader = new GLSL::Shader("SingleColor", { "assets/shaders/color.vert", "assets/shaders/color.frag" });
    }
    catch (std::runtime_error& exception) {
        std::cerr << exception.what() << std::endl;
        return 1;
    }

    // Main loop.
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return 0;
}
