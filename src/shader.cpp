
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
        std::string outputDirectory = ConstructOutputDirectory();
        std::unordered_map<std::string, std::pair<GLenum, std::string>> shaderComponents;

        // Get shader types.
        std::for_each(_parseData._shaderComponentPaths.begin(), _parseData._shaderComponentPaths.end(), [&](const std::string& filepath) {
            std::size_t dotPosition = filepath.find_last_of('.');

            // Found extension.
            if (dotPosition != std::string::npos) {
                std::string shaderExtension = filepath.substr(dotPosition + 1);
                GLenum shaderType = ShaderTypeFromString(shaderExtension);

                if (shaderType == GL_INVALID_VALUE) {
                    throw std::runtime_error("Unknown or unsupported shader of type: \"" + shaderExtension + "\"");
                }

                std::string shaderFile = ReadFile(filepath);
                EraseNewlines(shaderFile, false); // Keep last newline to finish file with newline.

                // Write parsed shader to output directory only if it exists.
                #ifdef OUTPUT_DIRECTORY
                    std::ofstream outputStream;

                    // Get only the shader name from the full filepath.
                    std::string assetName = GetShaderFilename(filepath);

                    // Create file.
                    outputStream.open(outputDirectory + assetName);
                    outputStream << shaderFile;
                    outputStream.close();
                #endif

                shaderComponents.emplace(filepath, std::make_pair(shaderType, shaderFile));

                // Reset.
                for (IncludeGuard& includeGuard : _parseData._includeGuards) {
                    // Found unterminated include guard.
                    if (includeGuard._endifLineNumber == -1) {
                        ThrowFormattedError(includeGuard._includeGuardFile, includeGuard._includeGuardLine, std::to_string(includeGuard._includeGuardLineNumber), "Unterminated #ifndef directive.", 0);
                    }
                }

                // Clear parsing information for new shader component.
                _parseData.Clear();
            }
            else {
                throw std::runtime_error("Could not find shader extension on file: \"" + filepath + "\"");
            }
        });

        return std::move(shaderComponents);
    }

    void Shader::Recompile() {
        _parseData.Clear();
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
            int lineNumber = 1;

            // Process file.
            while (!fileReader.eof()) {
                std::string line = GetLine(fileReader);
                std::string lineNumberString = std::to_string(lineNumber);

                // Stringstream for parsing the line.
                std::stringstream parser(line);
                std::string token;
                parser >> token;

                // Pragma once.
                if (token == "#pragma") {
                    std::string once;

                    // Ensure following token is 'once' (only pragma that is supported).
                    parser >> token;
                    if (token.empty() || token != "once") {
                        ThrowFormattedError(filePath, line, lineNumberString, "#pragma pre-processing directive must be followed by 'once'.", 8);
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
                    std::string includeGuardName;
                    parser >> includeGuardName;

                    // #ifndef needs macro as name, otherwise throw an error.
                    if (includeGuardName.empty()) {
                        ThrowFormattedError(filePath, line, lineNumberString, "Empty #ifndef pre-processor directive. Expected macro name.", 8);
                    }

                    // Make sure include guard was not already found.
                    auto includeGuardIt = _parseData._includeGuardInstances.find(includeGuardName);
                    if (includeGuardIt == _parseData._includeGuardInstances.end()) {
                        // New include guard.
                        _parseData._includeGuards.emplace_back();

                        IncludeGuard& includeGuard = _parseData._includeGuards.back();
                        includeGuard._includeGuardFile = filePath;
                        includeGuard._includeGuardName = includeGuardName;
                        includeGuard._includeGuardLine = line;
                        includeGuard._includeGuardLineNumber = lineNumber;

                        // Start tracking new include guard.
                        _parseData._includeGuardInstances.emplace(includeGuardName);
                    }
                    else {
                        // Check if existing #ifndef has the include guard defined.
                        for (IncludeGuard& includeGuard : _parseData._includeGuards) {
                            // include guard has associated #define, this has already been included.
                            if (includeGuard._includeGuardName == includeGuardName && includeGuard._defineLineNumber != -1) {
                                _parseData._processingExistingInclude = true;
                            }
                        }
                    }
                }

                else if (token == "#define") {
                    if (!_parseData._processingExistingInclude) {
                        // Get define name.
                        std::string defineName;
                        parser >> defineName;

                        if (defineName.empty()) {
                            ThrowFormattedError(filePath, line, lineNumberString, "Empty #define pre-processor directive. Expected identifier.", 8);
                        }

                        auto includeGuardIt = _parseData._includeGuardInstances.find(defineName);
                        if (includeGuardIt != _parseData._includeGuardInstances.end()) {

                            // Set line on which define was found.
                            for (IncludeGuard& includeGuard : _parseData._includeGuards) {
                                if (includeGuard._includeGuardName == defineName) {
                                    includeGuard._defineLineNumber = lineNumber;
                                }
                            }
                        }
                        else {
                            // Regular define.
                            if (_parseData._hasVersionInformation) {
                                file << token << ' ' << defineName << std::endl;
                            }
                            // Shader version information must be the first compiled line of shader code.
                            else {
                                ThrowFormattedError(filePath, line, lineNumberString, "Version directive must be first statement and may not be repeated.", 0);
                            }
                        }
                    }
                }

                // Clear #endif.
                else if (token == "#endif") {
                    // Reached the end of this include guard, safe to include file lines once again.
                    if (_parseData._processingExistingInclude) {
                        _parseData._processingExistingInclude = false;
                    }
                    else {
                        // Get the last include guard without a set #endif line number (last unterminated include guard).
                        bool found = false;
                        for (auto includeGuardIt = _parseData._includeGuards.rbegin(); includeGuardIt != _parseData._includeGuards.rend(); ++includeGuardIt) {
                            IncludeGuard& includeGuard = *includeGuardIt;

                            if (includeGuard._endifLineNumber == -1) {
                                found = true;
                                includeGuard._endifLineNumber = lineNumber;
                                break;
                            }
                        }

                        // If the include guard stack is empty, an endif was pushed without an existing #if / #ifndef.
                        if (!found) {
                            ThrowFormattedError(filePath, line, lineNumberString, "#endif pre-processor directive without preexisting #if / #ifndef directive.", 0);
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
                        std::string includeName;
                        parser >> includeName;

                        if (includeName.empty()) {
                            ThrowFormattedError(filePath, line, lineNumberString, "Empty #include pre-processor directive. Expected <filename> or \"filename\".", 9);
                        }

                        char beginning = includeName.front();
                        char end = includeName.back();
                        std::string filename = includeName.substr(1, includeName.size() - 2);

                        // Using system pre-designated include directory and any custom project include directories.
                        if (beginning == '<' && end == '>') {
                            std::string fileLocation = std::string(INCLUDE_DIRECTORY) + filename;

                            try {
                                file << ReadFile(fileLocation);
                            }
                                // Include callstack.
                            catch (std::runtime_error& exception) {
                                throw std::runtime_error(std::string(exception.what()) + '\n' + "Included from: '" + filePath + "', line number: " + std::to_string(lineNumber));
                            }
                        }
                            // Using current working directory.
                        else if (beginning == '"' && end == '"') {
                            try {
                                file << ReadFile(filename);
                            }
                                // Include callstack.
                            catch (std::runtime_error& exception) {
                                throw std::runtime_error(std::string(exception.what()) + '\n' + "Included from: '" + filePath + "', line number: " + std::to_string(lineNumber));
                            }
                        }
                        else {
                            ThrowFormattedError(filePath, line, lineNumberString, "Formatting mismatch. Expected <filename> or \"filename\'.", 9);
                        }
                    }
                }
                else {
                    // Normal shader line, emplace entire line.
                    if (!_parseData._processingExistingInclude && _parseData._hasVersionInformation) {
                        file << line << std::endl; // Need to manually emplace newline.
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
            throw std::runtime_error("Could not open shader file: '" + filePath + "'");
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

    void Shader::EraseComments(std::string &line) const {
        std::size_t previousItPosition = 0;
        bool previousIsSlash = false;

        while (previousItPosition != line.size()) {
            for (auto it = line.begin() + previousItPosition; it != line.end(); ++it, ++previousItPosition) {
                char character = *it;

                // Full-line comment processing.
                if (character == '/') {
                    if (previousIsSlash) {
                        std::size_t commentStart = previousItPosition - 1;
                        std::size_t commentEnd = previousItPosition;

                        while (line[commentEnd] != '\n') {
                            ++commentEnd;
                        }

                        line.erase(line.begin() + commentStart, line.begin() + commentEnd);

                        previousItPosition -= 1; // Reset iterator position to where the comment started.
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
                        while (line[commentEnd] != '/' || line[commentEnd - 1] != '*') {
                            ++commentEnd;
                        }

                        line.erase(line.begin() + commentStart, line.begin() + commentEnd + 1);
                        previousItPosition -= 1; // Reset iterator position to where the comment started.
                    }
                }
                else {
                    previousIsSlash = false;
                }
            }
        }
    }

    void Shader::EraseNewlines(std::string &line, bool eraseLast) const {
        std::size_t previousItPosition = 0;
        bool previousIsNL = false;

        while (previousItPosition != line.size()) {
            // Clear newline if line begins with one.
            if (line.front() == '\n') {
                line.erase(line.begin());
                continue;
            }

            for (auto it = line.begin() + previousItPosition; it != line.end(); ++it, ++previousItPosition) {
                char character = *it;

                // Newline processing.
                if (character == '\n') {
                    // Remove duplicate newlines.
                    if (previousIsNL) {
                        line.erase(it);
                        break;
                    }
                    else {
                        previousIsNL = true;
                    }
                }
                else {
                    previousIsNL = false;
                }
            }
        }

        if (eraseLast) {
            if (line.back() == '\n') {
                line.erase(line.begin() + line.size() - 1);
            }
        }
    }

    std::string Shader::ConstructOutputDirectory() const {
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

        return std::move(outputDirectory);
    }

    std::string Shader::GetShaderFilename(const std::string& filepath) const {
        std::string assetName;

        std::size_t winSlashPosition = filepath.find_last_of('\\');
        std::size_t linSlashPosition = filepath.find_last_of('/');

        if (winSlashPosition == std::string::npos) {
            if (linSlashPosition == std::string::npos) {
                // No slashes, filename is the asset name.
                assetName = filepath;
            }
            else {
                // No windows-style slashes, use linux-style.
                assetName = filepath.substr(linSlashPosition + 1);
            }
        }
        else {
            if (linSlashPosition == std::string::npos) {
                // No linux-style slashes, use windows-style.
                assetName = filepath.substr(winSlashPosition + 1);
            }
            else {
                // Both types of slashes, pick the furthest position.
                assetName = filepath.substr(std::max(winSlashPosition, linSlashPosition) + 1);
            }
        }

        return std::move(assetName);
    }

    std::string Shader::GetLine(std::ifstream &stream) const {
        std::string line;

        std::getline(stream, line);
        line += '\n'; // getline consumes the newline.
        EraseComments(line);

        return std::move(line);
    }

    void Shader::ThrowFormattedError(std::string filename, std::string line, std::string lineNumberString, std::string errorMessage, int locationOffset) const {
        static std::stringstream errorMessageBuilder;
        errorMessageBuilder.str(std::string()); // Clear.

        // Clear all newlines.
        EraseNewlines(filename, true);
        EraseNewlines(line, true);
        EraseNewlines(lineNumberString, true);
        EraseNewlines(errorMessage, true);

        errorMessageBuilder << "In file '" << filename << "' on line " << lineNumberString << ": error: " << errorMessage << std::endl;
        errorMessageBuilder << std::setw(4) << lineNumberString << " |    " << line << std::endl;
        errorMessageBuilder << std::setw(4) << ' ' << " |    ";
        if (locationOffset > 0) {
            errorMessageBuilder << std::setw(locationOffset) << ' ';
        }
        errorMessageBuilder << '^';

        throw std::runtime_error(errorMessageBuilder.str());
    }

    Shader::IncludeGuard::IncludeGuard() : _includeGuardName(""),
                                           _includeGuardLineNumber(-1),
                                           _defineLineNumber(-1),
                                           _endifLineNumber(-1) {
    }

    void Shader::ParsedShaderData::Clear() {
        _includeGuards.clear();
        _includeGuardInstances.clear();

        _pragmaInstances.clear();

        while (!_pragmaStack.empty()) {
            _pragmaStack.pop();
        }

        _hasVersionInformation = false;
        _processingExistingInclude = false;
    }
}