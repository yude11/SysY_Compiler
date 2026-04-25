#pragma once

#include <unordered_map>

#include "IR.h"

class Symbol {
  public:
    Symbol(std::string name, std::shared_ptr<Value> value, bool is_function, bool is_const, std::string type)
      : name(name), value(value), is_function(is_function), is_const(is_const), type(type) {}
    std::string name; // 标识符
    std::shared_ptr<Value> value; // 表示存储位置
    bool is_function;
    bool is_const;
    std::string type;
    
    int level; // 作用域级别
  };
  
  class SymbolFunc : public Symbol {
    public:
      SymbolFunc(std::string name, std::shared_ptr<Value> value, bool is_function, bool is_const, std::string type)
        : Symbol(name, value, is_function, is_const, type) {}
      std::vector<std::string> args_type;
};

// 局部作用域
class Scope {
  public:
    Scope() = default;
    ~Scope() = default;

    Symbol* FindSymbol(const std::string& name) {
      if (table.find(name) == table.end()) {
        return nullptr;
      }
      return table.find(name)->second.get();
    }

    std::shared_ptr<Value> FindValue(const std::string& name) {
      if (table.find(name) == table.end()) {
        return nullptr;
      }
      return table.find(name)->second->value;
    }

    bool IsContains(const std::string& name) {
      return table.find(name) != table.end();
    }

    void Insert(const std::string& name, const std::shared_ptr<Value>& value, bool is_function, bool is_const, std::string type) {
      if (is_function) {
        table[name] = std::make_unique<SymbolFunc>(name, value, is_function, is_const, type);
        return;
      } else {
        table[name] = std::make_unique<Symbol>(name, value, is_function, is_const, type);
      }
    }

    void Remove(const std::string& name) {
      table.erase(name);
    }

    // 符号表，键为符号名，值为符号信息
    std::unordered_map<std::string, std::unique_ptr<Symbol>> table;
};

class SymbolTable {
  public:
    SymbolTable() {
      // global_scope = std::make_unique<Scope>();
      // scopes.push_back(std::make_unique<Scope>());
      Push();
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
      if (scopes.empty()) {
        value_name_map.clear();
        name_count.clear();
      }
    }

    std::shared_ptr<Value> FindValue(const std::string& name) {
      for (int i = current_scope_level; i >= 0; i--) {
        if (scopes[i]->IsContains(name)) {
          return scopes[i]->FindValue(name);
        }
      }
      return nullptr;
    }

    Symbol* FindSymbol(const std::string& name) {
      for (int i = current_scope_level; i >= 0; i--) {
        if (scopes[i]->IsContains(name)) {
          return scopes[i]->FindSymbol(name);
        }
      }
      assert(0 && "Symbol not found");
      return nullptr;
    }

    // void InsertGlobal(const std::string& name, const std::shared_ptr<Value>& value, 
    //                             bool is_function, bool is_const, std::string type) {
    //   global_scope->Insert(name, value, is_function, is_const, type);
    // }

    // 插入符号到当前作用域
    void Insert(const std::string& name, const std::shared_ptr<Value>& value, 
                                bool is_function, bool is_const, std::string type) {
      scopes[current_scope_level]->Insert(name, value, is_function, is_const, type);
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
        if (symbol->is_function) {
          auto func_symbol = std::dynamic_pointer_cast<SymbolFunc>(symbol->value);
          std::cout << name << " " << symbol->value->name << " " << symbol->is_function << " " << symbol->is_const << " " << symbol->type << " " << func_symbol->args_type.size() << std::endl;
        } else {
          std::cout << name << " " << symbol->value->name << " " << symbol->is_function << " " << symbol->is_const << " " << symbol->type << std::endl;
        }
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

    // 全局作用域
    // std::unique_ptr<Scope> global_scope;
    // 局部作用域栈
    std::vector<std::unique_ptr<Scope>> scopes;
    int current_scope_level = -1;
    std::unordered_map<Value*, std::string> value_name_map;
    std::unordered_map<std::string, int> name_count;
};
