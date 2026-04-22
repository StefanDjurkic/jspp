#pragma once

#include <string>
#include <sstream>
#include <set>

namespace jspp {

class CppEmitter {
public:
    void emit(const std::string& text) { out_ << text; }
    void emitLine(const std::string& text = "") { out_ << text << "\n"; }
    void emitIndented(const std::string& text) {
        for (int i = 0; i < indent_; i++) out_ << "    ";
        out_ << text;
    }
    void emitIndentedLine(const std::string& text = "") {
        emitIndented(text);
        out_ << "\n";
    }

    void indent() { indent_++; }
    void dedent() { if (indent_ > 0) indent_--; }

    void addInclude(const std::string& header) {
        includes_.insert(header);
    }

    // Prepend raw text (e.g. hoisted anonymous struct defs) before all the
    // emitted code but after the includes.
    void addPrelude(const std::string& text) {
        prelude_ << text;
    }

    std::string getIncludes() const {
        std::ostringstream inc;
        for (auto& h : includes_) {
            inc << "#include " << h << "\n";
        }
        return inc.str();
    }

    std::string getOutput() const {
        return getIncludes() + "\n" + prelude_.str() + out_.str();
    }

    void clear() {
        out_.str("");
        out_.clear();
        prelude_.str("");
        prelude_.clear();
        includes_.clear();
        indent_ = 0;
    }

private:
    std::ostringstream out_;
    std::ostringstream prelude_;
    std::set<std::string> includes_;
    int indent_ = 0;
};

} // namespace jspp
