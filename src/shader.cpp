
#include <shader.h>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>

namespace GLSL {

    Shader::Shader(std::string name, const std::initializer_list<std::string>& shaderComponentPaths) : _shaderName(std::move(name)),
                                                                                                       _shaderID(-1) {
        _parseData._shaderComponentPaths = shaderComponentPaths;
        _parseData._hasVersionInformation = false;
        _parseData._processingExistingInclude = false;

        CompileShader(GetShaderSources());
    }

    std::unordered_map<std::string, std::pair<GLenum, std::string>> Shader::GetShaderSources() {
        std::unordered_map<std::string, std::pair<GLenum, std::string>> shaderComponents;

        // Get shader types.
        std::for_each(_parseData._shaderComponentPaths.begin(), _parseData._shaderComponentPaths.end(), [&](const std::string& file) {
            std::size_t dotPosition = file.find_last_of('.');

            // Found extension.
            if (dotPosition != std::string::npos) {
                std::string shaderExtension = file.substr(dotPosition + 1);
                GLenum shaderType = ShaderTypeFromString(shaderExtension);

                if (shaderType == GL_INVALID_VALUE) {
                    throw std::runtime_error("Unknown or unsupported shader of type: \"" + shaderExtension + "\"");
                }

                std::string shaderFile = CondenseFile(ReadFile(file));

                // Write parsed shader to output directory.
                #ifdef OUTPUT_DIRECTORY
                    std::ofstream outputStream;

                    // Get only the asset name.
                    std::string assetName;

                    std::size_t winSlashPosition = file.find_last_of('\\');
                    std::size_t linSlashPosition = file.find_last_of('/');

                    if (winSlashPosition == std::string::npos) {
                        if (linSlashPosition == std::string::npos) {
                            // No slashes, filename is the asset name.
                            assetName = file;
                        }
                        else {
                            // No windows-style slashes, use linux-style.
                            assetName = file.substr(linSlashPosition + 1);
                        }
                    }
                    else {
                        if (linSlashPosition == std::string::npos) {
                            // No linux-style slashes, use windows-style.
                            assetName = file.substr(winSlashPosition + 1);
                        }
                        else {
                            // Both types of slashes, pick the furthest position.
                            assetName = file.substr(std::max(winSlashPosition, linSlashPosition) + 1);
                        }
                    }

                    std::string outputDirectory = std::string(OUTPUT_DIRECTORY);
                    char slash = outputDirectory.back();

                    if (slash != '\\' && slash != '/') {
                        #ifdef _WIN32
                            outputDirectory += '\\';
                        #else
                            outputDirectory += '/';
                        #endif
                    }

                    // Make output directory if it doesn't exist.
                    if (!std::filesystem::exists(outputDirectory)) {
                        std::filesystem::create_directories(outputDirectory);
                    }

                    outputStream.open(outputDirectory + assetName);
                    outputStream << shaderFile;
                    outputStream.close();
                #endif

                shaderComponents.emplace(file, std::make_pair(shaderType, shaderFile));

                // Reset.

                // If there are remaining include guards, there is a mismatch between opening and closing include guard scopes.
                if (!_parseData._includeGuardStack.empty()) {
                    std::string errorMessage;

                    while (!_parseData._includeGuardStack.empty()) {
                        const std::pair<std::string, int>& includeGuardData = _parseData._includeGuardStack.top();
                        errorMessage += std::string("Unterminated include guard (#ifndef " + includeGuardData.first + ") on line number: " + std::to_string(includeGuardData.second) + '\n');
                        _parseData._includeGuardStack.pop();
                    }

                    throw std::runtime_error(errorMessage);
                }

                _parseData._hasVersionInformation = false;
                _parseData._processingExistingInclude = false;
            }
            else {
                throw std::runtime_error("Could not find shader extension on file: \"" + file + "\"");
            }
        });

        return std::move(shaderComponents);
    }

//    void Shader::Recompile() {
//        CompileShader(GetShaderSources());
//    }

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
            int lineNumber = 1;

            // Process file.
            while (!fileReader.eof()) {
                // Get line.
                std::string line;
                std::getline(fileReader, line);
                std::string lineNumberString = std::to_string(lineNumber);

                // Parse line looking for include tokens.
                std::stringstream parser(line);
                std::string token;
                parser >> token;

                // Pragma once.
                if (token == "#pragma") {
                    std::string once;

                    // Ensure following token is 'once' (only pragma that is supported).
                    parser >> token;
                    if (token.empty() || token != "once") {
                        std::stringstream errorMessageBuilder;

                        errorMessageBuilder << "In file '" << filePath << "' on line " << lineNumberString << ": error: #pragma pre-processing directive must be followed by 'once'." << std::endl;
                        errorMessageBuilder << std::setw(4) << lineNumber << " |    " << line << std::endl;
                        errorMessageBuilder << std::setw(4) << ' ' << " |    " << "        ^" << std::endl;
                        throw std::runtime_error(errorMessageBuilder.str());
                    }

                    // Track this file for it to be only be included once.
                    if (_parseData._pragmaInstances.find(filePath) == _parseData._pragmaInstances.end()) {
                        _parseData._pragmaInstances.insert(filePath);
                        _parseData._pragmaStack.push(std::make_pair(filePath, lineNumber));
                    }
                    else {
                        _parseData._processingExistingInclude = true;
                    }
                }

                // Include guard.
                else if (token == "#ifndef") {
                    // Get include guard name.
                    std::string includeGuardName;
                    parser >> includeGuardName;

                    if (includeGuardName.empty()) {
                        throw std::runtime_error("Empty #ifndef pre-processor directive. Expected macro name.");
                    }

                    // Make sure file was not already included in this shader.
                    if (_parseData._includeGuardInstances.find(includeGuardName) == _parseData._includeGuardInstances.end()) {
                        _parseData._includeGuardInstances.insert(includeGuardName);
                        _parseData._includeGuardStack.push(std::make_pair(includeGuardName, lineNumber));

                        std::getline(fileReader, line); // Get line with the define.

                        // Get define name.
                        std::stringstream defineParser(line);
                        defineParser >> token >> token;

                        if (token != includeGuardName) {
                            std::cout << "Warning: include guard name (" << includeGuardName << ") does not match following macro definition (" << token << ")." << std::endl;
                        }
                    }
                    else {
                        _parseData._processingExistingInclude = true;
                    }
                }
                // Clear #endif.
                else if (token == "#endif") {
                    // Reached the end of this include guard, safe to include file lines once again.
                    if (_parseData._processingExistingInclude) {
                        _parseData._processingExistingInclude = false;
                    }
                    else {
                        if (!_parseData._includeGuardStack.empty()) {
                            _parseData._includeGuardStack.pop();
                        }
                            // If the include guard stack is empty, an endif was pushed without an existing #if / #ifndef.
                        else {
                            std::stringstream errorMessageBuilder;

                            errorMessageBuilder << "In file '" << filePath << "' on line " << lineNumberString << ": error: #endif pre-processor directive without preexisting #if / #ifndef directive." << std::endl;
                            errorMessageBuilder << std::setw(4) << lineNumber << " |    " << line << std::endl;
                            errorMessageBuilder << std::setw(4) << ' ' << " |    " << '^' << std::endl;
                            throw std::runtime_error(errorMessageBuilder.str());
                        }
                    }
                }

                // Found shader version number, version number is the only thing on this line.
                else if (token == "#version") {
                    // Skip additional shader versions if they appear. First version is the version of the shader.
                    if (!_parseData._hasVersionInformation) {
                        file << line << std::endl;
                        _parseData._hasVersionInformation = true;
                    }
                }

                // Found include, include is the only thing that should be on this line.
                else if (token == "#include") {
                    if (!_parseData._processingExistingInclude) {
                        // Get filename to include.
                        parser >> token;

                        if (token.empty()) {
                            throw std::runtime_error("Empty #include pre-processor directive. Expected <filename> or \"filename\'.");
                        }

                        char beginning = token.front();
                        char end = token.back();
                        std::string filename = token.substr(1, token.size() - 2);

                        // Using system pre-designated include directory and any custom project include directories.
                        if (beginning == '<' && end == '>') {
                            std::string fileLocation = std::string(INCLUDE_DIRECTORY) + filename;

                            try {
                                file << ReadFile(fileLocation);
                            }
                                // Include callstack.
                            catch (std::runtime_error& exception) {
                                throw std::runtime_error(std::string(exception.what()) + " Included from: " + filePath + ", line number: " + std::to_string(lineNumber));
                            }
                        }
                            // Using current working directory.
                        else if (beginning == '"' && end == '"') {
                            try {
                                file << ReadFile(filename);
                            }
                                // Include callstack.
                            catch (std::runtime_error& exception) {
                                throw std::runtime_error(std::string(exception.what()) + " Included from: " + filePath + ", line number: " + std::to_string(lineNumber));
                            }
                        }
                        else {
                            throw std::runtime_error("Formatting mismatch. Expected <filename> or \"filename\'.");
                        }
                    }
                }
                else {
                    // Normal shader line, emplace entire line.
                    // Need to manually emplace newline.
                    if (!_parseData._processingExistingInclude) {
                        file << line << std::endl;
                    }
                }

                ++lineNumber;
            }

            // #pragma one preprocessor directive pushes filename.
            if (_parseData._processingExistingInclude) {
                const std::pair<std::string, int>& pragmaData = _parseData._pragmaStack.top();

                // Ensure filename is the same as the current processed filename.
                // This file has been included the maximum one time in this shader unit.
                if (pragmaData.first == filePath) {
                    _parseData._processingExistingInclude = false;
                    _parseData._pragmaStack.pop();
                }

                // Otherwise, pragma is kept and will be processed as an error later.
            }

            return file.str();
        }
        else {
            // Could not open file
            throw std::runtime_error("Could not open shader file: \"" + filePath + "\"");
        }
    }

    std::string Shader::CondenseFile(std::string file) const {
        std::size_t previousItPosition = 0;
        bool previousIsNL = false;
        bool previousIsSlash = false;

        while (previousItPosition != file.size()) {
            // Clear newline if file begins with one.
            if (file.front() == '\n') {
                file.erase(file.begin());
                continue;
            }

            for (auto it = file.begin() + previousItPosition; it != file.end(); ++it, ++previousItPosition) {
                char character = *it;

                if (it == file.end() - 1) {
                    continue;
                }

                // Newline processing.
                if (character == '\n') {
                    // Remove duplicate newlines.
                    if (previousIsNL) {
                        file.erase(it);
                        break;
                    }
                    else {
                        previousIsNL = true;
                    }
                }
                else {
                    previousIsNL = false;
                }

                // Full-line comment processing.
                if (character == '/') {
                    if (previousIsSlash) {
                        std::size_t commentStart = previousItPosition - 1;
                        std::size_t commentEnd = previousItPosition;

                        while (file[commentEnd] != '\n') {
                            ++commentEnd;
                        }

                        file.erase(file.begin() + commentStart, file.begin() + commentEnd);

                        previousItPosition -= 1; // Reset iterator position to where the comment started.
                        previousIsNL = true;
                        previousIsSlash = false;
                        break;
                    }
                    else {
                        previousIsSlash = true;
                    }
                }
                // Inline comment processing.
                else if (character == '*') {
                    if (previousIsSlash) {
                        std::size_t commentStart = previousItPosition - 1;
                        std::size_t commentEnd = previousItPosition;

                        // Find */ to determine comment ending.
                        while (file[commentEnd] != '/' || file[commentEnd - 1] != '*') {
                            ++commentEnd;
                        }

                        file.erase(file.begin() + commentStart, file.begin() + commentEnd + 1);
                        previousItPosition -= 1; // Reset iterator position to where the comment started.
                    }
                }
                else {
                    previousIsSlash = false;
                }
            }
        }

        return file;
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