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

// 内存分配指令
class Value_ALLOC : public Value {
public:
    Value_ALLOC(std::string name, std::string elem_type)
        : Value(name, Value_Type::KOOPA_RVT_ALLOC), elem_type(elem_type) {}
    
    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }
    
    std::string elem_type;  // "i32", "i32*", 等
};

// 内存加载指令
class Value_LOAD : public Value {
public:
    Value_LOAD(std::string name, std::shared_ptr<Value> src)
        : Value(name, Value_Type::KOOPA_RVT_LOAD), src(src) {}
    
    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }
    
  std::shared_ptr<Value> src;  // 指向内存的指针
};

// 内存存储指令
class Value_STORE : public Value {
public:
    Value_STORE(std::shared_ptr<Value> value, std::shared_ptr<Value> dst)
        : Value("null", Value_Type::KOOPA_RVT_STORE), value(value), dst(dst) {}
    
    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }
    
    std::shared_ptr<Value> value;  // 要存储的值
    std::shared_ptr<Value> dst;    // 目标内存地址
};

class Value_INTEGER : public Value {
  public:
    Value_INTEGER(int val) : Value(std::to_string(val), Value_Type::KOOPA_RVT_INTEGER), val(val) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    int val;
};

// class Value_VOID : public Value {
//   public:
//     Value_VOID() : Value("void", Value_Type::KOOPA_RVT_VOID) {}

//     void Accept(IRVisitor* visitor) override {
//       visitor->Visit(this);
//     }
// };

// 条件跳转指令
class Value_BRANCH : public Value {
  public:
    Value_BRANCH(std::shared_ptr<Value> cond, std::string then_name, 
        std::string else_name) : Value("branch", Value_Type::KOOPA_RVT_BRANCH), 
          cond(cond), then_name(then_name), else_name(else_name) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    std::shared_ptr<Value> cond;
    std::string then_name;
    std::string else_name;
};

// 无条件跳转指令
class Value_JUMP : public Value {
  public:
    Value_JUMP(std::string dst_name) : Value("jump", Value_Type::KOOPA_RVT_JUMP), dst_name(dst_name) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    std::string dst_name;
};

class Value_FUNC_ARG_REF : public Value {
 public:
  Value_FUNC_ARG_REF(std::string name, std::string elem_type) 
    : Value(name, Value_Type::KOOPA_RVT_FUNC_ARG_REF), elem_type(elem_type){}

  void Accept(IRVisitor* visitor) override {
    visitor->Visit(this);
  }
  std::string elem_type;  // "i32", 等
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

class Value_Call : public Value {
  public:
    Value_Call(std::string name, std::string ident, std::vector<std::shared_ptr<Value>> args)
     : Value(name, Value_Type::KOOPA_RVT_CALL), ident(ident), args(args) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }
    // 函数名
    std::string ident;
    // 函数参数
    std::vector<std::shared_ptr<Value>> args;
};

// 基本块
class BasicBlock {
  public:
    BasicBlock(std::string name) : name(name) {}

    ~BasicBlock() {}

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }
    void Terminate() {
      terminated = true;
    }
    bool IsTerminated() {
      return terminated;
    }

    std::string name;
    std::vector<std::shared_ptr<Value>> stmts;
    // 是否已经终止（有return/break/continue/jump）
    bool terminated = false;
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

    std::vector<std::shared_ptr<Value>> params;
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