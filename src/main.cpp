#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "typechecker/typechecker.hpp"
#include "codegen/codegen.hpp"
#include "common/error.hpp"

static std::string readFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static void printUsage() {
    std::cout << "Usage: jspp <input.jspp> [options]\n"
              << "Options:\n"
              << "  -o <file>     Output C++ file (default: stdout)\n"
              << "  --tokens      Print token list and exit\n"
              << "  --ast         Print AST summary and exit\n"
              << "  --no-check    Skip type checking\n"
              << "  --help        Show this help\n";
}

static void printTokens(const std::vector<jspp::Token>& tokens) {
    for (auto& tok : tokens) {
        std::cout << jspp::tokenTypeName(tok.type) << " \"" << tok.value
                  << "\" @ " << tok.loc.line << ":" << tok.loc.column << "\n";
    }
}

static void printAST(const jspp::ProgramNode& program, int depth = 0) {
    auto pad = [](int d) { return std::string(d * 2, ' '); };
    std::cout << pad(depth) << "Program (" << program.statements.size() << " statements)\n";
    for (auto& s : program.statements) {
        switch (s->kind) {
            case jspp::NodeKind::FunctionDecl:
                std::cout << pad(depth+1) << "FunctionDecl: "
                          << static_cast<jspp::FunctionDeclStmt*>(s.get())->name << "\n";
                break;
            case jspp::NodeKind::VariableDecl:
                std::cout << pad(depth+1) << "VariableDecl: "
                          << static_cast<jspp::VariableDeclStmt*>(s.get())->name << "\n";
                break;
            case jspp::NodeKind::ClassDecl:
                std::cout << pad(depth+1) << "ClassDecl: "
                          << static_cast<jspp::ClassDeclStmt*>(s.get())->name << "\n";
                break;
            case jspp::NodeKind::EnumDecl:
                std::cout << pad(depth+1) << "EnumDecl: "
                          << static_cast<jspp::EnumDeclStmt*>(s.get())->name << "\n";
                break;
            case jspp::NodeKind::ImportDecl:
                std::cout << pad(depth+1) << "ImportDecl: "
                          << static_cast<jspp::ImportDeclStmt*>(s.get())->source << "\n";
                break;
            default:
                std::cout << pad(depth+1) << "Statement (kind="
                          << static_cast<int>(s->kind) << ")\n";
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    bool showTokens = false;
    bool showAST = false;
    bool skipTypeCheck = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else if (arg == "--tokens") {
            showTokens = true;
        } else if (arg == "--ast") {
            showAST = true;
        } else if (arg == "--no-check") {
            skipTypeCheck = true;
        } else if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg[0] != '-') {
            inputFile = arg;
        } else {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "error: no input file\n";
        return 1;
    }

    // Read source
    std::string source = readFile(inputFile);

    // Lex
    jspp::Lexer lexer(source, inputFile);
    auto tokens = lexer.tokenize();

    if (showTokens) {
        printTokens(tokens);
        return 0;
    }

    // Parse
    jspp::ErrorReporter errors;
    jspp::Parser parser(tokens, errors);
    std::unique_ptr<jspp::ProgramNode> program;

    try {
        program = parser.parse();
    } catch (const std::exception& e) {
        errors.printAll();
        std::cerr << e.what() << "\n";
        return 1;
    }

    if (showAST) {
        printAST(*program);
        return 0;
    }

    // Type check
    if (!skipTypeCheck) {
        jspp::TypeChecker checker(errors);
        checker.check(*program);
        if (errors.hasErrors()) {
            errors.printAll();
            return 1;
        }
    }

    // Code generation
    jspp::CodeGenerator codegen(errors);
    std::string cppCode = codegen.generate(*program);

    if (errors.hasErrors()) {
        errors.printAll();
        return 1;
    }

    // Output
    if (outputFile.empty()) {
        std::cout << cppCode;
    } else {
        std::ofstream ofs(outputFile);
        if (!ofs.is_open()) {
            std::cerr << "error: cannot write to '" << outputFile << "'\n";
            return 1;
        }
        ofs << cppCode;
        std::cout << "[jspp] compiled " << inputFile << " -> " << outputFile << "\n";
    }

    return 0;
}
