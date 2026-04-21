# JSPP Grammar (EBNF)

This is the formal grammar for the JSPP language.
Notation: `{ x }` = zero or more, `[ x ]` = optional, `|` = alternation, `( )` = grouping.

---

## Top-Level

```ebnf
Program             = { TopLevelStatement } EOF ;

TopLevelStatement   = ImportDecl
                    | ExportDecl
                    | FunctionDecl
                    | TypeDecl
                    | ClassDecl
                    | EnumDecl
                    | VariableDecl
                    | CppIncludeDecl
                    | CppBlock
                    ;
```

## Imports / Exports

```ebnf
ImportDecl          = "import" ( NamedImports | NamespaceImport ) "from" StringLiteral ";" ;
NamedImports        = "{" Identifier { "," Identifier } "}" ;
NamespaceImport     = "*" "as" Identifier ;

ExportDecl          = "export" ( FunctionDecl | TypeDecl | ClassDecl | EnumDecl | VariableDecl ) ;
```

## Variable Declarations

```ebnf
VariableDecl        = ( "let" | "const" ) VariableBinding ";" ;
VariableBinding     = Identifier [ ":" TypeExpr ] "=" Expression
                    | Identifier ":" TypeExpr
                    | DestructureObj "=" Expression
                    | DestructureArr "=" Expression
                    ;

DestructureObj      = "{" Identifier { "," Identifier } "}" ;
DestructureArr      = "[" Identifier { "," Identifier } "]" ;
```

## Type Expressions

```ebnf
TypeExpr            = PrimaryType [ "?" ] [ "[]" { "[]" } ] ;

PrimaryType         = "int" | "i8" | "i16" | "i32" | "i64"
                    | "u8" | "u16" | "u32" | "u64"
                    | "float" | "double"
                    | "bool" | "string" | "char"
                    | "void" | "auto" | "size"
                    | Identifier [ "<" TypeExpr { "," TypeExpr } ">" ]
                    | "ptr" "<" TypeExpr ">"
                    | "(" ParamTypeList ")" "=>" TypeExpr      (* function type *)
                    ;

ParamTypeList       = [ TypeExpr { "," TypeExpr } ] ;
```

## Functions

```ebnf
FunctionDecl        = "function" Identifier [ GenericParams ] "(" ParamList ")" [ ":" TypeExpr ] Block ;

ParamList           = [ Param { "," Param } ] ;
Param               = [ "ref" ] Identifier [ ":" TypeExpr ] [ "=" Expression ] ;

GenericParams       = "<" Identifier { "," Identifier } ">" ;

ArrowFunction       = "(" ParamList ")" [ ":" TypeExpr ] "=>" ( Expression | Block ) ;

FunctionExpr        = "function" [ Identifier ] "(" ParamList ")" [ ":" TypeExpr ] Block ;
```

## Type / Struct Declarations

```ebnf
TypeDecl            = "type" Identifier [ GenericParams ] "=" TypeBody ";" ;

TypeBody            = "{" { StructMember } "}" ;

StructMember        = FieldDecl
                    | MethodDecl
                    | OperatorDecl
                    ;

FieldDecl           = Identifier ":" TypeExpr [ "=" Expression ] ";" ;

MethodDecl          = "function" Identifier [ GenericParams ] "(" ParamList ")" [ ":" TypeExpr ] Block ;

OperatorDecl        = "operator" OperatorSymbol "(" ParamList ")" [ ":" TypeExpr ] Block ;

OperatorSymbol      = "+" | "-" | "*" | "/" | "%" | "===" | "!==" | "<" | ">" | "<=" | ">=" ;
```

## Classes

```ebnf
ClassDecl           = "class" Identifier [ GenericParams ] [ "extends" Identifier [ "<" TypeExpr { "," TypeExpr } ">" ] ] ClassBody ;

ClassBody           = "{" { ClassMember } "}" ;

ClassMember         = FieldDecl
                    | ConstructorDecl
                    | MethodDecl
                    | OperatorDecl
                    ;

ConstructorDecl     = "constructor" "(" ParamList ")" Block ;
```

## Enums

```ebnf
EnumDecl            = "enum" Identifier "{" EnumMember { "," EnumMember } [ "," ] "}" ;

EnumMember          = Identifier [ "=" Expression ] ;
```

## Statements

```ebnf
Statement           = VariableDecl
                    | ExpressionStmt
                    | ReturnStmt
                    | IfStmt
                    | ForStmt
                    | ForOfStmt
                    | WhileStmt
                    | DoWhileStmt
                    | SwitchStmt
                    | TryStmt
                    | ThrowStmt
                    | BreakStmt
                    | ContinueStmt
                    | Block
                    | CppBlock
                    ;

Block               = "{" { Statement } "}" ;

ExpressionStmt      = Expression ";" ;

ReturnStmt          = "return" [ Expression ] ";" ;

BreakStmt           = "break" ";" ;

ContinueStmt        = "continue" ";" ;
```

## Control Flow

```ebnf
IfStmt              = "if" "(" Expression ")" ( Block | Statement ) { "else" "if" "(" Expression ")" ( Block | Statement ) } [ "else" ( Block | Statement ) ] ;

ForStmt             = "for" "(" [ VariableInit | Expression ] ";" [ Expression ] ";" [ Expression ] ")" ( Block | Statement ) ;
VariableInit        = ( "let" | "const" ) Identifier [ ":" TypeExpr ] "=" Expression ;

ForOfStmt           = "for" "(" ( "let" | "const" ) Identifier "of" Expression ")" ( Block | Statement ) ;

WhileStmt           = "while" "(" Expression ")" ( Block | Statement ) ;

DoWhileStmt         = "do" Block "while" "(" Expression ")" ";" ;

SwitchStmt          = "switch" "(" Expression ")" "{" { CaseClause } [ DefaultClause ] "}" ;
CaseClause          = "case" Expression ":" { Statement } ;
DefaultClause       = "default" ":" { Statement } ;
```

## Error Handling

```ebnf
TryStmt             = "try" Block CatchClause ;
CatchClause         = "catch" "(" Identifier ":" TypeExpr ")" Block ;

ThrowStmt           = "throw" Expression ";" ;
```

## Expressions

```ebnf
Expression          = AssignExpr ;

AssignExpr          = TernaryExpr [ AssignOp AssignExpr ] ;
AssignOp            = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

TernaryExpr         = LogicalOrExpr [ "?" Expression ":" Expression ] ;

LogicalOrExpr       = LogicalAndExpr { "||" LogicalAndExpr } ;
LogicalAndExpr      = BitwiseOrExpr { "&&" BitwiseOrExpr } ;
BitwiseOrExpr       = BitwiseXorExpr { "|" BitwiseXorExpr } ;
BitwiseXorExpr      = BitwiseAndExpr { "^" BitwiseAndExpr } ;
BitwiseAndExpr      = EqualityExpr { "&" EqualityExpr } ;
EqualityExpr        = RelationalExpr { ( "===" | "!==" ) RelationalExpr } ;
RelationalExpr      = ShiftExpr { ( "<" | ">" | "<=" | ">=" ) ShiftExpr } ;
ShiftExpr           = AdditiveExpr { ( "<<" | ">>" ) AdditiveExpr } ;
AdditiveExpr        = MultiExpr { ( "+" | "-" ) MultiExpr } ;
MultiExpr           = UnaryExpr { ( "*" | "/" | "%" ) UnaryExpr } ;

UnaryExpr           = ( "!" | "-" | "~" | "++" | "--" ) UnaryExpr
                    | PostfixExpr
                    ;

PostfixExpr         = PrimaryExpr { PostfixOp } ;
PostfixOp           = "++"
                    | "--"
                    | "(" ArgList ")"              (* function call *)
                    | "[" Expression "]"            (* index access *)
                    | "." Identifier                (* member access *)
                    | "." Identifier "(" ArgList ")" (* method call *)
                    ;

ArgList             = [ Expression { "," Expression } ] ;

PrimaryExpr         = IntLiteral
                    | FloatLiteral
                    | StringLiteral
                    | TemplateLiteral
                    | CharLiteral
                    | "true" | "false"
                    | "null"
                    | Identifier
                    | "this"
                    | "(" Expression ")"
                    | ArrayLiteral
                    | ObjectLiteral
                    | ArrowFunction
                    | FunctionExpr
                    | NewExpr
                    | SharedNewExpr
                    ;

ArrayLiteral        = "[" [ Expression { "," Expression } ] "]" ;

ObjectLiteral       = "{" [ ObjField { "," ObjField } [ "," ] ] "}" ;
ObjField            = Identifier ":" Expression ;

NewExpr             = "new" Identifier [ "<" TypeExpr { "," TypeExpr } ">" ] "(" ArgList ")" ;
SharedNewExpr       = "shared_new" Identifier [ "<" TypeExpr { "," TypeExpr } ">" ] "(" ArgList ")" ;

TemplateLiteral     = "`" { TemplateChar | "${" Expression "}" } "`" ;
```

## C++ Interop

```ebnf
CppBlock            = "cpp" "{" RawCppSource "}" ;
CppIncludeDecl      = "cpp_include" StringLiteral ";" ;
```

## Lexical Rules

```ebnf
Identifier          = Letter { Letter | Digit | "_" } ;
Letter              = "a".."z" | "A".."Z" | "_" ;
Digit               = "0".."9" ;

IntLiteral          = Digit { Digit }
                    | "0x" HexDigit { HexDigit }
                    | "0b" BinDigit { BinDigit }
                    ;
FloatLiteral        = Digit { Digit } "." Digit { Digit } [ ( "e" | "E" ) [ "+" | "-" ] Digit { Digit } ] ;
StringLiteral       = '"' { StringChar } '"' ;
CharLiteral         = "'" SingleChar "'" ;

HexDigit            = Digit | "a".."f" | "A".."F" ;
BinDigit            = "0" | "1" ;
StringChar          = <any char except '"' and '\\'> | EscapeSeq ;
SingleChar          = <any char except "'" and '\\'> | EscapeSeq ;
EscapeSeq           = "\\" ( "n" | "t" | "r" | "\\" | '"' | "'" | "0" ) ;

LineComment         = "//" { <any char except newline> } ;
BlockComment        = "/*" { <any char> } "*/" ;
```
