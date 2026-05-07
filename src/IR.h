// 数据结构设计参照文档https://pku-minic.github.io/online-doc/#/misc-app-ref/libkoopa
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <fstream>

#include "visitor.h"
#include "type.h"
#include "log.h"

// 基础类型枚举
enum class BaseType {
    INT,
    VOID
};


class TypeInfo {
  public:
    // 类型修饰符
    enum class TypeKind {
        SCALAR,     // 普通变量
        POINTER,    // 指针
        ARRAY,       // 数组
    };

    TypeInfo(BaseType base, std::vector<std::pair<TypeKind, int>> modifiers)
        : base(base), modifiers(modifiers) {}

    // 构建普通变量类型
    TypeInfo(BaseType base) : base(base) {}
    // 构建指针类型
    TypeInfo(BaseType base, TypeKind kind) : base(base), modifiers({{kind, 0}}) {}
    // 构建数组类型
    TypeInfo(BaseType base, TypeKind kind, int size) : base(base), modifiers({{kind, size}}) {}

    TypeInfo() : base(BaseType::VOID) {}
    ~TypeInfo() = default;
    
    bool isPointer() const {
        return !modifiers.empty() && modifiers[0].first == TypeKind::POINTER;
    }
    
    bool isArray() const {
        return !modifiers.empty() && modifiers[0].first == TypeKind::ARRAY;
    }

    bool isScalar() const {
        return modifiers.empty();
    }
    
    // 获取数组维度
    std::vector<int> getArrayDims() const {
        std::vector<int> dims;
        for (const auto& mod : modifiers) {
            if (mod.first == TypeKind::ARRAY) dims.push_back(mod.second);
            else if (mod.first == TypeKind::POINTER) dims.push_back(-1);
            else break;
        }
        return dims;
    }
    
    // 转换为 Koopa IR 类型字符串
    std::string toKoopaString() const {
        std::string result = (base == BaseType::INT) ? "i32" : "void";
        for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
            if (it->first == TypeKind::POINTER) {
                result = "*" + result;
            } else if (it->first == TypeKind::ARRAY) {
                result = "[" + result + ", " + std::to_string(it->second) + "]";
            }
        }
        return result;
    }
    
    // 重载==运算符
    bool operator==(const TypeInfo& other) const {
        return (base == other.base && modifiers == other.modifiers);
    }
    
    BaseType base;
    std::vector<std::pair<TypeKind, int>> modifiers;  // (类型, 大小)
};

// 全局变量和指令都是Value
class Value {
  public:
    Value(std::string name, Value_Type type) : name(name), type(type) {}

    Value(std::string name, Value_Type type, TypeInfo type_info) : name(name), type(type), type_info(type_info) {}
    
    virtual ~Value() = default;
    
    virtual void Accept(IRVisitor* visitor) = 0;

    // 表示Value的名字，如%0
    std::string name;
    // 表示Value的类型
    Value_Type type;
    // Value的类型信息
    TypeInfo type_info;
    // 被那些结构使用used_by
    //TODO
};

// 内存分配指令
class Value_ALLOC : public Value {
public:
    Value_ALLOC(std::string name, TypeInfo type_info)
        : Value(name, Value_Type::KOOPA_RVT_ALLOC, type_info) {
      // alloc指令分配内存返回的是一个指针
      LOG("ALLOC creating " + name + " input_type=" + type_info.toKoopaString() + " mods=" + std::to_string(type_info.modifiers.size()));
      this->type_info.modifiers.insert(this->type_info.modifiers.begin(), {TypeInfo::TypeKind::POINTER, 0});
      LOG("ALLOC result " + name + " type=" + this->type_info.toKoopaString() + " mods=" + std::to_string(this->type_info.modifiers.size()));
    }
    
    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class Value_GLOBOL_ALLOC : public Value {
public:
    Value_GLOBOL_ALLOC(std::string name, std::vector<std::shared_ptr<Value>> init, TypeInfo type_info)
        : Value(name, Value_Type::KOOPA_RVT_GLOBAL_ALLOC, type_info), init(init) {
      // alloc指令分配内存返回的是一个指针
      this->type_info.modifiers.insert(this->type_info.modifiers.begin(), {TypeInfo::TypeKind::POINTER, 0});
      
    }

    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }
    // 初始化值
    std::vector<std::shared_ptr<Value>> init;
};

class Value_GET_ELEM_PTR : public Value {
  public:
    Value_GET_ELEM_PTR(std::string name, std::shared_ptr<Value> base, std::shared_ptr<Value> index, int elem_size = 4)
        : Value(name, Value_Type::KOOPA_RVT_GET_ELEM_PTR, base->type_info), base(base), index(index), elem_size(elem_size) {
      assert(base->type_info.isPointer() && base->type_info.modifiers[1].first == TypeInfo::TypeKind::ARRAY
             && "base must be a array pointer");
      this->type_info.modifiers.erase(this->type_info.modifiers.begin() + 1);
    }

    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }

    std::shared_ptr<Value> base;  // 数组的基地址
    std::shared_ptr<Value> index;  // 索引值
    int elem_size;  // 元素大小（字节）
};

class Value_GET_PTR : public Value {
  public:
    Value_GET_PTR(std::string name, std::shared_ptr<Value> base, std::shared_ptr<Value> index, int elem_size = 4)
        : Value(name, Value_Type::KOOPA_RVT_GET_PTR, base->type_info), base(base), index(index), elem_size(elem_size) {
        assert(base->type_info.isPointer() && "base must be a pointer");
        // this->type_info.modifiers.erase(this->type_info.modifiers.begin() + 1);
    }

    void Accept(IRVisitor* visitor) override {
        visitor->Visit(this);
    }

    std::shared_ptr<Value> base;  // 指针的基地址
    std::shared_ptr<Value> index;  // 索引值
    int elem_size;  // 基元素的大小
};

// 内存加载指令
class Value_LOAD : public Value {
  public:
    Value_LOAD(std::string name, std::shared_ptr<Value> src)
        : Value(name, Value_Type::KOOPA_RVT_LOAD, src->type_info), src(src) {
      assert(src->type_info.isPointer() && "src must be a pointer");  
      this->type_info.modifiers.erase(this->type_info.modifiers.begin());
    }
    
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
    Value_INTEGER(int val) : Value(std::to_string(val), Value_Type::KOOPA_RVT_INTEGER, TypeInfo(BaseType::INT)), val(val) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    int val;
};

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
    Value_FUNC_ARG_REF(std::string name, TypeInfo type_info) 
    : Value(name, Value_Type::KOOPA_RVT_FUNC_ARG_REF, type_info) {}

  void Accept(IRVisitor* visitor) override {
    visitor->Visit(this);
  }
};

// 二元操作语句
class Value_BINARY : public Value {
  public:
    Value_BINARY(std::string name, std::shared_ptr<Value> lhs, std::shared_ptr<Value> rhs, Binary_Op_Type op)
     : Value(name, Value_Type::KOOPA_RVT_BINARY, lhs->type_info), type(op), lhs(lhs), rhs(rhs) {
      LOG("BINARY lhs type=" + lhs->type_info.toKoopaString() + " mods=" + std::to_string(lhs->type_info.modifiers.size()) + " Value_Type=" + std::to_string((int)lhs->type));
      LOG("BINARY rhs type=" + rhs->type_info.toKoopaString() + " mods=" + std::to_string(rhs->type_info.modifiers.size()) + " Value_Type=" + std::to_string((int)rhs->type));



      assert(lhs->type_info == rhs->type_info && "lhs and rhs must have the same type");
    }

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

    Value_RETURN() : Value("null", Value_Type::KOOPA_RVT_RETURN) {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }

    std::shared_ptr<Value> val;
};

class Value_Call : public Value {
  public:
    // 函数返回值需要查符号表获取，所以需要传递类型信息
    Value_Call(std::string name, std::string ident, std::vector<std::shared_ptr<Value>> args, TypeInfo type_info)
     : Value(name, Value_Type::KOOPA_RVT_CALL, type_info),  ident(ident), args(args) {}

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
    Function(BaseType base, std::vector<std::pair<TypeInfo::TypeKind, int>> modifiers, 
            std::string name) : return_type(base, modifiers), name(name) {}
    
    virtual ~Function() {}

    virtual void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    TypeInfo return_type;
    std::string name;
    // 函数参数 ...

    std::vector<TypeInfo> params_type;
    std::vector<std::shared_ptr<Value>> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
};

class Function_Decl : public Function{
  public:
    Function_Decl(BaseType base, std::vector<std::pair<TypeInfo::TypeKind, int>> modifiers, 
            std::string name) : Function(base, modifiers, name) {}
    
    ~Function_Decl() override {}

    void Accept(IRVisitor* visitor) override {
      visitor->Visit(this);
    }
};

// 程序
class Program {
  public:
    Program() {}
    
    ~Program() {}

    void Accept(IRVisitor* visitor) {
      visitor->Visit(this);
    }

    std::vector<std::shared_ptr<Value>> values;
    std::vector<std::unique_ptr<Function>> functions;
};