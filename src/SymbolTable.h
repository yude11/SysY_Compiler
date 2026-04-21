#pragma once

#include <unordered_map>

#include "IR.h"

class Symbol {
  public:
    std::string name; // 标识符
    std::shared_ptr<Value> value; // 表示存储位置
    bool is_const;
    std::string type;

    int level; // 作用域级别
};

class Scope {
  public:
    Scope() = default;
    ~Scope() = default;

    Symbol FindSymbol(const std::string& name) {
      if (table.find(name) == table.end()) {
        return Symbol();
      }
      return table.find(name)->second;
    }

    std::shared_ptr<Value> FindValue(const std::string& name) {
      if (table.find(name) == table.end()) {
        return nullptr;
      }
      return table.find(name)->second.value;
    }

    bool IsContains(const std::string& name) {
      return table.find(name) != table.end();
    }

    void Insert(const std::string& name, const std::shared_ptr<Value>& value, bool is_const, std::string type) {
      table[name] = {name, value, is_const, type};
    }

    void Remove(const std::string& name) {
      table.erase(name);
    }

    std::unordered_map<std::string, Symbol> table;
};

class SymbolTable {
  public:
    SymbolTable() {
      Push(); // 首先创建全局作用域
    }
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

    std::shared_ptr<Value> FindValue(const std::string& name) {
      for (int i = current_scope_level; i >= 0; i--) {
        if (scopes[i]->IsContains(name)) {
          return scopes[i]->FindValue(name);
        }
      }
      return nullptr;
    }

    Symbol FindSymbol(const std::string& name) {
      for (int i = current_scope_level; i >= 0; i--) {
        if (scopes[i]->IsContains(name)) {
          return scopes[i]->FindSymbol(name);
        }
      }
      return Symbol();
    }

    // 插入符号到当前作用域
    void Insert(const std::string& name, const std::shared_ptr<Value>& value, bool is_const, std::string type) {
      scopes[current_scope_level]->Insert(name, value, is_const, type);
      AllocateName(value.get());
    }

    // 删除当前作用域中该符号
    void Remove(const std::string& name) {
      scopes[current_scope_level]->Remove(name);
    }

    // 检查符号表中是否存在该符号
    bool IsContains(const std::string& name) {
      for (int i = current_scope_level; i >= 0; i--) {
        if (scopes[i]->IsContains(name)) {
          return true;
        }
      }
      return false;
    }

    void PrintTable() {
      std::cout << "Scope " << current_scope_level << std::endl;
      for (auto& [name, symbol] : scopes[current_scope_level]->table) {
        std::cout << name << " " << symbol.value->name << " " << symbol.is_const << " " << symbol.type << std::endl;
      }
    }

    // 为Value分配名称
    std::string AllocateName(Value* value) {
      if (value_name_map.find(value) != value_name_map.end()) {
        return value_name_map[value];
      }
      std::string name = value->name + std::to_string(name_count[value->name]);
      name_count[value->name]++;
      value_name_map[value] = name;
      return name;
    }

    std::string GetName(Value* value) {
      return value_name_map[value];
    }

    std::vector<std::unique_ptr<Scope>> scopes;
    int current_scope_level = -1;
    std::unordered_map<Value*, std::string> value_name_map;
    std::unordered_map<std::string, int> name_count;
};
