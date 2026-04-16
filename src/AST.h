#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <cassert>
#include <variant>

#include "IR.h"
#include "visitor.h"

class BaseAST {
 public:
  virtual ~BaseAST() = default;

  virtual void Dump() const = 0;

  virtual void Accept(ASTVisitor *visitor) = 0;
};

class CompUnitAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
   std::cout << "CompUnitAST { ";
   
   func_def->Dump();
   std::cout << " }";
  }
  
  std::unique_ptr<BaseAST> func_def;
  
};

// 对于FuncType会直接返回一个类型不会有推导过程
class FuncTypeAST : public BaseAST {
 public:
  std::string type;

  FuncTypeAST(std::string type) : type(type) {}

  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "FuncTypeAST: " <<  "{" << type << "}";
  }
};

class FuncDefAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

 void Dump() const override {
    std::cout << "FuncDefAST { ";
    func_type->Dump();
    std::cout << ", " << ident << ", ";
    block->Dump();
    std::cout << " }";
  }

  std::unique_ptr<BaseAST> func_type;
  std::string ident;
  std::unique_ptr<BaseAST> block;

};

class BlockAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "BlockAST { ";
    stmt->Dump();
    std::cout << " }";
  }

  std::unique_ptr<BaseAST> stmt;
};

class NumberAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "NumberAST: " << "{" << num << "}";
  }

  int num;
};

class StmtAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "StmtAST { ";
    number->Dump();
    std::cout << " }";
  }
  
  Stmt_Type type;
  std::unique_ptr<BaseAST> number;
};

class UnaryOpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "UnaryOpAST { ";
    switch (unaryop) {
    case Unary_Op_Type::AST_UNARY_OP_NEG:
      std::cout << "NEG";
      break;
    case Unary_Op_Type::AST_UNARY_OP_NOT:
      std::cout << "NOT";
      break;
    case Unary_Op_Type::AST_UNARY_OP_POS:
      std::cout << "POS";
      break;
    }
    std::cout << " }";
  }

  void SetOp(const char& op) { 
    switch (op) {
    case '-':
      unaryop = Unary_Op_Type::AST_UNARY_OP_NEG;
      break;
    case '!':
      unaryop = Unary_Op_Type::AST_UNARY_OP_NOT;
      break;
    case '+':
      unaryop = Unary_Op_Type::AST_UNARY_OP_POS;
      break;
    }
  }
  Unary_Op_Type unaryop;
};

// PrimaryExp = ( Exp ) | Number
class PrimaryExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "PrimaryExpAST { ";
    // number->Dump();
    std::cout << " }";
  }

  // 用来指示mem的类型
  int type = -1;

  std::unique_ptr<BaseAST> mem;
 };

// UnaryExp = PrimaryExp | UnaryOp UnaryExp
class UnaryExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "UnaryExprAST { ";
      // unary_op->Dump();
      std::cout << " }";
    }

    // 用来表示mem的类型
    int type = -1;

    struct UnaryExp {
      std::unique_ptr<BaseAST> unary_op;
      std::unique_ptr<BaseAST> unary_exp;
    };

    std::variant<std::unique_ptr<BaseAST>
                , UnaryExp> mem;
    
};

// Exp = UnaryExp
class ExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "ExpAST { ";
      unary_exp->Dump();
      std::cout << " }";
    }

    std::unique_ptr<BaseAST> unary_exp;
};
