
#ifndef GLSL_INCLUDE_SHADER_TPP
#define GLSL_INCLUDE_SHADER_TPP

#include <lib/glm/glm/glm.hpp>
#include <lib/glm/glm/gtc/type_ptr.hpp>

namespace GLSL {

    template <typename DataType>
    void Shader::SetUniform(const std::string& uniformName, DataType value) {
        auto uniformLocation = _uniformLocations.find(uniformName);

        // Location not found.
        if (uniformLocation == _uniformLocations.end()) {
            // Find location first.
            GLint location = glGetUniformLocation(_shaderID, uniformName.c_str());
            _uniformLocations.emplace(uniformName, location);

            SetUniformData(location, value);
        }
        else {
            SetUniformData(uniformLocation->second, value);
        }
    }

    template<typename DataType>
    void Shader::SetUniformData(GLuint uniformLocation, DataType value) {
        // BOOL, INT
        if constexpr (std::is_same_v<DataType, int> || std::is_same_v<DataType, bool>) {
            glUniform1i(uniformLocation, value);
        }
        // FLOAT
        else if constexpr (std::is_same_v<DataType, float>) {
            glUniform1f(uniformLocation, value);
        }
        // VEC2
        else if constexpr (std::is_same_v<DataType, glm::vec2>) {
            glUniform2fv(uniformLocation, 1, glm::value_ptr(value));
        }
        // VEC3
        else if constexpr (std::is_same_v<DataType, glm::vec3>) {
            glUniform3fv(uniformLocation, 1, glm::value_ptr(value));
        }
        // VEC4
        else if constexpr (std::is_same_v<DataType, glm::vec4>) {
            glUniform4fv(uniformLocation, 1, glm::value_ptr(value));
        }
        // MAT3
        else if constexpr (std::is_same_v<DataType, glm::mat3>) {
            glUniformMatrix3fv(uniformLocation, 1, GL_FALSE, glm::value_ptr(value));
        }
        // MAT4
        else if constexpr (std::is_same_v<DataType, glm::mat4>) {
            glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, glm::value_ptr(value));
        }
    }

}

#endif //GLSL_INCLUDE_SHADER_TPP