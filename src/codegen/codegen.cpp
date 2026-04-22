#include "codegen.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <functional>

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
    inferredArrayStructs_.clear();

    // ------------------------------------------------------------------
    // Pre-pass: infer element struct types for `let xs = []` that are
    // later populated with `xs.push({ ... })` somewhere in the program.
    // Without this, xs would be emitted as std::vector<int>{} and every
    // later push of an object would fail to compile.
    // ------------------------------------------------------------------
    std::unordered_map<std::string, const VariableDeclStmt*> emptyArrayVars;

    std::function<void(const Statement&)> collectDecls;
    std::function<void(const Statement&)> collectPushes;
    std::function<void(const Expression&, const std::function<void(const Expression&)>&)> walkExpr;
    std::function<void(const Statement&, const std::function<void(const Statement&)>&,
                       const std::function<void(const Expression&)>&)> walkStmt;

    walkExpr = [&](const Expression& e, const std::function<void(const Expression&)>& fn) {
        fn(e);
        switch (e.kind) {
            case NodeKind::BinaryExpr: {
                auto& x = static_cast<const BinaryExpr&>(e);
                if (x.left) walkExpr(*x.left, fn);
                if (x.right) walkExpr(*x.right, fn);
                break;
            }
            case NodeKind::UnaryExpr: {
                auto& x = static_cast<const UnaryExpr&>(e);
                if (x.operand) walkExpr(*x.operand, fn);
                break;
            }
            case NodeKind::CallExpr: {
                auto& x = static_cast<const CallExpr&>(e);
                if (x.callee) walkExpr(*x.callee, fn);
                for (auto& a : x.args) if (a) walkExpr(*a, fn);
                break;
            }
            case NodeKind::MemberExpr: {
                auto& x = static_cast<const MemberExpr&>(e);
                if (x.object) walkExpr(*x.object, fn);
                break;
            }
            case NodeKind::IndexExpr: {
                auto& x = static_cast<const IndexExpr&>(e);
                if (x.object) walkExpr(*x.object, fn);
                if (x.index) walkExpr(*x.index, fn);
                break;
            }
            case NodeKind::ArrayLiteral: {
                auto& x = static_cast<const ArrayLiteralExpr&>(e);
                for (auto& el : x.elements) if (el) walkExpr(*el, fn);
                break;
            }
            case NodeKind::ObjectLiteral: {
                auto& x = static_cast<const ObjectLiteralExpr&>(e);
                for (auto& f : x.fields) if (f.value) walkExpr(*f.value, fn);
                break;
            }
            default: break;
        }
    };

    walkStmt = [&](const Statement& s,
                   const std::function<void(const Statement&)>& sfn,
                   const std::function<void(const Expression&)>& efn) {
        sfn(s);
        auto recur = [&](const Statement& ss) { walkStmt(ss, sfn, efn); };
        auto recurE = [&](const Expression& ee) { walkExpr(ee, efn); };
        switch (s.kind) {
            case NodeKind::BlockStmt: {
                auto& x = static_cast<const BlockStmt&>(s);
                for (auto& c : x.statements) if (c) recur(*c);
                break;
            }
            case NodeKind::ExpressionStmt: {
                auto& x = static_cast<const ExpressionStmt&>(s);
                if (x.expr) recurE(*x.expr);
                break;
            }
            case NodeKind::VariableDecl: {
                auto& x = static_cast<const VariableDeclStmt&>(s);
                if (x.initializer) recurE(*x.initializer);
                break;
            }
            case NodeKind::FunctionDecl: {
                auto& x = static_cast<const FunctionDeclStmt&>(s);
                if (x.body) recur(*x.body);
                break;
            }
            case NodeKind::IfStmt: {
                auto& x = static_cast<const IfStmt_&>(s);
                if (x.condition) recurE(*x.condition);
                if (x.thenBlock) recur(*x.thenBlock);
                for (auto& ei : x.elseIfBlocks) {
                    if (ei.first) recurE(*ei.first);
                    if (ei.second) recur(*ei.second);
                }
                if (x.elseBlock) recur(*x.elseBlock);
                break;
            }
            case NodeKind::ForStmt: {
                auto& x = static_cast<const ForStmt_&>(s);
                if (x.init) recur(*x.init);
                if (x.condition) recurE(*x.condition);
                if (x.update) recurE(*x.update);
                if (x.body) recur(*x.body);
                break;
            }
            case NodeKind::ForOfStmt: {
                auto& x = static_cast<const ForOfStmt_&>(s);
                if (x.iterable) recurE(*x.iterable);
                if (x.body) recur(*x.body);
                break;
            }
            case NodeKind::WhileStmt: {
                auto& x = static_cast<const WhileStmt_&>(s);
                if (x.condition) recurE(*x.condition);
                if (x.body) recur(*x.body);
                break;
            }
            case NodeKind::DoWhileStmt: {
                auto& x = static_cast<const DoWhileStmt_&>(s);
                if (x.body) recur(*x.body);
                if (x.condition) recurE(*x.condition);
                break;
            }
            case NodeKind::ReturnStmt: {
                auto& x = static_cast<const ReturnStmt_&>(s);
                if (x.value) recurE(*x.value);
                break;
            }
            case NodeKind::ThrowStmt: {
                auto& x = static_cast<const ThrowStmt_&>(s);
                if (x.value) recurE(*x.value);
                break;
            }
            case NodeKind::SwitchStmt: {
                auto& x = static_cast<const SwitchStmt_&>(s);
                if (x.discriminant) recurE(*x.discriminant);
                for (auto& c : x.cases) {
                    if (c->value) recurE(*c->value);
                    for (auto& b : c->body) if (b) recur(*b);
                }
                if (x.defaultCase) {
                    for (auto& b : x.defaultCase->body) if (b) recur(*b);
                }
                break;
            }
            case NodeKind::TryStmt: {
                auto& x = static_cast<const TryStmt_&>(s);
                if (x.tryBlock) recur(*x.tryBlock);
                if (x.catchClause && x.catchClause->body) recur(*x.catchClause->body);
                break;
            }
            case NodeKind::ExportDecl: {
                auto& x = static_cast<const ExportDeclStmt&>(s);
                if (x.declaration) recur(*x.declaration);
                break;
            }
            default: break;
        }
    };

    // Pass 1: record every `let v = []` (empty array literal, no annotation).
    //   Also record `let name = new ClassName(...)` so a later push(name)
    //   can resolve to std::vector<ClassName>.
    std::unordered_map<std::string, std::string> newBoundVars;
    auto collectEmpty = [&](const Statement& s) {
        if (s.kind != NodeKind::VariableDecl) return;
        auto& v = static_cast<const VariableDeclStmt&>(s);
        if (v.initializer && v.initializer->kind == NodeKind::NewExpr) {
            auto& ne = static_cast<const NewExprNode&>(*v.initializer);
            if (!ne.typeName.empty()) newBoundVars[v.name] = ne.typeName;
        }
        if (v.typeAnnotation) return;
        if (!v.initializer || v.initializer->kind != NodeKind::ArrayLiteral) return;
        auto& arr = static_cast<const ArrayLiteralExpr&>(*v.initializer);
        if (!arr.elements.empty()) return;
        emptyArrayVars[v.name] = &v;
    };

    // Pass 2: find `name.push({...})` on any empty-array var. First hit wins.
    auto inferCppFieldType = [&](const Expression& fv) -> std::string {
        switch (fv.kind) {
            case NodeKind::IntLiteral:    return "int";
            case NodeKind::FloatLiteral:  return "double";
            case NodeKind::StringLiteral: emitter_.addInclude("<string>"); return "std::string";
            case NodeKind::BoolLiteral:   return "bool";
            case NodeKind::CallExpr: {
                auto& c = static_cast<const CallExpr&>(fv);
                if (c.callee && c.callee->kind == NodeKind::Identifier) {
                    auto& id = static_cast<const IdentifierExpr&>(*c.callee).name;
                    if (id == "randInt") return "int";
                    if (id == "rand") return "double";
                }
                return "double";
            }
            default: return "double";  // safest numeric default for computed exprs
        }
    };

    auto collectPushExprs = [&](const Expression& e) {
        if (e.kind != NodeKind::CallExpr) return;
        auto& c = static_cast<const CallExpr&>(e);
        if (!c.callee || c.callee->kind != NodeKind::MemberExpr) return;
        auto& me = static_cast<const MemberExpr&>(*c.callee);
        if (me.member != "push") return;
        if (!me.object || me.object->kind != NodeKind::Identifier) return;
        auto& id = static_cast<const IdentifierExpr&>(*me.object);
        auto it = emptyArrayVars.find(id.name);
        if (it == emptyArrayVars.end()) return;
        if (inferredArrayStructs_.count(id.name)) return;  // already inferred
        if (c.args.empty() || !c.args[0]) return;

        // Case 1: push({...}) — synthesize an anonymous struct from the object literal.
        if (c.args[0]->kind == NodeKind::ObjectLiteral) {
            auto& obj = static_cast<const ObjectLiteralExpr&>(*c.args[0]);
            InferredArrayStruct info;
            info.structName = generateAnonStructName();
            std::string def = "struct " + info.structName + " {\n";
            for (auto& f : obj.fields) {
                std::string t = inferCppFieldType(*f.value);
                info.fields.push_back({f.name, t});
                def += "    " + t + " " + f.name + ";\n";
            }
            def += "};\n";
            emitter_.addPrelude(def);
            inferredArrayStructs_[id.name] = std::move(info);
            return;
        }

        // Case 2: push(new ClassName(...)) — use the class as the element type.
        if (c.args[0]->kind == NodeKind::NewExpr) {
            auto& ne = static_cast<const NewExprNode&>(*c.args[0]);
            if (!ne.typeName.empty()) {
                InferredArrayStruct info;
                info.structName = ne.typeName;  // user-defined class name
                inferredArrayStructs_[id.name] = std::move(info);
                return;
            }
        }

        // Case 3: push(name) where `let name = new ClassName(...)` earlier.
        if (c.args[0]->kind == NodeKind::Identifier) {
            auto& aid = static_cast<const IdentifierExpr&>(*c.args[0]);
            auto nbIt = newBoundVars.find(aid.name);
            if (nbIt != newBoundVars.end()) {
                InferredArrayStruct info;
                info.structName = nbIt->second;
                inferredArrayStructs_[id.name] = std::move(info);
                return;
            }
        }
    };

    auto noopS = std::function<void(const Statement&)>([&](const Statement& s){ collectEmpty(s); });
    auto noopE1 = std::function<void(const Expression&)>([&](const Expression&){});
    for (auto& stmt : program.statements) {
        if (stmt) walkStmt(*stmt, noopS, noopE1);
    }
    auto noopS2 = std::function<void(const Statement&)>([&](const Statement&){});
    auto pushFn = std::function<void(const Expression&)>([&](const Expression& e){ collectPushExprs(e); });
    for (auto& stmt : program.statements) {
        if (stmt) walkStmt(*stmt, noopS2, pushFn);
    }

    // ------------------------------------------------------------------
    // Two-pass split: separate declarations from executable statements.
    // ------------------------------------------------------------------
    // Top-level `let` / `const` are hoisted into a 'globals' bucket emitted
    // before the function decls so that nested functions (which become real
    // C++ free functions) can reference them.
    std::vector<const Statement*> decls;
    std::vector<const Statement*> typeDecls;   // classes/types/enums emitted BEFORE globals
    std::vector<const Statement*> globals;
    std::vector<const Statement*> exec;
    bool hasMain = false;
    bool hasTick = false;
    int  tickArity = 1;

    for (auto& stmt : program.statements) {
        if (stmt->kind == NodeKind::FunctionDecl) {
            auto* fd = static_cast<const FunctionDeclStmt*>(stmt.get());
            if (fd->name == "main") hasMain = true;
            if (fd->name == "tick") { hasTick = true; tickArity = (int)fd->params.size(); }
            decls.push_back(stmt.get());
        } else if (stmt->kind == NodeKind::ClassDecl || stmt->kind == NodeKind::TypeDecl ||
                   stmt->kind == NodeKind::EnumDecl) {
            // Emit these BEFORE globals so `let v: MyClass = ...` and
            // auto-inferred `std::vector<MyClass>` globals see a complete type.
            typeDecls.push_back(stmt.get());
        } else if (stmt->kind == NodeKind::ImportDecl ||
                   stmt->kind == NodeKind::ExportDecl || stmt->kind == NodeKind::CppInclude ||
                   stmt->kind == NodeKind::CppBlock) {
            decls.push_back(stmt.get());
        } else if (stmt->kind == NodeKind::VariableDecl) {
            globals.push_back(stmt.get());
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

    // ZeroEngine graphics builtins. Each builtin prints one line of the
    // ZeroEngine visual protocol to stdout so the playground can replay
    // the program's draw calls on its canvas. Lines starting with '@' are
    // engine ops; anything else is regular user print() output.
    //
    //   @C r g b                 clear(r,g,b)
    //   @R x y w h r g b         drawRect
    //   @O x y rad r g b         drawCircle
    //   @L x1 y1 x2 y2 r g b     drawLine
    //   @S rx ry                 setRotation (3D)
    //   @Z s                     setScale (3D)
    //   @K r g b a               setFaceColor (3D)
    //   @F <n>                   start of frame n
    //   @E                       end of animation
    emitter_.addInclude("<random>");
    emitter_.emitLine("// ---- ZeroEngine visual-protocol stubs ----");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_clear(int r=0,int g=0,int b=0) {");
    emitter_.emitLine("    std::cout << \"@C \" << r << ' ' << g << ' ' << b << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_drawRect(double x,double y,double w,double h,int r=255,int g=255,int b=255) {");
    emitter_.emitLine("    std::cout << \"@R \" << x << ' ' << y << ' ' << w << ' ' << h << ' ' << r << ' ' << g << ' ' << b << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_drawCircle(double x,double y,double rad,int r=255,int g=255,int b=255) {");
    emitter_.emitLine("    std::cout << \"@O \" << x << ' ' << y << ' ' << rad << ' ' << r << ' ' << g << ' ' << b << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_drawLine(double x1,double y1,double x2,double y2,int r=255,int g=255,int b=255) {");
    emitter_.emitLine("    std::cout << \"@L \" << x1 << ' ' << y1 << ' ' << x2 << ' ' << y2 << ' ' << r << ' ' << g << ' ' << b << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_setRotation(double rx,double ry) {");
    emitter_.emitLine("    std::cout << \"@S \" << rx << ' ' << ry << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_setScale(double s) {");
    emitter_.emitLine("    std::cout << \"@Z \" << s << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static void __jspp_setFaceColor(int idx,int r,int g,int b) {");
    emitter_.emitLine("    std::cout << \"@K \" << idx << ' ' << r << ' ' << g << ' ' << b << '\\n';");
    emitter_.emitLine("}");
    emitter_.emitLine("[[maybe_unused]] static double __jspp_rand() { static thread_local std::mt19937_64 __r{std::random_device{}()}; static thread_local std::uniform_real_distribution<double> __d(0.0,1.0); return __d(__r); }");
    emitter_.emitLine("[[maybe_unused]] static int __jspp_randInt(int lo,int hi) { static thread_local std::mt19937_64 __r{std::random_device{}()}; std::uniform_int_distribution<int> __d(lo,hi); return __d(__r); }");
    emitter_.emitLine("#define clear        __jspp_clear");
    emitter_.emitLine("#define drawRect     __jspp_drawRect");
    emitter_.emitLine("#define drawCircle   __jspp_drawCircle");
    emitter_.emitLine("#define drawLine     __jspp_drawLine");
    emitter_.emitLine("#define setRotation  __jspp_setRotation");
    emitter_.emitLine("#define setScale     __jspp_setScale");
    emitter_.emitLine("#define setFaceColor __jspp_setFaceColor");
    emitter_.emitLine("#define rand         __jspp_rand");
    emitter_.emitLine("#define randInt      __jspp_randInt");
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

    // Emit hoisted top-level variables as C++ globals BEFORE function decls
    // so that functions that reference them compile cleanly. Class/type/enum
    // decls go even earlier, because globals (or inferred vector<Class> type
    // of a global) may depend on them being complete types.
    for (auto* d : typeDecls) {
        emitStatement(*d);
    }
    for (auto* g : globals) {
        emitStatement(*g);
    }

    // Emit remaining declarations (functions, imports, cpp blocks)
    for (auto* d : decls) {
        emitStatement(*d);
    }

    // Wrap executable statements in main() if needed.
    // If the user defined `tick(...)` and no explicit main, synthesize a
    // frame-loop main that runs any top-level exec statements first (their
    // init work), then calls tick() for a bounded number of frames and
    // emits frame-boundary markers so the playground can replay on canvas.
    if (!hasMain && (hasTick || !exec.empty())) {
        emitter_.emitLine("int main() {");
        emitter_.indent();
        emitter_.emitIndentedLine("std::cout.setf(std::ios::fixed); std::cout.precision(3);");
        for (auto* s : exec) {
            emitStatement(*s);
        }
        if (hasTick) {
            emitter_.emitIndentedLine("const int __JSPP_FRAMES = 240;");
            emitter_.emitIndentedLine("const double __JSPP_DT = 1.0 / 60.0;");
            emitter_.emitIndentedLine("for (int __f = 0; __f < __JSPP_FRAMES; ++__f) {");
            emitter_.indent();
            emitter_.emitIndentedLine("std::cout << \"@F \" << __f << '\\n';");
            if (tickArity >= 1) {
                emitter_.emitIndentedLine("tick(__f * __JSPP_DT);");
            } else {
                emitter_.emitIndentedLine("tick();");
            }
            emitter_.dedent();
            emitter_.emitIndentedLine("}");
            emitter_.emitIndentedLine("std::cout << \"@E\\n\";");
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
        // If this is an empty array literal and the pre-pass found a later
        // push of an object literal, use the inferred struct element type.
        if (cppType.empty() &&
            decl.initializer->kind == NodeKind::ArrayLiteral &&
            static_cast<const ArrayLiteralExpr&>(*decl.initializer).elements.empty()) {
            auto it = inferredArrayStructs_.find(decl.name);
            if (it != inferredArrayStructs_.end()) {
                emitter_.addInclude("<vector>");
                cppType = "std::vector<" + it->second.structName + ">";
                emitter_.emitIndentedLine(prefix + cppType + " " + decl.name + ";");
                return;
            }
        }
        if (cppType.empty()) {
            // Recursively infer inner type for array literals so nested arrays
            // become std::vector<std::vector<int>> and not std::vector<auto>.
            std::function<std::string(const Expression&)> infer = [&](const Expression& x) -> std::string {
                switch (x.kind) {
                    case NodeKind::IntLiteral:    return "int";
                    case NodeKind::FloatLiteral:  return "double";
                    case NodeKind::StringLiteral: emitter_.addInclude("<string>"); return "std::string";
                    case NodeKind::BoolLiteral:   return "bool";
                    case NodeKind::ArrayLiteral: {
                        auto& a = static_cast<const ArrayLiteralExpr&>(x);
                        emitter_.addInclude("<vector>");
                        if (a.elements.empty()) return "std::vector<int>";
                        return "std::vector<" + infer(*a.elements[0]) + ">";
                    }
                    default: return "";
                }
            };
            std::string inferred = infer(*decl.initializer);
            cppType = inferred.empty() ? "auto" : inferred;
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
            // If pushing an object literal into a var with an inferred
            // element struct type, emit it as StructName{field,field,...}
            // in the struct's canonical field order.
            if (me.object->kind == NodeKind::Identifier && !expr.args.empty() &&
                expr.args[0] && expr.args[0]->kind == NodeKind::ObjectLiteral) {
                auto& id = static_cast<const IdentifierExpr&>(*me.object);
                auto itS = inferredArrayStructs_.find(id.name);
                if (itS != inferredArrayStructs_.end()) {
                    auto& obl = static_cast<const ObjectLiteralExpr&>(*expr.args[0]);
                    std::string parts;
                    for (size_t i = 0; i < itS->second.fields.size(); i++) {
                        if (i > 0) parts += ", ";
                        const auto& fname = itS->second.fields[i].first;
                        const Expression* found = nullptr;
                        for (auto& fld : obl.fields) {
                            if (fld.name == fname) { found = fld.value.get(); break; }
                        }
                        parts += found ? emitExpression(*found) : "0";
                    }
                    return obj + ".push_back(" + itS->second.structName + "{" + parts + "})";
                }
            }
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
        // Transcendental / trig
        if (expr.member == "sin")   return "std::sin";
        if (expr.member == "cos")   return "std::cos";
        if (expr.member == "tan")   return "std::tan";
        if (expr.member == "asin")  return "std::asin";
        if (expr.member == "acos")  return "std::acos";
        if (expr.member == "atan")  return "std::atan";
        if (expr.member == "atan2") return "std::atan2";
        if (expr.member == "sinh")  return "std::sinh";
        if (expr.member == "cosh")  return "std::cosh";
        if (expr.member == "tanh")  return "std::tanh";
        if (expr.member == "exp")   return "std::exp";
        if (expr.member == "log")   return "std::log";
        if (expr.member == "log2")  return "std::log2";
        if (expr.member == "log10") return "std::log10";
        if (expr.member == "pow")   return "std::pow";
        if (expr.member == "sqrt")  return "std::sqrt";
        if (expr.member == "cbrt")  return "std::cbrt";
        if (expr.member == "abs")   return "std::abs";
        if (expr.member == "floor") return "std::floor";
        if (expr.member == "ceil")  return "std::ceil";
        if (expr.member == "round") return "std::round";
        if (expr.member == "trunc") return "std::trunc";
        if (expr.member == "sign")  return "((double(*)(double)) [](double x){ return (x>0)-(x<0); })";
        if (expr.member == "max")   return "std::max";
        if (expr.member == "min")   return "std::min";
        // Constants (use literals: M_PI / M_E are POSIX, not standard C++).
        if (expr.member == "PI")    return "3.14159265358979323846";
        if (expr.member == "E")     return "2.71828182845904523536";
        if (expr.member == "LN2")   return "0.69314718055994530942";
        if (expr.member == "LN10")  return "2.30258509299404568402";
        if (expr.member == "SQRT2") return "1.41421356237309504880";
        // Pseudo: Math.random() → a tiny inline RNG.
        if (expr.member == "random") {
            emitter_.addInclude("<random>");
            return "([](){ static thread_local std::mt19937_64 __r{std::random_device{}()}; static thread_local std::uniform_real_distribution<double> __d(0.0,1.0); return __d(__r); })";
        }
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

    // Infer inner type recursively so nested arrays become
    // std::vector<std::vector<int>> instead of std::vector<auto>.
    auto inferInner = [&](const Expression& e) -> std::string {
        std::function<std::string(const Expression&)> rec = [&](const Expression& x) -> std::string {
            switch (x.kind) {
                case NodeKind::IntLiteral:    return "int";
                case NodeKind::FloatLiteral:  return "double";
                case NodeKind::StringLiteral: emitter_.addInclude("<string>"); return "std::string";
                case NodeKind::BoolLiteral:   return "bool";
                case NodeKind::ArrayLiteral: {
                    auto& a = static_cast<const ArrayLiteralExpr&>(x);
                    if (a.elements.empty()) return "std::vector<int>";
                    return "std::vector<" + rec(*a.elements[0]) + ">";
                }
                default: return "auto";
            }
        };
        return rec(e);
    };
    std::string innerType = inferInner(*expr.elements[0]);

    std::string els;
    for (size_t i = 0; i < expr.elements.size(); i++) {
        if (i > 0) els += ", ";
        els += emitExpression(*expr.elements[i]);
    }
    return "std::vector<" + innerType + ">{" + els + "}";
}

std::string CodeGenerator::emitObjectLiteral(const ObjectLiteralExpr& expr) {
    std::string name = generateAnonStructName();

    // Emit the struct definition into the prelude so it lives at file scope
    // above any use. Each object literal becomes its own anonymous struct type.
    std::string structDef = "struct " + name + " {\n";
    for (auto& f : expr.fields) {
        std::string t = "auto";
        if (f.value->kind == NodeKind::IntLiteral) t = "int";
        else if (f.value->kind == NodeKind::FloatLiteral) t = "double";
        else if (f.value->kind == NodeKind::StringLiteral) { t = "std::string"; emitter_.addInclude("<string>"); }
        else if (f.value->kind == NodeKind::BoolLiteral) t = "bool";
        structDef += "    " + t + " " + f.name + ";\n";
    }
    structDef += "};\n";
    emitter_.addPrelude(structDef);

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
        {"number", "double"},
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
