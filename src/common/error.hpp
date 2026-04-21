#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "../lexer/token.hpp"

namespace jspp {

enum class ErrorLevel {
    Error,
    Warning,
    Note
};

struct CompileError {
    ErrorLevel level;
    std::string code;       // E001, W001, etc.
    std::string message;
    SourceLocation loc;
    std::string snippet;    // Source line for context
};

class ErrorReporter {
public:
    void error(const std::string& code, const std::string& message,
               SourceLocation loc, const std::string& snippet = "") {
        errors_.push_back({ErrorLevel::Error, code, message, loc, snippet});
    }

    void warning(const std::string& code, const std::string& message,
                 SourceLocation loc, const std::string& snippet = "") {
        errors_.push_back({ErrorLevel::Warning, code, message, loc, snippet});
    }

    void note(const std::string& code, const std::string& message,
              SourceLocation loc, const std::string& snippet = "") {
        errors_.push_back({ErrorLevel::Note, code, message, loc, snippet});
    }

    bool hasErrors() const {
        for (auto& e : errors_) {
            if (e.level == ErrorLevel::Error) return true;
        }
        return false;
    }

    int errorCount() const {
        int count = 0;
        for (auto& e : errors_) {
            if (e.level == ErrorLevel::Error) count++;
        }
        return count;
    }

    const std::vector<CompileError>& errors() const { return errors_; }

    void printAll(std::ostream& os = std::cerr) const {
        for (auto& e : errors_) {
            printError(e, os);
        }
        if (hasErrors()) {
            os << "\ncompilation failed with " << errorCount() << " error(s)\n";
        }
    }

    void clear() { errors_.clear(); }

private:
    std::vector<CompileError> errors_;

    void printError(const CompileError& e, std::ostream& os) const {
        const char* levelStr = "error";
        if (e.level == ErrorLevel::Warning) levelStr = "warning";
        if (e.level == ErrorLevel::Note)    levelStr = "note";

        os << levelStr << "[" << e.code << "]: " << e.message << "\n";
        os << "  --> " << e.loc.filename << ":" << e.loc.line << ":" << e.loc.column << "\n";

        if (!e.snippet.empty()) {
            os << "   |\n";
            os << " " << e.loc.line << " | " << e.snippet << "\n";
            os << "   |";
            // Underline at column position
            for (int i = 0; i < e.loc.column; i++) os << " ";
            os << "^^^^\n";
        }
        os << "\n";
    }
};

} // namespace jspp
