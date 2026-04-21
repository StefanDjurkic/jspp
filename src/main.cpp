#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

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
              << "       jspp init <name> [--bare]\n"
              << "Options:\n"
              << "  -o <file>     Output C++ file (default: stdout)\n"
              << "  --tokens      Print token list and exit\n"
              << "  --ast         Print AST summary and exit\n"
              << "  --no-check    Skip type checking\n"
              << "  --help        Show this help\n"
              << "\nSubcommands:\n"
              << "  init <name>   Scaffold a new JSPP project in ./<name>/\n"
              << "                Use --bare for a minimal layout (src/ + jspp.toml only).\n";
}

// ---------------------------------------------------------------------------
// `jspp init` — scaffold a new project
// ---------------------------------------------------------------------------

namespace fs = std::filesystem;

static bool writeFileIfAbsent(const fs::path& path, const std::string& content) {
    if (fs::exists(path)) {
        std::cerr << "error: refusing to overwrite existing file '" << path.string() << "'\n";
        return false;
    }
    fs::create_directories(path.parent_path());
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "error: cannot create '" << path.string() << "'\n";
        return false;
    }
    ofs << content;
    return true;
}

static int runInit(const std::string& name, bool bare) {
    if (name.empty()) {
        std::cerr << "error: jspp init requires a project name\n";
        return 1;
    }
    fs::path root = fs::path(name);
    if (fs::exists(root) && !fs::is_empty(root)) {
        std::cerr << "error: directory '" << root.string() << "' already exists and is not empty\n";
        return 1;
    }
    fs::create_directories(root);

    const std::string mainJspp =
        "// " + name + " — a JSPP project\n"
        "// Build: jspp src/main.jspp -o main.cpp && c++ -std=c++20 main.cpp -o " + name + "\n"
        "\n"
        "function main(): int {\n"
        "    print(\"Hello from " + name + "!\");\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "main();\n";

    const std::string jsppToml =
        "[package]\n"
        "name = \"" + name + "\"\n"
        "version = \"0.1.0\"\n"
        "entry = \"src/main.jspp\"\n"
        "\n"
        "[build]\n"
        "cxx_standard = 20\n"
        "optimization = \"O2\"\n";

    if (!writeFileIfAbsent(root / "src" / "main.jspp", mainJspp)) return 1;
    if (!writeFileIfAbsent(root / "jspp.toml", jsppToml)) return 1;

    if (!bare) {
        const std::string gitignore =
            "# Build artifacts\n"
            "build/\n"
            "*.o\n"
            "*.obj\n"
            "*.exe\n"
            "\n"
            "# Generated C++\n"
            "*.generated.cpp\n"
            "\n"
            "# Editor\n"
            ".vscode/\n"
            ".idea/\n"
            "*.swp\n";

        const std::string readme =
            "# " + name + "\n"
            "\n"
            "A project written in [JSPP](https://github.com/StefanDjurkic/jspp) — JavaScript syntax, C++ performance.\n"
            "\n"
            "## Build\n"
            "\n"
            "```sh\n"
            "jspp src/main.jspp -o main.cpp\n"
            "c++ -std=c++20 -O2 main.cpp -o " + name + "\n"
            "./" + name + "\n"
            "```\n"
            "\n"
            "## Run with the reference interpreter\n"
            "\n"
            "```sh\n"
            "node path/to/jspp/prototype/jspp.mjs src/main.jspp --run\n"
            "```\n";

        const std::string testJspp =
            "// Add regression tests here — one .jspp per scenario.\n"
            "\n"
            "print(\"ok\");\n";

        const std::string ci =
            "name: CI\n"
            "\n"
            "on:\n"
            "  push:\n"
            "    branches: [main]\n"
            "  pull_request:\n"
            "    branches: [main]\n"
            "\n"
            "jobs:\n"
            "  build:\n"
            "    runs-on: ubuntu-latest\n"
            "    steps:\n"
            "      - uses: actions/checkout@v4\n"
            "      - uses: actions/setup-node@v4\n"
            "        with: { node-version: '20' }\n"
            "      # Replace this with: download the jspp binary / checkout jspp repo / build it.\n"
            "      - run: echo \"Wire up jspp build here — see https://github.com/StefanDjurkic/jspp\"\n";

        if (!writeFileIfAbsent(root / ".gitignore", gitignore)) return 1;
        if (!writeFileIfAbsent(root / "README.md", readme)) return 1;
        if (!writeFileIfAbsent(root / "tests" / "main_test.jspp", testJspp)) return 1;
        if (!writeFileIfAbsent(root / ".github" / "workflows" / "ci.yml", ci)) return 1;
    }

    std::cout << "Created " << (bare ? "bare " : "") << "JSPP project at '" << root.string() << "'.\n"
              << "\nNext steps:\n"
              << "  cd " << root.string() << "\n"
              << "  jspp src/main.jspp -o main.cpp\n"
              << "  c++ -std=c++20 main.cpp -o " << name << " && ./" << name << "\n";
    return 0;
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

    // Subcommand: jspp init <name> [--bare]
    if (std::string(argv[1]) == "init") {
        std::string name;
        bool bare = false;
        for (int i = 2; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--bare") bare = true;
            else if (a == "--help" || a == "-h") { printUsage(); return 0; }
            else if (!a.empty() && a[0] != '-') name = a;
            else { std::cerr << "error: unknown option for init: '" << a << "'\n"; return 1; }
        }
        return runInit(name, bare);
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
