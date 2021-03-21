
#ifndef GLSL_INCLUDE_SHADER_H
#define GLSL_INCLUDE_SHADER_H

#include <glad/glad.h>
#include <string>
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include <set>
#include <stack>

namespace GLSL {

    class Shader {
        public:
            Shader(std::string name, const std::initializer_list<std::string>& shaderComponentPaths);
            ~Shader();

            void Bind() const;
            void Unbind() const;

//            void Recompile();

            [[nodiscard]] const std::string& GetName() const;

            template <typename DataType>
            void SetUniform(const std::string& uniformName, DataType value);

        private:
            struct ParsedShaderData {
                std::vector<std::string> _shaderComponentPaths;

                std::set<std::string> _includeGuardInstances;
                std::stack<std::pair<std::string, int>> _includeGuardStack; // Contains include guard name and line number it appears on.

                std::set<std::string> _pragmaInstances;
                std::stack<std::pair<std::string, int>> _pragmaStack; // Contains pragma name (filename) and line number it appears on.

                bool _hasVersionInformation;
                bool _processingExistingInclude;
            };

            std::string ReadFile(const std::string& filePath);
            std::string CondenseFile(std::string file) const;
            std::unordered_map<std::string, std::pair<GLenum, std::string>> GetShaderSources();
            void CompileShader(const std::unordered_map<std::string, std::pair<GLenum, std::string>>& shaderComponents);
            GLuint CompileShaderComponent(const std::pair<std::string, std::pair<GLenum, std::string>>& shaderComponent);

            std::string ShaderTypeToString(GLenum shaderType) const;
            GLenum ShaderTypeFromString(const std::string& shaderExtension);

            template <typename DataType>
            void SetUniformData(GLuint uniformLocation, DataType value);

            // Shader data.
            std::unordered_map<std::string, GLint> _uniformLocations;
            GLuint _shaderID;

            std::string _shaderName;
            ParsedShaderData _parseData;
    };

}

#include <shader.tpp>


#endif //GLSL_INCLUDE_SHADER_H
