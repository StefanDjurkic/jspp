#include "codegen.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace jspp {

// ============================================================================
// Constructor
// ============================================================================

CodeGenerator::CodeGenerator(ErrorReporter& errors)
    : errors_(errors) {}

// ============================================================================
// Top-level generate
// ============================================================================

std::string CodeGenerator::generate(const ProgramNode& program) {
    emitter_.clear();
    emitter_.addInclude("<iostream>");
    emitter_.addInclude("<string>");
    emitter_.addInclude("<vector>");

    // Two-pass: separate declarations from executable statements
    std::vector<const Statement*> decls;
    std::vector<const Statement*> exec;
    bool hasMain = false;

    for (auto& stmt : program.statements) {
        if (stmt->kind == NodeKind::FunctionDecl) {
            auto* fd = static_cast<const FunctionDeclStmt*>(stmt.get());
            if (fd->name == "main") hasMain = true;
            decls.push_back(stmt.get());
        } else if (stmt->kind == NodeKind::ClassDecl || stmt->kind == NodeKind::TypeDecl ||
                   stmt->kind == NodeKind::EnumDecl || stmt->kind == NodeKind::ImportDecl ||
                   stmt->kind == NodeKind::ExportDecl || stmt->kind == NodeKind::CppInclude ||
                   stmt->kind == NodeKind::CppBlock) {
            decls.push_back(stmt.get());
        } else {
            exec.push_back(stmt.get());
        }
    }

    // Emit print helper
    emitter_.emitLine("template<typename... Args>");
    emitter_.emitLine("void print(Args&&... args) {");
    emitter_.emitLine("    bool __first = true;");
    emitter_.emitLine("    ((std::cout << (__first ? \"\" : \" \") << args, __first = false), ...);");
    emitter_.emitLine("    std::cout << std::endl;");
    emitter_.emitLine("}");
    emitter_.emitLine();

    // Emit __jspp_set helper for dynamic array assignment
    emitter_.emitLine("template<typename T>");
    emitter_.emitLine("void __jspp_set(std::vector<T>& v, size_t i, T val) {");
    emitter_.emitLine("    if (i >= v.size()) v.resize(i + 1);");
    emitter_.emitLine("    v[i] = std::move(val);");
    emitter_.emitLine("}");
    emitter_.emitLine();

    // Emit __jspp_concat helper for type-safe string concatenation
    emitter_.emitLine("template<typename T>");
    emitter_.emitLine("std::string __jspp_to_str(const T& v) { return std::to_string(v); }");
    emitter_.emitLine("inline std::string __jspp_to_str(const std::string& v) { return v; }");
    emitter_.emitLine("inline std::string __jspp_to_str(const char* v) { return std::string(v); }");
    emitter_.emitLine("template<typename A, typename B>");
    emitter_.emitLine("std::string __jspp_concat(const A& a, const B& b) { return __jspp_to_str(a) + __jspp_to_str(b); }");
    emitter_.emitLine();

    // Emit declarations
    for (auto* d : decls) {
        emitStatement(*d);
    }

    // Wrap executable statements in main() if needed
    if (!exec.empty() && !hasMain) {
        emitter_.emitLine("int main() {");
        emitter_.indent();
        for (auto* s : exec) {
            emitStatement(*s);
        }
        emitter_.emitIndentedLine("return 0;");
        emitter_.dedent();
        emitter_.emitLine("}");
    }

    return emitter_.getOutput();
}

// ============================================================================
// Statement emission
// ============================================================================

void CodeGenerator::emitStatement(const Statement& stmt) {
    switch (stmt.kind) {
        case NodeKind::VariableDecl:
            emitVariableDecl(static_cast<const VariableDeclStmt&>(stmt)); break;
        case NodeKind::FunctionDecl:
            emitFunctionDecl(static_cast<const FunctionDeclStmt&>(stmt)); break;
        case NodeKind::ClassDecl:
            emitClassDecl(static_cast<const ClassDeclStmt&>(stmt)); break;
        case NodeKind::TypeDecl:
            emitTypeDecl(static_cast<const TypeDeclStmt&>(stmt)); break;
        case NodeKind::EnumDecl:
            emitEnumDecl(static_cast<const EnumDeclStmt&>(stmt)); break;
        case NodeKind::ImportDecl:
            emitImport(static_cast<const ImportDeclStmt&>(stmt)); break;
        case NodeKind::ExportDecl: {
            auto& exp = static_cast<const ExportDeclStmt&>(stmt);
            if (exp.declaration) emitStatement(*exp.declaration);
            break;
        }
        case NodeKind::CppInclude:
            emitCppInclude(static_cast<const CppIncludeStmt&>(stmt)); break;
        case NodeKind::CppBlock:
            emitCppBlock(static_cast<const CppBlockStmt&>(stmt)); break;
        case NodeKind::BlockStmt:
            emitBlock(static_cast<const BlockStmt&>(stmt)); break;
        case NodeKind::ExpressionStmt: {
            auto& es = static_cast<const ExpressionStmt&>(stmt);
            emitter_.emitIndentedLine(emitExpression(*es.expr) + ";");
            break;
        }
        case NodeKind::ReturnStmt:
            emitReturn(static_cast<const ReturnStmt_&>(stmt)); break;
        case NodeKind::IfStmt:
            emitIf(static_cast<const IfStmt_&>(stmt)); break;
        case NodeKind::ForStmt:
            emitFor(static_cast<const ForStmt_&>(stmt)); break;
        case NodeKind::ForOfStmt:
            emitForOf(static_cast<const ForOfStmt_&>(stmt)); break;
        case NodeKind::WhileStmt:
            emitWhile(static_cast<const WhileStmt_&>(stmt)); break;
        case NodeKind::DoWhileStmt:
            emitDoWhile(static_cast<const DoWhileStmt_&>(stmt)); break;
        case NodeKind::SwitchStmt:
            emitSwitch(static_cast<const SwitchStmt_&>(stmt)); break;
        case NodeKind::TryStmt:
            emitTryCatch(static_cast<const TryStmt_&>(stmt)); break;
        case NodeKind::ThrowStmt:
            emitThrow(static_cast<const ThrowStmt_&>(stmt)); break;
        case NodeKind::BreakStmt:
            emitter_.emitIndentedLine("break;"); break;
        case NodeKind::ContinueStmt:
            emitter_.emitIndentedLine("continue;"); break;
        default:
            emitter_.emitIndentedLine("/* unknown statement */");
            break;
    }
}

// ============================================================================
// Declarations
// ============================================================================

void CodeGenerator::emitVariableDecl(const VariableDeclStmt& decl) {
    std::string cppType;
    if (decl.typeAnnotation) {
        cppType = emitType(*decl.typeAnnotation);
    }

    std::string prefix = decl.isConst ? "const " : "";

    if (decl.initializer) {
        if (cppType.empty()) {
            // Infer type from initializer
            cppType = "auto";
            // Try to get a more specific type for arrays
            if (decl.initializer->kind == NodeKind::ArrayLiteral) {
                auto& arr = static_cast<const ArrayLiteralExpr&>(*decl.initializer);
                if (!arr.elements.empty()) {
                    std::string inner = "auto";
                    if (arr.elements[0]->kind == NodeKind::IntLiteral) inner = "int";
                    else if (arr.elements[0]->kind == NodeKind::FloatLiteral) inner = "double";
                    else if (arr.elements[0]->kind == NodeKind::StringLiteral) inner = "std::string";
                    else if (arr.elements[0]->kind == NodeKind::BoolLiteral) inner = "bool";
                    cppType = "std::vector<" + inner + ">";
                    emitter_.addInclude("<vector>");
                } else {
                    cppType = "std::vector<int>";
                    emitter_.addInclude("<vector>");
                }
            } else if (decl.initializer->kind == NodeKind::StringLiteral) {
                cppType = "std::string";
            } else if (decl.initializer->kind == NodeKind::IntLiteral) {
                cppType = "int";
            } else if (decl.initializer->kind == NodeKind::FloatLiteral) {
                cppType = "double";
            } else if (decl.initializer->kind == NodeKind::BoolLiteral) {
                cppType = "bool";
            }
        }
        emitter_.emitIndentedLine(prefix + cppType + " " + decl.name + " = " +
                                  emitExpression(*decl.initializer) + ";");
    } else {
        emitter_.emitIndentedLine(prefix + (cppType.empty() ? "auto" : cppType) +
                                  " " + decl.name + ";");
    }
}

void CodeGenerator::emitFunctionDecl(const FunctionDeclStmt& decl) {
    std::string retType = decl.returnType ? emitType(*decl.returnType) : "auto";

    // Generic params
    if (!decl.genericParams.empty()) {
        std::string tmpl = "template<";
        for (size_t i = 0; i < decl.genericParams.size(); i++) {
            if (i > 0) tmpl += ", ";
            tmpl += "typename " + decl.genericParams[i];
        }
        tmpl += ">";
        emitter_.emitIndentedLine(tmpl);
    }

    // Params
    std::string params;
    for (size_t i = 0; i < decl.params.size(); i++) {
        if (i > 0) params += ", ";
        auto& p = *decl.params[i];
        std::string t = p.typeAnnotation ? emitType(*p.typeAnnotation) : "auto";
        std::string ref = p.isRef ? "&" : "";
        params += t + ref + " " + p.name;
        if (p.defaultValue) {
            params += " = " + emitExpression(*p.defaultValue);
        }
    }

    emitter_.emitIndentedLine(retType + " " + decl.name + "(" + params + ") {");
    emitter_.indent();
    if (decl.body) {
        for (auto& s : decl.body->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
    emitter_.emitLine();
}

void CodeGenerator::emitClassDecl(const ClassDeclStmt& decl) {
    std::string line = "class " + decl.name;
    if (!decl.baseName.empty()) {
        line += " : public " + decl.baseName;
    }
    line += " {";
    emitter_.emitIndentedLine(line);
    emitter_.emitIndentedLine("public:");
    emitter_.indent();

    // Fields
    for (auto& f : decl.fields) {
        std::string t = f->type ? emitType(*f->type) : "auto";
        std::string fl = t + " " + f->name;
        if (f->defaultValue) {
            fl += " = " + emitExpression(*f->defaultValue);
        }
        emitter_.emitIndentedLine(fl + ";");
    }
    if (!decl.fields.empty()) emitter_.emitLine();

    // Constructor
    if (decl.constructor) {
        std::string params;
        for (size_t i = 0; i < decl.constructor->params.size(); i++) {
            if (i > 0) params += ", ";
            auto& p = *decl.constructor->params[i];
            std::string t = p.typeAnnotation ? emitType(*p.typeAnnotation) : "auto";
            params += t + " " + p.name;
        }
        // Check for super() call to emit as initializer list
        std::string initList;
        std::vector<const Statement*> bodyStmts;
        if (decl.constructor->body) {
            for (auto& s : decl.constructor->body->statements) {
                // Detect super(args) — an ExpressionStatement with a CallExpr whose callee is SuperExpr
                if (s->kind == NodeKind::ExpressionStmt) {
                    auto& es = static_cast<const ExpressionStmt&>(*s);
                    if (es.expr && es.expr->kind == NodeKind::CallExpr) {
                        auto& call = static_cast<const CallExpr&>(*es.expr);
                        if (call.callee && call.callee->kind == NodeKind::SuperExpr && !decl.baseName.empty()) {
                            std::string superArgs;
                            for (size_t j = 0; j < call.args.size(); j++) {
                                if (j > 0) superArgs += ", ";
                                superArgs += emitExpression(*call.args[j]);
                            }
                            initList = " : " + decl.baseName + "(" + superArgs + ")";
                            continue; // Don't emit as a statement
                        }
                    }
                }
                bodyStmts.push_back(s.get());
            }
        }
        emitter_.emitIndentedLine(decl.name + "(" + params + ")" + initList + " {");
        emitter_.indent();
        for (auto* s : bodyStmts) {
            emitStatement(*s);
        }
        emitter_.dedent();
        emitter_.emitIndentedLine("}");
        emitter_.emitLine();
    }

    // Methods
    for (auto& m : decl.methods) {
        std::string retType = m->returnType ? emitType(*m->returnType) : "auto";
        std::string params;
        for (size_t i = 0; i < m->params.size(); i++) {
            if (i > 0) params += ", ";
            auto& p = *m->params[i];
            std::string t = p.typeAnnotation ? emitType(*p.typeAnnotation) : "auto";
            params += t + " " + p.name;
        }
        emitter_.emitIndentedLine(retType + " " + m->name + "(" + params + ") {");
        emitter_.indent();
        if (m->body) {
            for (auto& s : m->body->statements) {
                emitStatement(*s);
            }
        }
        emitter_.dedent();
        emitter_.emitIndentedLine("}");
        emitter_.emitLine();
    }

    emitter_.dedent();
    emitter_.emitIndentedLine("};");
    emitter_.emitLine();
}

void CodeGenerator::emitTypeDecl(const TypeDeclStmt& decl) {
    if (!decl.genericParams.empty()) {
        std::string tmpl = "template<";
        for (size_t i = 0; i < decl.genericParams.size(); i++) {
            if (i > 0) tmpl += ", ";
            tmpl += "typename " + decl.genericParams[i];
        }
        tmpl += ">";
        emitter_.emitIndentedLine(tmpl);
    }

    emitter_.emitIndentedLine("struct " + decl.name + " {");
    emitter_.indent();
    for (auto& f : decl.fields) {
        std::string t = f->type ? emitType(*f->type) : "auto";
        std::string fl = t + " " + f->name;
        if (f->defaultValue) {
            fl += " = " + emitExpression(*f->defaultValue);
        }
        emitter_.emitIndentedLine(fl + ";");
    }
    for (auto& m : decl.methods) {
        std::string retType = m->returnType ? emitType(*m->returnType) : "auto";
        std::string params;
        for (size_t i = 0; i < m->params.size(); i++) {
            if (i > 0) params += ", ";
            auto& p = *m->params[i];
            std::string t = p.typeAnnotation ? emitType(*p.typeAnnotation) : "auto";
            params += t + " " + p.name;
        }
        emitter_.emitIndentedLine(retType + " " + m->name + "(" + params + ") {");
        emitter_.indent();
        if (m->body) {
            for (auto& s : m->body->statements) {
                emitStatement(*s);
            }
        }
        emitter_.dedent();
        emitter_.emitIndentedLine("}");
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("};");
    emitter_.emitLine();
}

void CodeGenerator::emitEnumDecl(const EnumDeclStmt& decl) {
    emitter_.emitIndentedLine("enum class " + decl.name + " {");
    emitter_.indent();
    for (auto& m : decl.members) {
        std::string line = m->name;
        if (m->value) {
            line += " = " + emitExpression(*m->value);
        }
        line += ",";
        emitter_.emitIndentedLine(line);
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("};");
    emitter_.emitLine();
}

void CodeGenerator::emitImport(const ImportDeclStmt& decl) {
    // Convert JSPP import path to C++ #include
    std::string header = decl.source;
    // Remove ./ prefix and file extension, add .hpp
    if (header.substr(0, 2) == "./") header = header.substr(2);
    auto dot = header.rfind('.');
    if (dot != std::string::npos) header = header.substr(0, dot);
    emitter_.emitLine("#include \"" + header + ".hpp\"");
}

void CodeGenerator::emitCppBlock(const CppBlockStmt& block) {
    emitter_.emitIndentedLine(block.code);
}

void CodeGenerator::emitCppInclude(const CppIncludeStmt& inc) {
    emitter_.addInclude(inc.header);
}

void CodeGenerator::emitBlock(const BlockStmt& block) {
    emitter_.emitIndentedLine("{");
    emitter_.indent();
    for (auto& s : block.statements) {
        emitStatement(*s);
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
}

// ============================================================================
// Control flow statements
// ============================================================================

void CodeGenerator::emitIf(const IfStmt_& stmt) {
    emitter_.emitIndentedLine("if (" + emitExpression(*stmt.condition) + ") {");
    emitter_.indent();
    if (stmt.thenBlock) {
        for (auto& s : stmt.thenBlock->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    if (stmt.elseBlock) {
        // Check if else contains a single IfStmt (else-if chain)
        if (stmt.elseBlock->statements.size() == 1 &&
            stmt.elseBlock->statements[0]->kind == NodeKind::IfStmt) {
            emitter_.emitIndented("} else ");
            emitIf(static_cast<const IfStmt_&>(*stmt.elseBlock->statements[0]));
            return;
        }
        emitter_.emitIndentedLine("} else {");
        emitter_.indent();
        for (auto& s : stmt.elseBlock->statements) {
            emitStatement(*s);
        }
        emitter_.dedent();
        emitter_.emitIndentedLine("}");
    } else {
        emitter_.emitIndentedLine("}");
    }
}

void CodeGenerator::emitFor(const ForStmt_& stmt) {
    std::string init;
    if (stmt.init) {
        if (stmt.init->kind == NodeKind::VariableDecl) {
            auto& vd = static_cast<const VariableDeclStmt&>(*stmt.init);
            std::string t = vd.typeAnnotation ? emitType(*vd.typeAnnotation) : "auto";
            init = t + " " + vd.name;
            if (vd.initializer) {
                init += " = " + emitExpression(*vd.initializer);
            }
        } else if (stmt.init->kind == NodeKind::ExpressionStmt) {
            init = emitExpression(*static_cast<const ExpressionStmt&>(*stmt.init).expr);
        }
    }
    std::string cond = stmt.condition ? emitExpression(*stmt.condition) : "";
    std::string upd = stmt.update ? emitExpression(*stmt.update) : "";

    emitter_.emitIndentedLine("for (" + init + "; " + cond + "; " + upd + ") {");
    emitter_.indent();
    if (stmt.body) {
        for (auto& s : stmt.body->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
}

void CodeGenerator::emitForOf(const ForOfStmt_& stmt) {
    emitter_.addInclude("<vector>");
    emitter_.emitIndentedLine("for (auto& " + stmt.varName + " : " +
                              emitExpression(*stmt.iterable) + ") {");
    emitter_.indent();
    if (stmt.body) {
        for (auto& s : stmt.body->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
}

void CodeGenerator::emitWhile(const WhileStmt_& stmt) {
    emitter_.emitIndentedLine("while (" + emitExpression(*stmt.condition) + ") {");
    emitter_.indent();
    if (stmt.body) {
        for (auto& s : stmt.body->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
}

void CodeGenerator::emitDoWhile(const DoWhileStmt_& stmt) {
    emitter_.emitIndentedLine("do {");
    emitter_.indent();
    if (stmt.body) {
        for (auto& s : stmt.body->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("} while (" + emitExpression(*stmt.condition) + ");");
}

void CodeGenerator::emitSwitch(const SwitchStmt_& stmt) {
    emitter_.emitIndentedLine("switch (" + emitExpression(*stmt.discriminant) + ") {");
    emitter_.indent();
    for (auto& c : stmt.cases) {
        emitter_.emitIndentedLine("case " + emitExpression(*c->value) + ":");
        emitter_.indent();
        for (auto& s : c->body) {
            emitStatement(*s);
        }
        emitter_.dedent();
    }
    if (stmt.defaultCase) {
        emitter_.emitIndentedLine("default:");
        emitter_.indent();
        for (auto& s : stmt.defaultCase->body) {
            emitStatement(*s);
        }
        emitter_.dedent();
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
}

void CodeGenerator::emitTryCatch(const TryStmt_& stmt) {
    emitter_.emitIndentedLine("try {");
    emitter_.indent();
    if (stmt.tryBlock) {
        for (auto& s : stmt.tryBlock->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    std::string paramType = "const auto&";
    if (stmt.catchClause && stmt.catchClause->paramType) {
        paramType = "const " + emitType(*stmt.catchClause->paramType) + "&";
    }
    std::string paramName = stmt.catchClause ? stmt.catchClause->paramName : "e";
    emitter_.emitIndentedLine("} catch (" + paramType + " " + paramName + ") {");
    emitter_.indent();
    if (stmt.catchClause && stmt.catchClause->body) {
        for (auto& s : stmt.catchClause->body->statements) {
            emitStatement(*s);
        }
    }
    emitter_.dedent();
    emitter_.emitIndentedLine("}");
}

void CodeGenerator::emitReturn(const ReturnStmt_& stmt) {
    if (stmt.value) {
        emitter_.emitIndentedLine("return " + emitExpression(*stmt.value) + ";");
    } else {
        emitter_.emitIndentedLine("return;");
    }
}

void CodeGenerator::emitThrow(const ThrowStmt_& stmt) {
    emitter_.emitIndentedLine("throw std::string(" + emitExpression(*stmt.value) + ");");
}

// ============================================================================
// Expression emission
// ============================================================================

static std::string escapeString(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

std::string CodeGenerator::emitExpression(const Expression& expr) {
    switch (expr.kind) {
        case NodeKind::IntLiteral:
            return std::to_string(static_cast<const IntLiteralExpr&>(expr).value);
        case NodeKind::FloatLiteral: {
            std::ostringstream oss;
            oss << static_cast<const FloatLiteralExpr&>(expr).value;
            return oss.str();
        }
        case NodeKind::StringLiteral: {
            auto& s = static_cast<const StringLiteralExpr&>(expr);
            emitter_.addInclude("<string>");
            return "std::string(\"" + escapeString(s.value) + "\")";
        }
        case NodeKind::CharLiteral:
            return "'" + std::string(1, static_cast<const CharLiteralExpr&>(expr).value) + "'";
        case NodeKind::BoolLiteral:
            return static_cast<const BoolLiteralExpr&>(expr).value ? "true" : "false";
        case NodeKind::NullLiteral:
            return "nullptr";
        case NodeKind::Identifier:
            return static_cast<const IdentifierExpr&>(expr).name;
        case NodeKind::ThisExpr:
            return "this";
        case NodeKind::SuperExpr:
            return "/* super */";
        case NodeKind::BinaryExpr:
            return emitBinaryExpr(static_cast<const BinaryExpr&>(expr));
        case NodeKind::UnaryExpr:
            return emitUnaryExpr(static_cast<const UnaryExpr&>(expr));
        case NodeKind::AssignExpr: {
            auto& ae = static_cast<const AssignExpr&>(expr);
            // Check for dynamic array set: arr[i] = val
            if (ae.op == "=" && ae.target->kind == NodeKind::IndexExpr) {
                emitter_.addInclude("<vector>");
                auto& idx = static_cast<const IndexExpr&>(*ae.target);
                return "__jspp_set(" + emitExpression(*idx.object) + ", (size_t)(" +
                       emitExpression(*idx.index) + "), " + emitExpression(*ae.value) + ")";
            }
            return emitExpression(*ae.target) + " " + ae.op + " " + emitExpression(*ae.value);
        }
        case NodeKind::CallExpr:
            return emitCallExpr(static_cast<const CallExpr&>(expr));
        case NodeKind::MemberExpr:
            return emitMemberExpr(static_cast<const MemberExpr&>(expr));
        case NodeKind::IndexExpr:
            return emitIndexExpr(static_cast<const IndexExpr&>(expr));
        case NodeKind::ArrayLiteral:
            return emitArrayLiteral(static_cast<const ArrayLiteralExpr&>(expr));
        case NodeKind::ObjectLiteral:
            return emitObjectLiteral(static_cast<const ObjectLiteralExpr&>(expr));
        case NodeKind::TemplateLiteral:
            return emitTemplateLiteral(static_cast<const TemplateLiteralExpr&>(expr));
        case NodeKind::TernaryExpr: {
            auto& te = static_cast<const TernaryExpr&>(expr);
            return "(" + emitExpression(*te.condition) + " ? " +
                   emitExpression(*te.thenExpr) + " : " +
                   emitExpression(*te.elseExpr) + ")";
        }
        case NodeKind::NewExpr: {
            auto& ne = static_cast<const NewExprNode&>(expr);
            std::string args;
            for (size_t i = 0; i < ne.args.size(); i++) {
                if (i > 0) args += ", ";
                args += emitExpression(*ne.args[i]);
            }
            return ne.typeName + "(" + args + ")";
        }
        case NodeKind::ArrowFunction:
            return emitArrowFunction(static_cast<const ArrowFunctionExpr&>(expr));
        case NodeKind::FunctionExpr:
            return emitFunctionExpr(static_cast<const FunctionExprNode&>(expr));
        default:
            return "/* unknown expr */";
    }
}

std::string CodeGenerator::emitBinaryExpr(const BinaryExpr& expr) {
    auto l = emitExpression(*expr.left);
    auto r = emitExpression(*expr.right);
    std::string op = expr.op;
    if (op == "===") op = "==";
    if (op == "!==") op = "!=";

    // String concatenation: when + involves at least one string literal,
    // use __jspp_concat helper for type-safe concatenation
    if (op == "+" && (expr.left->kind == NodeKind::StringLiteral ||
                      expr.right->kind == NodeKind::StringLiteral ||
                      isStringConcatExpr(*expr.left) ||
                      isStringConcatExpr(*expr.right))) {
        return "__jspp_concat(" + l + ", " + r + ")";
    }

    return "(" + l + " " + op + " " + r + ")";
}

std::string CodeGenerator::emitUnaryExpr(const UnaryExpr& expr) {
    auto o = emitExpression(*expr.operand);
    if (expr.prefix) return "(" + expr.op + o + ")";
    return "(" + o + expr.op + ")";
}

std::string CodeGenerator::emitCallExpr(const CallExpr& expr) {
    // Special: print()
    if (expr.callee->kind == NodeKind::Identifier) {
        auto& name = static_cast<const IdentifierExpr&>(*expr.callee).name;
        if (name == "print") {
            std::string args;
            for (size_t i = 0; i < expr.args.size(); i++) {
                if (i > 0) args += ", ";
                args += emitExpression(*expr.args[i]);
            }
            return "print(" + args + ")";
        }
    }

    // Special: console.log
    if (expr.callee->kind == NodeKind::MemberExpr) {
        auto& me = static_cast<const MemberExpr&>(*expr.callee);
        if (me.object->kind == NodeKind::Identifier &&
            static_cast<const IdentifierExpr&>(*me.object).name == "console" &&
            me.member == "log") {
            std::string parts;
            for (size_t i = 0; i < expr.args.size(); i++) {
                if (i > 0) parts += " << \" \" << ";
                parts += emitExpression(*expr.args[i]);
            }
            return "(std::cout << " + parts + " << std::endl)";
        }
    }

    // Array methods
    if (expr.callee->kind == NodeKind::MemberExpr) {
        auto& me = static_cast<const MemberExpr&>(*expr.callee);
        auto obj = emitExpression(*me.object);
        if (me.member == "push") {
            emitter_.addInclude("<vector>");
            std::string args;
            for (size_t i = 0; i < expr.args.size(); i++) {
                if (i > 0) args += ", ";
                args += emitExpression(*expr.args[i]);
            }
            return obj + ".push_back(" + args + ")";
        }
        if (me.member == "pop") {
            return obj + ".pop_back()";
        }
        if (me.member == "includes") {
            emitter_.addInclude("<algorithm>");
            auto val = emitExpression(*expr.args[0]);
            return "(std::find(" + obj + ".begin(), " + obj + ".end(), " + val + ") != " + obj + ".end())";
        }
    }

    // Super call
    if (expr.callee->kind == NodeKind::SuperExpr) {
        std::string args;
        for (size_t i = 0; i < expr.args.size(); i++) {
            if (i > 0) args += ", ";
            args += emitExpression(*expr.args[i]);
        }
        return "/* super(" + args + ") */";
    }

    auto callee = emitExpression(*expr.callee);
    std::string args;
    for (size_t i = 0; i < expr.args.size(); i++) {
        if (i > 0) args += ", ";
        args += emitExpression(*expr.args[i]);
    }
    return callee + "(" + args + ")";
}

std::string CodeGenerator::emitMemberExpr(const MemberExpr& expr) {
    auto obj = emitExpression(*expr.object);

    // Math.* → std::*
    if (expr.object->kind == NodeKind::Identifier &&
        static_cast<const IdentifierExpr&>(*expr.object).name == "Math") {
        emitter_.addInclude("<cmath>");
        if (expr.member == "sqrt")  return "std::sqrt";
        if (expr.member == "abs")   return "std::abs";
        if (expr.member == "floor") return "std::floor";
        if (expr.member == "ceil")  return "std::ceil";
        if (expr.member == "max")   return "std::max";
        if (expr.member == "min")   return "std::min";
        if (expr.member == "PI")    return "M_PI";
    }

    // .length → .size()
    if (expr.member == "length") {
        return obj + ".size()";
    }

    // this.x → this->x (C++ pointer member access)
    if (expr.object->kind == NodeKind::ThisExpr) {
        return "this->" + expr.member;
    }

    return obj + "." + expr.member;
}

std::string CodeGenerator::emitIndexExpr(const IndexExpr& expr) {
    return emitExpression(*expr.object) + "[" + emitExpression(*expr.index) + "]";
}

std::string CodeGenerator::emitArrayLiteral(const ArrayLiteralExpr& expr) {
    emitter_.addInclude("<vector>");
    if (expr.elements.empty()) return "std::vector<int>{}";

    // Infer inner type from first element
    std::string innerType = "auto";
    if (expr.elements[0]->kind == NodeKind::IntLiteral) innerType = "int";
    else if (expr.elements[0]->kind == NodeKind::FloatLiteral) innerType = "double";
    else if (expr.elements[0]->kind == NodeKind::StringLiteral) {
        innerType = "std::string";
        emitter_.addInclude("<string>");
    }
    else if (expr.elements[0]->kind == NodeKind::BoolLiteral) innerType = "bool";

    std::string els;
    for (size_t i = 0; i < expr.elements.size(); i++) {
        if (i > 0) els += ", ";
        els += emitExpression(*expr.elements[i]);
    }
    return "std::vector<" + innerType + ">{" + els + "}";
}

std::string CodeGenerator::emitObjectLiteral(const ObjectLiteralExpr& expr) {
    std::string name = generateAnonStructName();

    // Build struct definition (will be emitted before main)
    // For now embed as a lambda-constructed struct
    std::string structDef = "struct " + name + " {\n";
    for (auto& f : expr.fields) {
        std::string t = "auto";
        if (f.value->kind == NodeKind::IntLiteral) t = "int";
        else if (f.value->kind == NodeKind::FloatLiteral) t = "double";
        else if (f.value->kind == NodeKind::StringLiteral) t = "std::string";
        else if (f.value->kind == NodeKind::BoolLiteral) t = "bool";
        structDef += "    " + t + " " + f.name + ";\n";
    }
    structDef += "}";
    // Note: In a real implementation we'd hoist this. For now inline.

    std::string init = name + "{";
    for (size_t i = 0; i < expr.fields.size(); i++) {
        if (i > 0) init += ", ";
        init += emitExpression(*expr.fields[i].value);
    }
    init += "}";
    return init;
}

std::string CodeGenerator::emitTemplateLiteral(const TemplateLiteralExpr& expr) {
    if (expr.expressions.empty()) {
        return "std::string(\"" + escapeString(expr.strings[0]) + "\")";
    }

    // Use string concatenation: std::string("...") + std::to_string(expr) + ...
    std::string result;
    for (size_t i = 0; i < expr.strings.size(); i++) {
        if (i > 0) result += " + ";
        if (!expr.strings[i].empty()) {
            result += "std::string(\"" + escapeString(expr.strings[i]) + "\")";
            if (i < expr.expressions.size()) result += " + ";
        }
        if (i < expr.expressions.size()) {
            auto exprStr = emitExpression(*expr.expressions[i]);
            // Wrap in to_string for non-string types
            result += "std::to_string(" + exprStr + ")";
        }
    }
    return result;
}

std::string CodeGenerator::emitArrowFunction(const ArrowFunctionExpr& expr) {
    std::string params;
    for (size_t i = 0; i < expr.params.size(); i++) {
        if (i > 0) params += ", ";
        auto& p = *expr.params[i];
        std::string t = p.typeAnnotation ? emitType(*p.typeAnnotation) : "auto";
        params += t + " " + p.name;
    }
    std::string ret = expr.returnType ? " -> " + emitType(*expr.returnType) : "";

    if (expr.hasBlockBody) {
        std::string body = "{\n";
        emitter_.indent();
        for (auto& s : expr.bodyBlock) {
            // Collect into string via temp
            body += emitExpression(*static_cast<const ExpressionStmt*>(s.get())->expr);
        }
        emitter_.dedent();
        // Simpler: just emit block
        return "[=](" + params + ")" + ret + " mutable { /* block */ }";
    }
    return "[=](" + params + ")" + ret + " { return " + emitExpression(*expr.bodyExpr) + "; }";
}

std::string CodeGenerator::emitFunctionExpr(const FunctionExprNode& expr) {
    std::string params;
    for (size_t i = 0; i < expr.params.size(); i++) {
        if (i > 0) params += ", ";
        auto& p = *expr.params[i];
        std::string t = p.typeAnnotation ? emitType(*p.typeAnnotation) : "auto";
        params += t + " " + p.name;
    }
    std::string ret = expr.returnType ? " -> " + emitType(*expr.returnType) : "";

    // For function expressions, capture by value with mutable
    std::string result = "[=](" + params + ")" + ret + " mutable {";
    // We need to emit the body statements ... but we're in expression context
    // For now, mark as block
    result += " /* function body */ }";
    return result;
}

// ============================================================================
// Type mapping
// ============================================================================

std::string CodeGenerator::emitType(const TypeNode& type) {
    if (type.kind == TypeKind::Array) {
        emitter_.addInclude("<vector>");
        if (type.elementType) {
            return "std::vector<" + emitType(*type.elementType) + ">";
        }
        return "std::vector<auto>";
    }
    if (type.kind == TypeKind::Optional) {
        emitter_.addInclude("<optional>");
        if (type.elementType) {
            return "std::optional<" + emitType(*type.elementType) + ">";
        }
        return "std::optional<auto>";
    }
    if (type.kind == TypeKind::Pointer) {
        emitter_.addInclude("<memory>");
        if (type.elementType) {
            return "std::unique_ptr<" + emitType(*type.elementType) + ">";
        }
        return "std::unique_ptr<void>";
    }
    return mapPrimitiveType(type.name);
}

std::string CodeGenerator::mapPrimitiveType(const std::string& jsppType) {
    static const std::unordered_map<std::string, std::string> typeMap = {
        {"int",    "int"},
        {"i8",     "int8_t"},
        {"i16",    "int16_t"},
        {"i32",    "int32_t"},
        {"i64",    "int64_t"},
        {"u8",     "uint8_t"},
        {"u16",    "uint16_t"},
        {"u32",    "uint32_t"},
        {"u64",    "uint64_t"},
        {"float",  "float"},
        {"double", "double"},
        {"bool",   "bool"},
        {"char",   "char"},
        {"string", "std::string"},
        {"void",   "void"},
        {"auto",   "auto"},
        {"size",   "size_t"},
    };
    auto it = typeMap.find(jsppType);
    if (it != typeMap.end()) {
        if (it->second == "std::string") emitter_.addInclude("<string>");
        return it->second;
    }
    return jsppType; // User-defined type name
}

std::string CodeGenerator::generateAnonStructName() {
    return "__anon_" + std::to_string(anonStructCounter_++);
}

bool CodeGenerator::isStringConcatExpr(const Expression& expr) {
    // Returns true if the expression is a + chain involving a string literal
    if (expr.kind == NodeKind::StringLiteral) return true;
    if (expr.kind == NodeKind::BinaryExpr) {
        auto& bin = static_cast<const BinaryExpr&>(expr);
        if (bin.op == "+") {
            return isStringConcatExpr(*bin.left) || isStringConcatExpr(*bin.right);
        }
    }
    return false;
}

} // namespace jspp
