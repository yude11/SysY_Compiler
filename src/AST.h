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

class OpAST : public BaseAST {
  public:
    OpAST(Op_Type op) : op(op) {}

    void Dump() const override {
      std::cout << "OpAST: " << "{" << op << "}";
    }

    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }
    Op_Type op;
};

class UnaryOpAST : public OpAST {
 public:
  UnaryOpAST(Op_Type op) : OpAST(op) {}

  void Dump() const override {
    std::cout << "UnaryOpAST { ";
    // unary_op->Dump();
    std::cout << " }";
  }

  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }
};

class BinaryOpAST : public OpAST {
  public:
    BinaryOpAST(Op_Type op) : OpAST(op) {}

    void Dump() const override {
      std::cout << "BinaryOpAST { ";
      // binary_op->Dump();
      std::cout << " }";
    }

    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }
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

// Exp = AddExp
class ExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "ExpAST { ";
      add_exp->Dump();
      std::cout << " }";
    }

    std::unique_ptr<BaseAST> add_exp;
};

// AddExp : MulExp | AddExp AddOp MulExp
class AddExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "AddExpAST { ";
      // add_exp->Dump();
      std::cout << " }";
    }

    int type = -1;
    struct AddExp {
      std::unique_ptr<BaseAST> add_exp;
      std::unique_ptr<BaseAST> add_op;
      std::unique_ptr<BaseAST> mul_exp;
    };
    std::variant<std::unique_ptr<BaseAST>
                , AddExp> mem;
};


// MulExp =  UnaryExp | MulExp MulOp UnaryExp
class MulExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "MulExpAST { ";
    std::cout << " }";
  }

  int type = -1;
  struct MulExp {
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> mul_op;
    std::unique_ptr<BaseAST> unary_exp;
  };
  std::variant<std::unique_ptr<BaseAST>
                , MulExp> mem;
};
