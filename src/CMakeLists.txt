# PROJECT FILES
set(CORE_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/src/main.cpp"
        "${PROJECT_SOURCE_DIR}/src/shader.cpp"
        "${PROJECT_SOURCE_DIR}/src/util.cpp"
    )

add_executable(glsl-include ${CORE_SOURCE_FILES})

target_include_directories(glsl-include PUBLIC "${CMAKE_SOURCE_DIR}/include/")
target_compile_definitions(glsl-include
        PRIVATE INCLUDE_DIRECTORY="${PROJECT_SOURCE_DIR}/include/"
        PRIVATE GLSL_INCLUDE_DIRECTORY="${PROJECT_SOURCE_DIR}/assets/shaders/"
        PRIVATE OUTPUT_DIRECTORY="${PROJECT_SOURCE_DIR}/data/runtime/"
    )

# DEPENDENCIES
# OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED) # Ensure OpenGL exists on the system.
target_link_libraries(glsl-include OpenGL::GL)

target_link_libraries(glsl-include glad)
target_link_libraries(glsl-include glfw)
target_link_libraries(glsl-include glm)

