#pragma once
// IR中的Type相关的类型定义

// Koopa中Value的类型
typedef enum {
  /// Integer constant.
  KOOPA_RVT_INTEGER,
  /// Zero initializer.
  KOOPA_RVT_ZERO_INIT,
  /// Undefined value.
  KOOPA_RVT_UNDEF,
  /// Aggregate constant.
  KOOPA_RVT_AGGREGATE,
  /// Function argument reference.
  KOOPA_RVT_FUNC_ARG_REF,
  /// Basic block argument reference.
  KOOPA_RVT_BLOCK_ARG_REF,
  /// Local memory allocation.
  KOOPA_RVT_ALLOC,
  /// Global memory allocation.
  KOOPA_RVT_GLOBAL_ALLOC,
  /// Memory load.
  KOOPA_RVT_LOAD,
  /// Memory store.
  KOOPA_RVT_STORE,
  /// Pointer calculation.
  KOOPA_RVT_GET_PTR,
  /// Element pointer calculation.
  KOOPA_RVT_GET_ELEM_PTR,
  /// Binary operation.
  KOOPA_RVT_BINARY,
  /// Conditional branch.
  KOOPA_RVT_BRANCH,
  /// Unconditional jump.
  KOOPA_RVT_JUMP,
  /// Function call.
  KOOPA_RVT_CALL,
  /// Function return.
  KOOPA_RVT_RETURN,
  /// Void value.
  KOOPA_RVT_VOID,
} Value_Type;

// Koopa中二元操作符的类型
typedef enum {
  /// Not equal to.
  KOOPA_RBO_NOT_EQ,
  /// Equal to.
  KOOPA_RBO_EQ,
  /// Greater than.
  KOOPA_RBO_GT,
  /// Less than.
  KOOPA_RBO_LT,
  /// Greater than or equal to.
  KOOPA_RBO_GE,
  /// Less than or equal to.
  KOOPA_RBO_LE,
  /// Addition.
  KOOPA_RBO_ADD,
  /// Subtraction.
  KOOPA_RBO_SUB,
  /// Multiplication.
  KOOPA_RBO_MUL,
  /// Division.
  KOOPA_RBO_DIV,
  /// Modulo.
  KOOPA_RBO_MOD,
  /// Bitwise AND.
  KOOPA_RBO_AND,
  /// Bitwise OR.
  KOOPA_RBO_OR,
  /// Bitwise XOR.
  KOOPA_RBO_XOR,
  /// Shift left logical.
  KOOPA_RBO_SHL,
  /// Shift right logical.
  KOOPA_RBO_SHR,
  /// Shift right arithmetic.
  KOOPA_RBO_SAR,
} Binary_Op_Type;

// AST中的Type相关的类型定义
// 操作符的类型
typedef enum {
    /// Negation.
    AST_UNARY_OP_NEG,
    /// Logical NOT.
    AST_UNARY_OP_NOT,
    /// Positive.
    AST_UNARY_OP_POS,
    /// Multiplication.

    // 二元算术运算符
    AST_BINARY_OP_MUL,
    /// Division.
    AST_BINARY_OP_DIV,
    /// Addition.
    AST_BINARY_OP_ADD,
    /// Subtraction.
    AST_BINARY_OP_SUB,
    /// Modulo.
    AST_BINARY_OP_MOD,
    
    // 二元比较运算符
    /// Less than.
    AST_BINARY_OP_LT,
    /// Greater than.
    AST_BINARY_OP_GT,
    /// Greater than or equal to.
    AST_BINARY_OP_GE,
    /// Less than or equal to.
    AST_BINARY_OP_LE,
    /// Equal to.
    AST_BINARY_OP_EQ,
    /// Not equal to.
    AST_BINARY_OP_NE,

    // 二元逻辑运算符
    /// Logical AND.
    AST_BINARY_OP_LA,
    /// Logical OR.
    AST_BINARY_OP_LO,
} Op_Type;

typedef enum {
    /// Return.
    AST_STMT_RETURN,
    /// Assignment.
    AST_STMT_ASSIGN,
    /// Block statement.
    AST_STMT_BLOCK,
    /// If-Else statement.
    AST_STMT_IF_ELSE,
    /// While statement.
    AST_STMT_WHILE,
    /// Break statement.
    AST_STMT_BREAK,
    /// Continue statement.
    AST_STMT_CONTINUE,
    /// Expression statement.
    AST_STMT_EXP,
} Stmt_Type;

//变量，函数返回值的类型 
typedef enum {
  /// Integer.
  AST_TYPE_INTEGER,
  /// Void.
  AST_TYPE_VOID,
  /// Aggregate.
} AST_Type;


