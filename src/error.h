#pragma once
#include <string>
#include <vector>
#include <iostream>

// One compiler diagnostic: a message with a source location.
// col == 0 means column is unknown (caret is suppressed).
struct Diagnostic {
    std::string message;
    int         line = 0;
    int         col  = 0;
};

// Split source text into lines for caret rendering.
inline std::vector<std::string> splitLines(const std::string& src) {
    std::vector<std::string> result;
    std::string line;
    for (char c : src) {
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            result.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) result.push_back(line);
    return result;
}

// Print every diagnostic in GCC/Clang style:
//
//   file:line:col: error: message
//       source line text
//       ^
//
// col == 0  →  omit ":col" and "^" (column unknown)
inline void renderErrors(const std::string&            file,
                         const std::vector<Diagnostic>& errors,
                         const std::string&             source) {
    auto lines = splitLines(source);
    for (const auto& err : errors) {
        // Header
        std::cerr << file << ":" << err.line;
        if (err.col > 0) std::cerr << ":" << err.col;
        std::cerr << ": error: " << err.message << "\n";

        // Source line
        if (err.line >= 1 && err.line <= (int)lines.size()) {
            std::cerr << "    " << lines[err.line - 1] << "\n";
            // Caret
            if (err.col > 0) {
                std::cerr << "    " << std::string(err.col - 1, ' ') << "^\n";
            }
        }
    }
}
