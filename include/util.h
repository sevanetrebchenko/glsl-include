
#ifndef GLSL_INCLUDE_UTIL_H
#define GLSL_INCLUDE_UTIL_H

#include <string>

namespace GLSL {

    // Creates directory if it does not exist already. Returns directory path.
    std::string CreateDirectory(const std::string& directoryPath);

    // Returns asset name from a given path.
    std::string GetAssetName(const std::string& filepath);

    void EraseNewlines(std::string& line, bool eraseLast);
    void EraseComments(std::string& line);

}

#endif //GLSL_INCLUDE_UTIL_H
