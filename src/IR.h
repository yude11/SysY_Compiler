// 参照文档https://pku-minic.github.io/online-doc/#/misc-app-ref/libkoopa
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>

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
    
    virtual void Dump() const = 0;

    std::string name;
    Type type;

};

class Value_RETURN : public Value {
  public:
    Value_RETURN(int val) : Value("return", "i32"), val(val) {}
    
    void Dump() const override {
      std::cout << "  ret " << val << std::endl;
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

    Value* NewStmt(int val) {
      auto stmt = new Value_RETURN(val);
      stmts.push_back(stmt);
      return stmt;
    }

    void Dump() const {
      std::cout << "%entry:" << std::endl;
      for (auto stmt : stmts) {
        stmt->Dump();
      }
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

    BasicBlock* NewBlock(std::string name) {
      std::cout << "AddBlock: "  << std::endl;
      auto block = new BasicBlock(name);
      blocks.push_back(block);
      return block;
    }

    void Dump() const {
      std::cout << "fun @" << name << "(): "  << type.name << " {" << std::endl;
      for (auto block : blocks) {
        block->Dump();
      }
      std::cout << "}" << std::endl;
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

    Function* NewFunc(std::string type, std::string name, std::vector<Type*> params) {
      auto func = new Function(type, name, params);
      functions.emplace_back(func);
      return func;
    }

    void Dump() const {
      for (auto func : functions) {
        func->Dump();
      }
    }

    void NewValue() {
      // TODO
    }

    std::vector<Value*> values;
    std::vector<Function*> functions;
};