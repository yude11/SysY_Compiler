// 数据结构设计参照文档https://pku-minic.github.io/online-doc/#/misc-app-ref/libkoopa
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <fstream>

#include "visitor.h"
#include "type.h"


class Type {
  public:
    Type(std::string name) : name(name) {}
    Type() {}
    // 表示是什么类型的Value
    std::string name;
};

// 全局变量和指令都是Value
class Value {
  public:
    Value(std::string name, Value_Type type) : name(name), type(type) {}
    
    virtual ~Value() = default;
    
    virtual void Accept(IRVisitor* visitor) = 0;

    // 表示Value的名字，如%0
    std::string name;
    // 表示Value的类型
    Value_Type type;
    // 被那些结构使用used_by
    //TODO
};

class Value_INTEGER : public Value {
  public:
    Value_INTEGER(int val) : Value(std::to_string(val), Value_Type::KOOPA_RVT_INTEGER), val(val) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    int val;
};

class Value_REF : public Value {
 public:
  Value_REF(std::string name) 
    : Value(name, Value_Type::KOOPA_RVT_REF) {}  // 类型可以是 INTEGER 或通用类型

  void Accept(IRVisitor* visitor) override {
    visitor->Visit(this);
  }
};

// 二元操作语句
class Value_BINARY : public Value {
  public:
    Value_BINARY(std::string name, std::shared_ptr<Value> lhs, std::shared_ptr<Value> rhs, Binary_Op_Type op)
     : Value(name, Value_Type::KOOPA_RVT_BINARY), type(op), lhs(lhs), rhs(rhs) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    // 二元操作符类型
    Binary_Op_Type type;
    std::shared_ptr<Value> lhs;
    std::shared_ptr<Value> rhs;
};

// 返回语句
class Value_RETURN : public Value {
  public:
    Value_RETURN(std::shared_ptr<Value> val) 
     : Value("null", Value_Type::KOOPA_RVT_RETURN), val(val) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    std::shared_ptr<Value> val;
};

// 基本块
class BasicBlock {
  public:
    BasicBlock(std::string name) : name(name) {}

    ~BasicBlock() {}

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    std::string name;
    std::vector<std::shared_ptr<Value>> stmts;
};

// 函数
class Function {
  public:
    Function(std::string type, std::string name) : type(type), name(name) {}
    Function() {}
    
    ~Function() {}

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    Type type;
    std::string name;
    // 函数参数 ...
    std::vector<std::unique_ptr<Type>> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
};

// 程序
class Program {
  public:
    Program() {}
    
    ~Program() {}

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    std::vector<std::unique_ptr<Value>> values;
    std::vector<std::unique_ptr<Function>> functions;
};