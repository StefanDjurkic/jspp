#include "typechecker.hpp"
#include <algorithm>

namespace jspp {

// ============================================================================
// Constructor
// ============================================================================

TypeChecker::TypeChecker(ErrorReporter& errors)
    : errors_(errors), currentScope_(std::make_shared<Scope>()) {
    // Register built-in functions
    currentScope_->declare("print", Symbol{"print", SymbolKind::Function,
                           ResolvedType::makeVoid(), false, true});
}

// ============================================================================
// Entry point
// ============================================================================

void TypeChecker::check(ProgramNode& program) {
    for (auto& stmt : program.statements) {
        checkStatement(*stmt);
    }
}

// ============================================================================
// Scope management
// ============================================================================

void TypeChecker::pushScope() {
    currentScope_ = std::make_shared<Scope>(currentScope_);
}

void TypeChecker::popScope() {
    if (currentScope_->parent()) {
        currentScope_ = currentScope_->parent();
    }
}

// ============================================================================
// Statement checking
// ============================================================================

void TypeChecker::checkStatement(Statement& stmt) {
    switch (stmt.kind) {
        case NodeKind::VariableDecl:
            checkVariableDecl(static_cast<VariableDeclStmt&>(stmt)); break;
        case NodeKind::FunctionDecl:
            checkFunctionDecl(static_cast<FunctionDeclStmt&>(stmt)); break;
        case NodeKind::TypeDecl:
            checkTypeDecl(static_cast<TypeDeclStmt&>(stmt)); break;
        case NodeKind::ClassDecl:
            checkClassDecl(static_cast<ClassDeclStmt&>(stmt)); break;
        case NodeKind::EnumDecl:
            checkEnumDecl(static_cast<EnumDeclStmt&>(stmt)); break;
        case NodeKind::BlockStmt:
            checkBlock(static_cast<BlockStmt&>(stmt)); break;
        case NodeKind::IfStmt:
            checkIf(static_cast<IfStmt_&>(stmt)); break;
        case NodeKind::ForStmt:
            checkFor(static_cast<ForStmt_&>(stmt)); break;
        case NodeKind::ForOfStmt:
            checkForOf(static_cast<ForOfStmt_&>(stmt)); break;
        case NodeKind::WhileStmt:
            checkWhile(static_cast<WhileStmt_&>(stmt)); break;
        case NodeKind::ReturnStmt:
            checkReturn(static_cast<ReturnStmt_&>(stmt)); break;
        case NodeKind::TryStmt:
            checkTryCatch(static_cast<TryStmt_&>(stmt)); break;
        case NodeKind::ExpressionStmt: {
            auto& es = static_cast<ExpressionStmt&>(stmt);
            inferType(*es.expr);
            break;
        }
        case NodeKind::ExportDecl: {
            auto& exp = static_cast<ExportDeclStmt&>(stmt);
            if (exp.declaration) checkStatement(*exp.declaration);
            break;
        }
        case NodeKind::ImportDecl: {
            // Register imported names as AUTO
            auto& imp = static_cast<ImportDeclStmt&>(stmt);
            for (auto& name : imp.namedImports) {
                currentScope_->declare(name, Symbol{name, SymbolKind::Variable,
                                       ResolvedType::makeAuto(), true, true});
            }
            break;
        }
        default:
            break; // CppBlock, CppInclude, break, continue, throw — no checking needed
    }
}

void TypeChecker::checkVariableDecl(VariableDeclStmt& decl) {
    std::shared_ptr<ResolvedType> type;
    if (decl.typeAnnotation) {
        type = resolveTypeNode(*decl.typeAnnotation);
    }
    if (decl.initializer) {
        auto initType = inferType(*decl.initializer);
        if (!type) type = initType;
    }
    if (!type) type = ResolvedType::makeAuto();

    currentScope_->declare(decl.name, Symbol{decl.name, SymbolKind::Variable,
                           type, !decl.isConst, decl.initializer != nullptr});
}

void TypeChecker::checkFunctionDecl(FunctionDeclStmt& decl) {
    // Register function in current scope
    auto returnType = decl.returnType ? resolveTypeNode(*decl.returnType)
                                      : ResolvedType::makeAuto();

    auto funcType = std::make_shared<ResolvedType>();
    funcType->kind = ResolvedTypeKind::Function;
    funcType->name = decl.name;
    funcType->returnType = returnType;
    for (auto& p : decl.params) {
        auto pType = p->typeAnnotation ? resolveTypeNode(*p->typeAnnotation)
                                       : ResolvedType::makeAuto();
        funcType->paramTypes.push_back(pType);
    }

    currentScope_->declare(decl.name, Symbol{decl.name, SymbolKind::Function,
                           funcType, false, true});

    // Check body in new scope
    pushScope();
    for (auto& p : decl.params) {
        auto pType = p->typeAnnotation ? resolveTypeNode(*p->typeAnnotation)
                                       : ResolvedType::makeAuto();
        currentScope_->declare(p->name, Symbol{p->name, SymbolKind::Parameter,
                               pType, true, true});
    }
    if (decl.body) {
        for (auto& s : decl.body->statements) {
            checkStatement(*s);
        }
    }
    popScope();
}

void TypeChecker::checkTypeDecl(TypeDeclStmt& decl) {
    auto structType = std::make_shared<ResolvedType>();
    structType->kind = ResolvedTypeKind::Struct;
    structType->name = decl.name;
    currentScope_->declare(decl.name, Symbol{decl.name, SymbolKind::Type,
                           structType, false, true});
}

void TypeChecker::checkClassDecl(ClassDeclStmt& decl) {
    auto classType = std::make_shared<ResolvedType>();
    classType->kind = ResolvedTypeKind::Class;
    classType->name = decl.name;
    currentScope_->declare(decl.name, Symbol{decl.name, SymbolKind::Class,
                           classType, false, true});
}

void TypeChecker::checkEnumDecl(EnumDeclStmt& decl) {
    auto enumType = std::make_shared<ResolvedType>();
    enumType->kind = ResolvedTypeKind::Enum;
    enumType->name = decl.name;
    currentScope_->declare(decl.name, Symbol{decl.name, SymbolKind::Enum,
                           enumType, false, true});

    // Register each member
    for (auto& m : decl.members) {
        currentScope_->declare(decl.name + "::" + m->name,
                              Symbol{m->name, SymbolKind::EnumMember,
                                     enumType, false, true});
    }
}

void TypeChecker::checkBlock(BlockStmt& block) {
    pushScope();
    for (auto& s : block.statements) {
        checkStatement(*s);
    }
    popScope();
}

void TypeChecker::checkIf(IfStmt_& stmt) {
    if (stmt.condition) inferType(*stmt.condition);
    if (stmt.thenBlock) checkBlock(*stmt.thenBlock);
    if (stmt.elseBlock) checkBlock(*stmt.elseBlock);
}

void TypeChecker::checkFor(ForStmt_& stmt) {
    pushScope();
    if (stmt.init) checkStatement(*stmt.init);
    if (stmt.condition) inferType(*stmt.condition);
    if (stmt.update) inferType(*stmt.update);
    if (stmt.body) {
        for (auto& s : stmt.body->statements) {
            checkStatement(*s);
        }
    }
    popScope();
}

void TypeChecker::checkForOf(ForOfStmt_& stmt) {
    pushScope();
    if (stmt.iterable) inferType(*stmt.iterable);
    currentScope_->declare(stmt.varName, Symbol{stmt.varName, SymbolKind::Variable,
                           ResolvedType::makeAuto(), !stmt.isConst, true});
    if (stmt.body) {
        for (auto& s : stmt.body->statements) {
            checkStatement(*s);
        }
    }
    popScope();
}

void TypeChecker::checkWhile(WhileStmt_& stmt) {
    if (stmt.condition) inferType(*stmt.condition);
    if (stmt.body) checkBlock(*stmt.body);
}

void TypeChecker::checkReturn(ReturnStmt_& stmt) {
    if (stmt.value) inferType(*stmt.value);
}

void TypeChecker::checkTryCatch(TryStmt_& stmt) {
    if (stmt.tryBlock) checkBlock(*stmt.tryBlock);
    if (stmt.catchClause) {
        pushScope();
        auto catchType = stmt.catchClause->paramType
            ? resolveTypeNode(*stmt.catchClause->paramType)
            : ResolvedType::makeAuto();
        currentScope_->declare(stmt.catchClause->paramName,
                              Symbol{stmt.catchClause->paramName, SymbolKind::Variable,
                                     catchType, false, true});
        if (stmt.catchClause->body) {
            for (auto& s : stmt.catchClause->body->statements) {
                checkStatement(*s);
            }
        }
        popScope();
    }
}

// ============================================================================
// Expression type inference
// ============================================================================

std::shared_ptr<ResolvedType> TypeChecker::inferType(Expression& expr) {
    switch (expr.kind) {
        case NodeKind::IntLiteral:     return ResolvedType::makeInt();
        case NodeKind::FloatLiteral:   return ResolvedType::makeDouble();
        case NodeKind::StringLiteral:  return ResolvedType::makeString();
        case NodeKind::CharLiteral: {
            auto t = std::make_shared<ResolvedType>();
            t->kind = ResolvedTypeKind::Char; t->name = "char";
            return t;
        }
        case NodeKind::BoolLiteral:    return ResolvedType::makeBool();
        case NodeKind::NullLiteral:    return ResolvedType::makeAuto();
        case NodeKind::Identifier: {
            auto& id = static_cast<IdentifierExpr&>(expr);
            auto* sym = currentScope_->lookup(id.name);
            if (sym) return sym->type;
            // Don't error — could be a forward reference
            return ResolvedType::makeAuto();
        }
        case NodeKind::BinaryExpr:
            return inferBinary(static_cast<BinaryExpr&>(expr));
        case NodeKind::UnaryExpr:
            return inferUnary(static_cast<UnaryExpr&>(expr));
        case NodeKind::CallExpr:
            return inferCall(static_cast<CallExpr&>(expr));
        case NodeKind::MemberExpr:
            return inferMember(static_cast<MemberExpr&>(expr));
        case NodeKind::IndexExpr:
            return inferIndex(static_cast<IndexExpr&>(expr));
        case NodeKind::AssignExpr: {
            auto& ae = static_cast<AssignExpr&>(expr);
            return inferType(*ae.value);
        }
        case NodeKind::ArrayLiteral: {
            auto& arr = static_cast<ArrayLiteralExpr&>(expr);
            auto arrType = std::make_shared<ResolvedType>();
            arrType->kind = ResolvedTypeKind::Array;
            arrType->name = "Array";
            if (!arr.elements.empty()) {
                arrType->elementType = inferType(*arr.elements[0]);
            } else {
                arrType->elementType = ResolvedType::makeInt();
            }
            return arrType;
        }
        case NodeKind::ArrowFunction:
        case NodeKind::FunctionExpr:
            return ResolvedType::makeAuto();
        case NodeKind::TernaryExpr: {
            auto& te = static_cast<TernaryExpr&>(expr);
            inferType(*te.condition);
            return inferType(*te.thenExpr);
        }
        case NodeKind::NewExpr:
            return ResolvedType::makeAuto();
        case NodeKind::TemplateLiteral:
            return ResolvedType::makeString();
        default:
            return ResolvedType::makeAuto();
    }
}

std::shared_ptr<ResolvedType> TypeChecker::inferBinary(BinaryExpr& expr) {
    auto lt = inferType(*expr.left);
    auto rt = inferType(*expr.right);

    // Comparison operators always return bool
    if (expr.op == "===" || expr.op == "!==" || expr.op == "==" || expr.op == "!=" ||
        expr.op == "<" || expr.op == ">" || expr.op == "<=" || expr.op == ">=") {
        return ResolvedType::makeBool();
    }

    // Logical operators
    if (expr.op == "&&" || expr.op == "||") {
        return ResolvedType::makeBool();
    }

    // String concatenation
    if (lt->kind == ResolvedTypeKind::String || rt->kind == ResolvedTypeKind::String) {
        return ResolvedType::makeString();
    }

    // Numeric promotion
    if (lt->isFloatingPoint() || rt->isFloatingPoint()) {
        return ResolvedType::makeDouble();
    }

    return lt->kind != ResolvedTypeKind::Auto ? lt : rt;
}

std::shared_ptr<ResolvedType> TypeChecker::inferUnary(UnaryExpr& expr) {
    auto t = inferType(*expr.operand);
    if (expr.op == "!") return ResolvedType::makeBool();
    return t;
}

std::shared_ptr<ResolvedType> TypeChecker::inferCall(CallExpr& expr) {
    auto calleeType = inferType(*expr.callee);
    if (calleeType->kind == ResolvedTypeKind::Function && calleeType->returnType) {
        return calleeType->returnType;
    }
    // Infer arg types anyway
    for (auto& arg : expr.args) {
        inferType(*arg);
    }
    return ResolvedType::makeAuto();
}

std::shared_ptr<ResolvedType> TypeChecker::inferMember(MemberExpr& expr) {
    auto objType = inferType(*expr.object);
    if (expr.member == "length" || expr.member == "size") {
        return ResolvedType::makeInt();
    }
    if (objType->kind == ResolvedTypeKind::Struct || objType->kind == ResolvedTypeKind::Class) {
        auto it = objType->fields.find(expr.member);
        if (it != objType->fields.end()) return it->second;
    }
    return ResolvedType::makeAuto();
}

std::shared_ptr<ResolvedType> TypeChecker::inferIndex(IndexExpr& expr) {
    auto objType = inferType(*expr.object);
    inferType(*expr.index);
    if (objType->kind == ResolvedTypeKind::Array && objType->elementType) {
        return objType->elementType;
    }
    return ResolvedType::makeAuto();
}

// ============================================================================
// Type resolution
// ============================================================================

std::shared_ptr<ResolvedType> TypeChecker::resolveTypeNode(TypeNode& type) {
    static const std::unordered_map<std::string, ResolvedTypeKind> primitiveMap = {
        {"int",    ResolvedTypeKind::Int},
        {"i8",     ResolvedTypeKind::I8},
        {"i16",    ResolvedTypeKind::I16},
        {"i32",    ResolvedTypeKind::I32},
        {"i64",    ResolvedTypeKind::I64},
        {"u8",     ResolvedTypeKind::U8},
        {"u16",    ResolvedTypeKind::U16},
        {"u32",    ResolvedTypeKind::U32},
        {"u64",    ResolvedTypeKind::U64},
        {"float",  ResolvedTypeKind::Float},
        {"double", ResolvedTypeKind::Double},
        {"bool",   ResolvedTypeKind::Bool},
        {"string", ResolvedTypeKind::String},
        {"char",   ResolvedTypeKind::Char},
        {"void",   ResolvedTypeKind::Void},
        {"auto",   ResolvedTypeKind::Auto},
    };

    auto t = std::make_shared<ResolvedType>();

    if (type.kind == TypeKind::Array) {
        t->kind = ResolvedTypeKind::Array;
        t->name = "Array";
        if (type.elementType) {
            t->elementType = resolveTypeNode(*type.elementType);
        }
        return t;
    }

    if (type.kind == TypeKind::Optional) {
        // Wrap inner type in optional
        if (type.elementType) {
            t = resolveTypeNode(*type.elementType);
        }
        return t;
    }

    auto it = primitiveMap.find(type.name);
    if (it != primitiveMap.end()) {
        t->kind = it->second;
        t->name = type.name;
    } else {
        // Named user type
        t->kind = ResolvedTypeKind::Class;
        t->name = type.name;
    }
    return t;
}

bool TypeChecker::isAssignable(const ResolvedType& target, const ResolvedType& source) {
    if (target.kind == ResolvedTypeKind::Auto || source.kind == ResolvedTypeKind::Auto)
        return true;
    if (target.kind == source.kind) return true;
    // Numeric promotion
    if (target.isNumeric() && source.isNumeric()) return true;
    return false;
}

} // namespace jspp
