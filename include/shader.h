
#ifndef GLSL_INCLUDE_SHADER_H
#define GLSL_INCLUDE_SHADER_H

#include <glad/glad.h>
#include <string>
#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace GLSL {

    class Shader {
        public:
            Shader(std::string name, const std::initializer_list<std::string>& shaderComponentPaths);
            ~Shader();

            void Bind() const;
            void Unbind() const;

            void Recompile();

            [[nodiscard]] const std::string& GetName() const;

            template <typename DataType>
            void SetUniform(const std::string& uniformName, DataType value);

        private:
            std::string ReadFile(const std::string& filePath);
            std::unordered_map<std::string, std::pair<GLenum, std::string>> GetShaderSources();
            void CompileShader(const std::unordered_map<std::string, std::pair<GLenum, std::string>>& shaderComponents);
            std::string ShaderTypeToString(GLenum shaderType) const;
            GLenum ShaderTypeFromString(const std::string& shaderExtension);
            GLuint CompileShaderComponent(const std::pair<std::string, std::pair<GLenum, std::string>>& shaderComponent);

            std::vector<std::string> _shaderComponentPaths;
            std::unordered_map<std::string, GLint> _uniformLocations;
            std::string _shaderName;
            GLuint _shaderID;
    };

}

#include <shader.tpp>

#endif //GLSL_INCLUDE_SHADER_H