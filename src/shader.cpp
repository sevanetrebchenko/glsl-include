
#include <shader.h>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <fstream>

namespace GLSL {

    Shader::Shader(std::string name, const std::initializer_list<std::string>& shaderComponentPaths) : _shaderName(std::move(name)),
                                                                                                       _shaderComponentPaths(shaderComponentPaths),
                                                                                                       _shaderID(-1) {
        CompileShader(GetShaderSources());
    }

    std::unordered_map<std::string, std::pair<GLenum, std::string>> Shader::GetShaderSources() {
        std::unordered_map<std::string, std::pair<GLenum, std::string>> shaderComponents;

        // Get shader types.
        std::for_each(_shaderComponentPaths.begin(), _shaderComponentPaths.end(), [&](const std::string& file) {
            std::size_t dotPosition = file.find_last_of('.');

            // Found extension.
            if (dotPosition != std::string::npos) {
                std::string shaderExtension = file.substr(dotPosition + 1);
                GLenum shaderType = ShaderTypeFromString(shaderExtension);

                if (shaderType == GL_INVALID_VALUE) {
                    throw std::runtime_error("Unknown or unsupported shader of type: \"" + shaderExtension + "\"");
                }

                shaderComponents.emplace(file, std::make_pair(shaderType, ReadFile(file)));
            }
            else {
                throw std::runtime_error("Could not find shader extension on file: \"" + file + "\"");
            }
        });

        return std::move(shaderComponents);
    }

    void Shader::Recompile() {
        CompileShader(GetShaderSources());
    }

    void Shader::CompileShader(const std::unordered_map<std::string, std::pair<GLenum, std::string>> &shaderComponents) {
        GLuint shaderProgram = glCreateProgram();
        unsigned numShaderComponents = shaderComponents.size();
        GLuint* shaders = new GLenum[numShaderComponents];
        unsigned currentShaderIndex = 0;

        //--------------------------------------------------------------------------------------------------------------
        // SHADER COMPONENT COMPILING
        //--------------------------------------------------------------------------------------------------------------
        for (auto& shaderComponent : shaderComponents) {
            // Compile shader - throws on error, not caught here.
            GLuint shader = CompileShaderComponent(shaderComponent);

            // Shader successfully compiled.
            glAttachShader(shaderProgram, shader);
            shaders[currentShaderIndex++] = shader;
        }

        //--------------------------------------------------------------------------------------------------------------
        // SHADER PROGRAM LINKING
        //--------------------------------------------------------------------------------------------------------------
        glLinkProgram(shaderProgram);

        GLint isLinked = 0;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);
        if (!isLinked) {
            // Shader failed to link - get error information from OpenGL.
            GLint errorMessageLength = 0;
            glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &errorMessageLength);

            std::vector<GLchar> errorMessageBuffer;
            errorMessageBuffer.resize(errorMessageLength + 1);
            glGetProgramInfoLog(shaderProgram, errorMessageLength, nullptr, &errorMessageBuffer[0]);
            std::string errorMessage(errorMessageBuffer.begin(), errorMessageBuffer.end());

            // Program is unnecessary at this point.
            glDeleteProgram(shaderProgram);

            // Delete shader types.
            for (int i = 0; i < numShaderComponents; ++i) {
                glDeleteShader(shaders[i]);
            }

            throw std::runtime_error("Shader: " + _shaderName + " failed to link. Provided error information: " + errorMessage);
        }

        // Shader has already been initialized, delete prior shader program.
        if (_shaderID != (GLuint)-1) {
            glDeleteProgram(_shaderID);
        }
        // Shader is successfully initialized.
        _shaderID = shaderProgram;

        // Clear previous shader uniform locations.
        _uniformLocations.clear();

        // Shader types are no longer necessary.
        for (int i = 0; i < numShaderComponents; ++i) {
            GLuint shaderComponentID = shaders[i];
            glDetachShader(shaderProgram, shaderComponentID);
            glDeleteShader(shaderComponentID);
        }
    }

    GLuint Shader::CompileShaderComponent(const std::pair<std::string, std::pair<GLenum, std::string>> &shaderComponent) {
        const std::string& shaderFilePath = shaderComponent.first;
        GLenum shaderType = shaderComponent.second.first;
        const GLchar* shaderSource = reinterpret_cast<const GLchar*>(shaderComponent.second.second.c_str());

        // Create shader from source.
        GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &shaderSource, nullptr); // If length is NULL, each string is assumed to be null terminated.
        glCompileShader(shader);

        // Compile shader source code.
        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (!isCompiled) {
            // Shader failed to compile - get error information from OpenGL.
            GLint errorMessageLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &errorMessageLength);

            std::vector<GLchar> errorMessageBuffer;
            errorMessageBuffer.resize(errorMessageLength + 1);
            glGetShaderInfoLog(shader, errorMessageLength, nullptr, &errorMessageBuffer[0]);
            std::string errorMessage(errorMessageBuffer.begin(), errorMessageBuffer.end());

            glDeleteShader(shader);
            throw std::runtime_error("Shader: " + _shaderName + " failed to compile " + ShaderTypeToString(shaderType) + " component (" + shaderFilePath + "). Provided error information: " + errorMessage);
        }

        return shader;
    }

    std::string Shader::ShaderTypeToString(GLenum shaderType) const {
        switch(shaderType) {
            case GL_FRAGMENT_SHADER:
                return "FRAGMENT";
            case GL_VERTEX_SHADER:
                return "VERTEX";
            case GL_GEOMETRY_SHADER:
                return "GEOMETRY";
            default:
                return "";
        }
    }

    std::string Shader::ReadFile(const std::string &filePath) {
        std::ifstream fileReader;

        // Open the file.
        fileReader.open(filePath);
        if (fileReader.is_open()) {
            std::stringstream file;

            // Process file.
            while (!fileReader.eof()) {
                // Get line.
                std::string line;
                std::getline(fileReader, line);

                // Parse line looking for include tokens.
                std::stringstream parser(line);
                std::string token;
                parser >> token;

                // Found include, include is the only thing that should be on this line.
                if (token == "#include") {
                    // Get filename to include.
                    parser >> token;

                    if (token.empty()) {
                        throw std::runtime_error("Empty #include pre-processor directive. Expected <filename> or \"filename\'.");
                    }

                    char beginning = token.front();
                    char end = token.back();

                    // Using system pre-designated include directory and any custom project include directories.
                    if (beginning == '<' && end == '>') {
                        std::string fileLocation = std::string(INCLUDE_DIRECTORY) + token;

                        try {
                            file << ReadFile(fileLocation);
                        }
                        // Include callstack.
                        catch (std::runtime_error& exception) {
                            throw std::runtime_error(std::string(exception.what()) + " Included from: " + fileLocation);
                        }
                    }
                    // Using current working directory.
                    else if (beginning == '"' && end == '"') {
                        try {
                            file << ReadFile(token);
                        }
                        // Include callstack.
                        catch (std::runtime_error& exception) {
                            throw std::runtime_error(std::string(exception.what()) + " Included from: " + token);
                        }
                    }
                    else {
                        throw std::runtime_error("Formatting mismatch. Expected <filename> or \"filename\'.");
                    }
                }
                else {
                    // Normal shader line, emplace entire line.
                    file << line;
                }
            }

            return file.str();
        }
        else {
            // Could not open file
            throw std::runtime_error("Could not open shader file: \"" + filePath + "\"");
        }
    }

    GLenum Shader::ShaderTypeFromString(const std::string &shaderExtension) {
        if (shaderExtension == "vert") {
            return GL_VERTEX_SHADER;
        }
        if (shaderExtension == "frag") {
            return GL_FRAGMENT_SHADER;
        }
        if (shaderExtension == "geom") {
            return GL_GEOMETRY_SHADER;
        }

        return GL_INVALID_VALUE;
    }

    Shader::~Shader() {
        glDeleteProgram(_shaderID);
    }

    void Shader::Bind() const {
        glUseProgram(_shaderID);
    }

    void Shader::Unbind() const {
        glUseProgram(0);
    }

    const std::string &Shader::GetName() const {
        return _shaderName;
    }

}