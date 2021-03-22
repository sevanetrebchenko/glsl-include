
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

            void Recompile();

            [[nodiscard]] const std::string& GetName() const;

            template <typename DataType>
            void SetUniform(const std::string& uniformName, DataType value);

        private:
            struct IncludeGuard {
                IncludeGuard();

                std::string _includeGuardFile;
                std::string _includeGuardName;
                std::string _includeGuardLine;
                int _includeGuardLineNumber;
                int _defineLineNumber;
                int _endifLineNumber;
            };

            struct ParsedShaderData {
                void Clear();

                std::vector<std::string> _shaderComponentPaths;

                std::vector<IncludeGuard> _includeGuards;
                std::set<std::string> _includeGuardInstances;

                std::set<std::string> _pragmaInstances;
                std::stack<std::pair<std::string, int>> _pragmaStack; // Contains pragma name (filename) and line number it appears on.

                bool _hasVersionInformation;
                bool _processingExistingInclude;
            };

            std::string ReadFile(const std::string& filePath);
            std::unordered_map<std::string, std::pair<GLenum, std::string>> GetShaderSources();
            void CompileShader(const std::unordered_map<std::string, std::pair<GLenum, std::string>>& shaderComponents);
            GLuint CompileShaderComponent(const std::pair<std::string, std::pair<GLenum, std::string>>& shaderComponent);

            std::string ShaderTypeToString(GLenum shaderType) const;
            GLenum ShaderTypeFromString(const std::string& shaderExtension);

            std::string GetLine(std::ifstream& stream) const;
            void EraseComments(std::string& line) const;
            void EraseNewlines(std::string &line, bool eraseLast) const;

            std::string ConstructOutputDirectory() const;
            std::string GetShaderFilename(const std::string& filepath) const;

            template <typename DataType>
            void SetUniformData(GLuint uniformLocation, DataType value);

            void ThrowFormattedError(std::string filename, std::string line, std::string lineNumber, std::string errorMessage, int locationOffset) const;

            // Shader data.
            std::unordered_map<std::string, GLint> _uniformLocations;
            GLuint _shaderID;

            std::string _shaderName;
            ParsedShaderData _parseData;
    };

}

#include <shader.tpp>


#endif //GLSL_INCLUDE_SHADER_H
