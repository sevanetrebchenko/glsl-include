# PROJECT FILES
set(CORE_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/src/main.cpp"
        "${PROJECT_SOURCE_DIR}/src/shader.cpp"
        )

add_executable(glsl-include ${CORE_SOURCE_FILES})

target_include_directories(glsl-include PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_compile_definitions(glsl-include
        PRIVATE INCLUDE_DIRECTORY="${PROJECT_SOURCE_DIR}/include/"
        PRIVATE OUTPUT_DIRECTORY="${PROJECT_SOURCE_DIR}/data/runtime"
        )

# DEPENDENCIES
# Link OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED) # Ensure OpenGL exists on the system.
target_link_libraries(glsl-include OpenGL::GL)

# Link Glad.
# Glad gives access to both OpenGL extensions and the modern version of the core OpenGL API.
target_link_libraries(glsl-include glad)

# Link GLFW
# GLFW does not depend on OpenGL - OpenGL/Vulkan dependencies get loaded in at runtime.
target_link_libraries(glsl-include glfw)
