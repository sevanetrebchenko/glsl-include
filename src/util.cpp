
#include <util.h>
#include <filesystem>

namespace GLSL {

    std::string CreateDirectory(const std::string& directoryPath) {
        if (!std::filesystem::is_directory(directoryPath)) {
            throw std::runtime_error("Path provided to CreateDirectory is not a directory.");
        }

        std::string outputDirectory = directoryPath;
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

    std::string GetAssetName(const std::string& filepath) {
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

    void EraseNewlines(std::string& line, bool eraseLast) {
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

    void EraseComments(std::string& line) {
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

}
