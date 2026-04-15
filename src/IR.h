// 参照文档https://pku-minic.github.io/online-doc/#/misc-app-ref/libkoopa
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <fstream>

#include "visitor.h"

enum Mode {
  IR,
  BINARY,
};

class Type {
  public:
    Type(std::string name) : name(name) {}
    // 表示是什么类型的Value
    std::string name;
};

// class ValueKind {
//   public:
//     ValueKind(std::string kind) : kind(kind) {}
//     std::string kind;
// };

// 全局变量和指令都是Value
class Value {
  public:
    Value(std::string name, std::string type) : name(name), type(type) {}
    
    virtual ~Value() = default;
    
    virtual void Accept(IRVisitor* visitor) = 0;

    std::string name;
    Type type;

};

class Value_RETURN : public Value {
  public:
    Value_RETURN(int val) : Value("return", "i32"), val(val) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }
    
    int val;
};

// 基本块
class BasicBlock {
  public:
    BasicBlock(std::string name) : name(name) {}

    ~BasicBlock() {
      for (auto stmt : stmts) {
        delete stmt;
      }
    }

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    std::string name;
    std::vector<Value*> stmts;
};

// 函数
class Function {
  public:
    Function(std::string type, std::string name, std::vector<Type*> params) : type(type), name(name), params(params) {}
    
    ~Function() {
      for (auto block : blocks) {
        delete block;
      }
    }

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    Type type;
    std::string name;
    // 函数参数 ...
    std::vector<Type*> params;
    std::vector<BasicBlock*> blocks;
};

// 程序
class Program {
  public:
    Program() {}
    
    ~Program() {
      for (auto func : functions) {
        delete func;
      }
    }

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    void NewValue() {
      // TODO
    }

    std::vector<Value*> values;
    std::vector<Function*> functions;
};