#pragma once

#include <unordered_map>

#include "IR.h"

// 统一的符号类型
enum class SymbolKind : std::uint8_t {
  VARIABLE,  // 变量（包括普通变量、数组、指针）
  FUNCTION   // 函数
};

class Symbol {
 public:
  std::string name;              // 符号名称
  SymbolKind kind;               // 符号类型
  std::shared_ptr<Value> value;  // IR 中的值

  Symbol(std::string name, SymbolKind kind, std::shared_ptr<Value> value)
      : name(std::move(name)), kind(kind), value(std::move(value)) {}
};

class VariableSymbol : public Symbol {
 public:
  TypeInfo type;
  bool is_const;

  VariableSymbol(std::string name, std::shared_ptr<Value> value, const TypeInfo &type, bool is_const)
      : Symbol(std::move(name), SymbolKind::VARIABLE, std::move(value)), type(type), is_const(is_const) {}
};

class FunctionSymbol : public Symbol {
 public:
  TypeInfo return_type;
  std::vector<TypeInfo> param_types;

  FunctionSymbol(std::string name, std::shared_ptr<Value> value, const TypeInfo &return_type,
                 std::vector<TypeInfo> param_types)
      : Symbol(std::move(name), SymbolKind::FUNCTION, std::move(value)),
        return_type(return_type),
        param_types(std::move(param_types)) {}
};

// 作用域
class Scope {
 public:
  Scope() = default;
  ~Scope() = default;

  Symbol *FindSymbol(const std::string &name) {
    if (table.find(name) == table.end()) {
      return nullptr;
    }
    return table.find(name)->second.get();
  }

  std::shared_ptr<Value> FindValue(const std::string &name) {
    if (table.find(name) == table.end()) {
      return nullptr;
    }
    return table.find(name)->second->value;
  }

  bool IsContains(const std::string &name) const { return table.find(name) != table.end(); }

  void Insert(const std::string &name, std::shared_ptr<Value> value, const TypeInfo &type, bool is_const) {
    table[name] = std::make_unique<VariableSymbol>(name, std::move(value), type, is_const);
  }
  // 插入数组符号到当前作用域
  void Insert(const std::string &name, std::shared_ptr<Value> value, const TypeInfo &return_type,
              const std::vector<TypeInfo> &param_types) {
    table[name] = std::make_unique<FunctionSymbol>(name, std::move(value), return_type, param_types);
  }

  void Remove(const std::string &name) { table.erase(name); }

  // 符号表，键为符号名，值为符号信息
  std::unordered_map<std::string, std::unique_ptr<Symbol>> table;
};

class SymbolTable {
 public:
  SymbolTable() { Push(); }
  ~SymbolTable() = default;

  // 创建新的作用域
  void Push() {
    scopes.push_back(std::make_unique<Scope>());
    current_scope_level++;
  }

  // 弹出当前作用域
  void Pop() {
    scopes.pop_back();
    current_scope_level--;
  }

  std::shared_ptr<Value> FindValue(const std::string &name) {
    for (int i = current_scope_level; i >= 0; i--) {
      if (scopes[i]->IsContains(name)) {
        return scopes[i]->FindValue(name);
      }
    }
    return nullptr;
  }

  Symbol *FindSymbol(const std::string &name) {
    for (int i = current_scope_level; i >= 0; i--) {
      if (scopes[i]->IsContains(name)) {
        return scopes[i]->FindSymbol(name);
      }
    }
    assert(0 && "Symbol not found");
    return nullptr;
  }

  // 插入变量符号到当前作用域
  void Insert(const std::string &name, const std::shared_ptr<Value> &value, const TypeInfo &type, bool is_const) {
    scopes[current_scope_level]->Insert(name, value, type, is_const);
  }

  // 插入函数符号到当前作用域
  void Insert(const std::string &name, const std::shared_ptr<Value> &value, const TypeInfo &return_type,
              const std::vector<TypeInfo> &param_types) {
    scopes[current_scope_level]->Insert(name, value, return_type, param_types);
  }

  // 删除当前作用域中该符号
  void Remove(const std::string &name) { scopes[current_scope_level]->Remove(name); }

  // 检查符号表中是否存在该符号
  bool IsContains(const std::string &name) {
    for (int i = current_scope_level; i >= 0; i--) {
      if (scopes[i]->IsContains(name)) {
        return true;
      }
    }
    return false;
  }

  void PrintTable() {
    std::cout << "Scope " << current_scope_level << std::endl;
    for (auto &[name, symbol] : scopes[current_scope_level]->table) {
      if (symbol->kind == SymbolKind::FUNCTION) {
        std::cout << "func : " << name << std::endl;
      } else if (symbol->kind == SymbolKind::VARIABLE) {
        std::cout << "var : " << name << std::endl;
      }
    }
  }

  // 作用域栈
  std::vector<std::unique_ptr<Scope>> scopes;
  int current_scope_level = -1;
};
