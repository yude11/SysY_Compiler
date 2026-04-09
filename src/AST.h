#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <cassert>

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

  std::unique_ptr<BaseAST> number;
};
