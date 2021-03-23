
#include <shader.h>
#include <util.h>

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <utility>
#include <filesystem>

namespace GLSL {

    // Static initialization.
    std::vector<std::string> Shader::_includeDirectories { };

    Shader::Shader(std::string name, const std::initializer_list<std::string>& shaderComponentPaths) : _shaderName(std::move(name)),
                                                                                                       _shaderID(-1),
                                                                                                       _shaderComponentPaths(shaderComponentPaths) {
        CompileShader(GetShaderSources());
    }

    std::unordered_map<std::string, std::pair<GLenum, std::string>> Shader::GetShaderSources() {
        std::string outputDirectory = CreateDirectory(std::string(OUTPUT_DIRECTORY));
        std::unordered_map<std::string, std::pair<GLenum, std::string>> shaderComponents;

        // Get shader types.
        std::for_each(_shaderComponentPaths.begin(), _shaderComponentPaths.end(), [&](const std::string& filepath) {
            std::size_t dotPosition = filepath.find_last_of('.');

            // Found extension.
            if (dotPosition != std::string::npos) {
                std::string shaderExtension = filepath.substr(dotPosition + 1);
                GLenum shaderType = ShaderTypeFromString(shaderExtension);

                if (shaderType == GL_INVALID_VALUE) {
                    throw std::runtime_error("Unknown or unsupported shader of type: \"" + shaderExtension + "\"");
                }

                std::string shaderFile = ProcessFile(filepath);

                #ifdef OUTPUT_DIRECTORY
                    WriteToOutputDirectory(outputDirectory, filepath, shaderFile);
                #endif

                shaderComponents.emplace(filepath, std::make_pair(shaderType, shaderFile));
            }
            else {
                throw std::runtime_error("Could not find shader extension on file: \"" + filepath + "\"");
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
        switch (shaderType) {
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

    std::string Shader::ProcessFile(const std::string &filepath) {
        Parser parser;

        std::string processedShaderSource = parser.ProcessFile(filepath);
        EraseNewlines(processedShaderSource, false);
        parser.ValidateIncludeGuardScope();

        return std::move(processedShaderSource);
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

    void Shader::WriteToOutputDirectory(const std::string& outputDirectory, const std::string& filepath, const std::string& shaderFile) const {
        std::ofstream outputStream;

        // Get only the shader name from the full filepath.
        std::string assetName = GetAssetName(filepath);

        // Create file.
        outputStream.open(outputDirectory + assetName);
        outputStream << shaderFile;
        outputStream.close();
    }

    void Shader::AddIncludeDirectory(std::string includeDirectory) {
        char slash = includeDirectory.back();

        // Make sure directory is terminated with a slash.
        if (slash != '\\' && slash != '/') {
            #ifdef _WIN32
                includeDirectory += '\\';
            #else
                includeDirectory += '/';
            #endif
        }

        _includeDirectories.emplace_back(includeDirectory);
    }

    Shader::Parser::Parser() : _hasVersionInformation(false),
                               _processingExistingInclude(false) {
    }

    Shader::Parser::~Parser() {
        _includeGuards.clear();
        _includeGuardInstances.clear();

        _pragmaInstances.clear();

        while (!_pragmaStack.empty()) {
            _pragmaStack.pop();
        }

        _hasVersionInformation = false;
        _processingExistingInclude = false;
    }

    std::string Shader::Parser::ProcessFile(const std::string &filepath) {
        std::ifstream fileReader;

        // Open the file.
        fileReader.open(filepath);
        if (fileReader.is_open()) {
            std::stringstream file;
            int lineNumber = 1;

            // Process file.
            while (!fileReader.eof()) {
                std::string line = GetLine(fileReader);

                // Stringstream for parsing the line.
                std::stringstream parser(line);
                std::string token;
                parser >> token;

                // Pragma.
                if (token == "#pragma") {
                    // Get token following #pragma directive.
                    parser >> token;
                    PragmaDirective(filepath, line, lineNumber, token);
                }

                // Open include guard.
                else if (token == "#ifndef") {
                    // Get include guard name.
                    parser >> token;
                    OpenIncludeGuard(filepath, line, lineNumber, token);
                }

                // Define (macro or include guard).
                else if (token == "#define") {
                    // Get define name.
                    parser >> token;
                    bool regularDefine = DefineDirective(filepath, line, lineNumber, token);

                    // Define does not belong to an include guard, include in final shader file.
                    if (regularDefine) {
                        file << line << std::endl;
                    }
                }

                // Close include guard.
                else if (token == "#endif") {
                    parser >> token;
                    CloseIncludeGuard(filepath, line, lineNumber, token);
                }

                // GLSL shader version.
                else if (token == "#version") {
                    // Skip additional shader versions if they appear. First version is the version of the shader.
                    if (!_hasVersionInformation) {
                        file << line << std::endl;
                        _hasVersionInformation = true;
                    }
                }

                // Include external file.
                else if (token == "#include") {
                    parser >> token; // Get filename to include;
                    IncludeFile(filepath, line, lineNumber, token);
                }

                // Normal shader line, emplace entire line.
                else {
                    if (!_processingExistingInclude && _hasVersionInformation) {
                        file << line << std::endl;
                    }
                }

                ++lineNumber;
            }

            // #pragma one preprocessor directive pushes filename.
            if (_processingExistingInclude) {
                const std::pair<std::string, int>& pragmaData = _pragmaStack.top();

                // Ensure filename is the same as the current processed filename.
                // This file has been included the maximum one time in this shader unit.
                if (pragmaData.first == filepath) {
                    _processingExistingInclude = false;
                    _pragmaStack.pop();
                }

                // Otherwise, pragma is kept and will be processed as an error later.
            }

            return file.str();
        }
        else {
            // Could not open file
            throw std::runtime_error("Could not open shader file: '" + filepath + "'");
        }
    }

    std::string Shader::Parser::GetLine(std::ifstream &stream) const {
        std::string line;

        std::getline(stream, line);
        line += '\n'; // getline consumes the newline.
        EraseComments(line);
        EraseNewlines(line, true);

        return std::move(line);
    }

    std::string Shader::Parser::IncludeFile(const std::string &currentFile, const std::string& line, int lineNumber, const std::string& fileToInclude) {
        if (!_processingExistingInclude) {
            if (ValidateAgainst("#include", fileToInclude)) {
                ThrowFormattedError(currentFile, line, lineNumber, "Empty #include pre-processor directive. Expected <filename> or \"filename\".", 9);
            }

            char beginning = fileToInclude.front();
            char end = fileToInclude.back();
            std::string filename = fileToInclude.substr(1, fileToInclude.size() - 2);

            // Using system pre-designated include directory and any custom project include directories.
            if (beginning == '<' && end == '>') {
                for (const std::string& directory : _includeDirectories) {
                    std::string fileLocation = directory + filename;

                    // File exists.
                    if (std::filesystem::is_regular_file(fileLocation)) {
                        try {
                            return ProcessFile(fileLocation);
                        }
                            // Include callstack.
                        catch (std::runtime_error& exception) {
                            throw std::runtime_error(std::string(exception.what()) + '\n' + "Included from: '" + currentFile + "', line number: " + std::to_string(lineNumber));
                        }
                    }
                }

                // File was not found in any of the provided include directories.
                ThrowFormattedError(currentFile, line, lineNumber, "File '" + filename + "' was not found in the provided include directories.", 9);
            }
                // Using current working directory.
            else if (beginning == '"' && end == '"') {
                try {
                    return ProcessFile(filename);
                }
                    // Include callstack.
                catch (std::runtime_error& exception) {
                    throw std::runtime_error(std::string(exception.what()) + '\n' + "Included from: '" + currentFile + "', line number: " + std::to_string(lineNumber));
                }
            }
            else {
                ThrowFormattedError(currentFile, line, lineNumber, "Formatting mismatch. Expected <filename> or \"filename\'.", 9);
            }
        }

        // Encountered include while processing already included file, include nothing.
        return "";
    }

    void Shader::Parser::PragmaDirective(const std::string &currentFile, const std::string &line, int lineNumber, const std::string& pragmaArgument) {
        if (ValidateAgainst("#pragma", pragmaArgument)) {
            ThrowFormattedError(currentFile, line, lineNumber, "#pragma pre-processing directive must be followed by 'once'.", 8);
        }

        // Track this file for it to be only be included once.
        if (_pragmaInstances.find(currentFile) == _pragmaInstances.end()) {
            _pragmaInstances.insert(currentFile);
            _pragmaStack.push(std::make_pair(currentFile, lineNumber));
        }
        else {
            // File has already been included.
            _processingExistingInclude = true;
        }
    }

    void Shader::Parser::OpenIncludeGuard(const std::string &currentFile, const std::string &line, int lineNumber, const std::string &includeGuardName) {
        // #ifndef needs macro as name, otherwise throw an error.
        if (ValidateAgainst("#ifndef", includeGuardName)) { // Include guard token is #ifndef when there is no token after the original #ifndef.
            ThrowFormattedError(currentFile, line, lineNumber, "Empty #ifndef pre-processor directive. Expected macro name.", 8);
        }

        // Make sure include guard was not already found.
        auto includeGuardIt = _includeGuardInstances.find(includeGuardName);
        if (includeGuardIt == _includeGuardInstances.end()) {
            // New include guard.
            _includeGuards.emplace_back();

            IncludeGuard& includeGuard = _includeGuards.back();
            includeGuard._includeGuardFile = currentFile;
            includeGuard._includeGuardName = includeGuardName;
            includeGuard._includeGuardLine = line;
            includeGuard._includeGuardLineNumber = lineNumber;

            // Start tracking new include guard.
            _includeGuardInstances.emplace(includeGuardName);
        }
        else {
            // Check if existing #ifndef has the include guard defined.
            for (IncludeGuard& includeGuard : _includeGuards) {
                // include guard has associated #define, this has already been included.
                if (includeGuard._includeGuardName == includeGuardName && includeGuard._defineLineNumber != -1) {
                    _processingExistingInclude = true;
                }
            }
        }
    }

    bool Shader::Parser::DefineDirective(const std::string &currentFile, const std::string &line, int lineNumber, const std::string &defineName) {
        if (!_processingExistingInclude) {
            if (ValidateAgainst("#define", defineName)) {
                ThrowFormattedError(currentFile, line, lineNumber, "Empty #define pre-processor directive. Expected identifier.", 8);
            }

            auto includeGuardIt = _includeGuardInstances.find(defineName);
            if (includeGuardIt != _includeGuardInstances.end()) {
                // Set line on which define was found.
                for (IncludeGuard& includeGuard : _includeGuards) {
                    if (includeGuard._includeGuardName == defineName) {
                        includeGuard._defineLineNumber = lineNumber;
                        return false;
                    }
                }

                // This should never happen due to include setup, but throw for safeguard reasons.
                ThrowFormattedError(currentFile, line, lineNumber, "Incorrectly setting up include guard mapping.", 0);
            }
            else {
                // Regular define.
                if (_hasVersionInformation) {
                    return true;
                }
                    // Shader version information must be the first compiled line of shader code.
                else {
                    ThrowFormattedError(currentFile, line, lineNumber, "Version directive must be first statement and may not be repeated.", 0);
                }
            }
        }

        return false;
    }

    void Shader::Parser::CloseIncludeGuard(const std::string &currentFile, const std::string &line, int lineNumber, const std::string &includeGuardName) {
        // Reached the end of this include guard, safe to include file lines once again.
        if (_processingExistingInclude) {
            _processingExistingInclude = false;
        }
        else {
            // Get the last include guard without a set #endif line number (last unterminated include guard).
            bool found = false;
            for (auto includeGuardIt = _includeGuards.rbegin(); includeGuardIt != _includeGuards.rend(); ++includeGuardIt) {
                IncludeGuard& includeGuard = *includeGuardIt;

                if (includeGuard._endifLineNumber == -1) {
                    found = true;
                    includeGuard._endifLineNumber = lineNumber;
                    break;
                }
            }

            // If the include guard stack is empty, an endif was pushed without an existing #if / #ifndef.
            if (!found) {
                ThrowFormattedError(currentFile, line, lineNumber, "#endif pre-processor directive without preexisting #if / #ifndef directive.", 0);
            }
        }
    }

    void Shader::Parser::ThrowFormattedError(std::string filename, std::string line, int lineNumber, std::string errorMessage, int locationOffset) const {
        static std::stringstream errorMessageBuilder;
        errorMessageBuilder.str(std::string()); // Clear.

        // Clear all newlines.
        EraseNewlines(filename, true);
        EraseNewlines(line, true);
        EraseNewlines(errorMessage, true);

        std::string lineNumberString = std::to_string(lineNumber);

        errorMessageBuilder << "In file '" << filename << "' on line " << lineNumberString << ": error: " << errorMessage << std::endl;
        errorMessageBuilder << std::setw(4) << lineNumberString << " |    " << line << std::endl;
        errorMessageBuilder << std::setw(4) << ' ' << " |    ";
        if (locationOffset > 0) {
            errorMessageBuilder << std::setw(locationOffset) << ' ';
        }
        errorMessageBuilder << '^';

        throw std::runtime_error(errorMessageBuilder.str());
    }

    void Shader::Parser::ValidateIncludeGuardScope() const {
        // Check for unterminated include guards.
        for (const IncludeGuard& includeGuard : _includeGuards) {
            // Found unterminated include guard.
            if (includeGuard._endifLineNumber == -1) {
                ThrowFormattedError(includeGuard._includeGuardFile, includeGuard._includeGuardLine, includeGuard._includeGuardLineNumber, "Unterminated #ifndef directive.", 0);
            }
        }
    }

    bool Shader::Parser::ValidateAgainst(const std::string &directiveName, const std::string& token) const {
        bool condition = directiveName.empty() || directiveName == token || token.front() == '#';

        if (directiveName == "#pragma") {
            condition |= token != "once";
        }

        return condition;
    }

}

