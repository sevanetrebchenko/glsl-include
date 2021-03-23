
#ifndef GLSL_INCLUDE_SHADER_H
#define GLSL_INCLUDE_SHADER_H

#include <glad/glad.h>
#include <string>
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include <set>
#include <stack>

#include <fstream>
#include <sstream>

namespace GLSL {

    class Shader {
        public:
            Shader(std::string shaderName, const std::initializer_list<std::string>& shaderComponentPaths);
            ~Shader();

            void Bind() const;
            void Unbind() const;

            void Recompile();

            // Add directory that will be checked when parsing #include statements in GLSL shader code.
            static void AddIncludeDirectory(std::string includeDirectory);

            [[nodiscard]] const std::string& GetName() const;

            template <typename DataType>
            void SetUniform(const std::string& uniformName, DataType value);

        private:
            class Parser {
                public:
                    Parser();
                    ~Parser();

                    std::string ProcessFile(const std::string& filepath);

                    // Returns true if all include guards are properly closed.
                    void ValidateIncludeGuardScope() const;

                private:
                    // Shader parsing.
                    struct IncludeGuard {
                        std::string _includeGuardFile;
                        std::string _includeGuardName;
                        std::string _includeGuardLine;
                        int _includeGuardLineNumber = -1;
                        int _defineLineNumber = -1;
                        int _endifLineNumber = -1;
                    };

                    std::string GetLine(std::ifstream& stream) const;

                    // Parsing #pragma pre-processor directive.
                    void PragmaDirective(const std::string &currentFile, const std::string& line, int lineNumber, const std::string& pragmaArgument);

                    // Parsing #ifndef pre-processor directive.
                    void OpenIncludeGuard(const std::string &currentFile, const std::string &line, int lineNumber, const std::string& includeGuardName);

                    // Parsing #define pre-processor directive.
                    bool DefineDirective(const std::string &currentFile, const std::string &line, int lineNumber, const std::string& defineName);

                    // Parsing #endif pre-processor directive.
                    void CloseIncludeGuard(const std::string &currentFile, const std::string &line, int lineNumber, const std::string& includeGuardName);

                    // Parsing #include pre-processor directive.
                    std::string IncludeFile(const std::string &currentFile, const std::string& line, int lineNumber, const std::string& fileToInclude);

                    [[nodiscard]] bool ValidateAgainst(const std::string& directiveName, const std::string& token) const;

                    // Throws error in the following format:
                    // In file '[filename]' on line [lineNumber]: error: [errorMessage]
                    // 5 |    [line]
                    //   |    [ locationOffset ]^
                    void ThrowFormattedError(std::string filename, std::string line, int lineNumber, std::string errorMessage, int locationOffset) const;

                    // Include guards.
                    std::vector<IncludeGuard> _includeGuards;
                    std::set<std::string> _includeGuardInstances;

                    // Pragmas.
                    std::set<std::string> _pragmaInstances;
                    std::stack<std::pair<std::string, int>> _pragmaStack; // Contains pragma filename and line number it appears on.

                    bool _hasVersionInformation;
                    bool _processingExistingInclude;
            };

            template <typename DataType>
            void SetUniformData(GLuint uniformLocation, DataType value);

            // Handles shader include guards and pragmas.
            std::string ProcessFile(const std::string& filepath);
            void WriteToOutputDirectory(const std::string& outputDirectory, const std::string& filepath, const std::string& shaderFile) const;

            // Processes input files to shader. Returns mapping of shader filepath to a pairing between the shader type and processed shader source.
            std::unordered_map<std::string, std::pair<GLenum, std::string>> GetShaderSources();

            // Compiles and links shader components into shader program. Throws std::runtime_error on error.
            void CompileShader(const std::unordered_map<std::string, std::pair<GLenum, std::string>>& shaderComponents);

            // Compiles shader component (vertex, fragment, etc.). Throws std::runtime_error on error.
            // Returns ID of compiled shader.
            GLuint CompileShaderComponent(const std::pair<std::string, std::pair<GLenum, std::string>>& shaderComponent);

            std::string ShaderTypeToString(GLenum shaderType) const;
            GLenum ShaderTypeFromString(const std::string& shaderExtension);

            static std::vector<std::string> _includeDirectories;

            std::unordered_map<std::string, GLint> _uniformLocations;
            GLuint _shaderID;

            std::string _shaderName;
            std::vector<std::string> _shaderComponentPaths;
    };

}

#include <shader.tpp>

#endif //GLSL_INCLUDE_SHADER_H
