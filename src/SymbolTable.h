#pragma once

#include <unordered_map>

#include "IR.h"

enum class SymbolType {
  Var,
  Func,
  Array,
};

class Symbol {
  public:
    Symbol(std::string name, std::shared_ptr<Value> value, SymbolType symbol_type, bool is_const, std::string elem_type)
      : name(name), value(value), symbol_type(symbol_type), is_const(is_const), elem_type(elem_type) {}
    std::string name; // 标识符
    std::shared_ptr<Value> value; // 表示存储位置
    SymbolType symbol_type;
    bool is_const;
    std::string elem_type;
    
    int level; // 作用域级别
  };
  
class SymbolFunc : public Symbol {
  public:
    SymbolFunc(std::string name, std::shared_ptr<Value> value, SymbolType symbol_type, bool is_const, std::string type)
      : Symbol(name, value, symbol_type, is_const, type) {}
    std::vector<std::string> args_type;
};

class SymbolArray : public Symbol {
  public:
    SymbolArray(std::string name, std::shared_ptr<Value> value, SymbolType symbol_type, bool is_const, std::string elem_type)
      : Symbol(name, value, symbol_type, is_const, elem_type) {}
    std::vector<int> dims;
};




// 作用域
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

    void Insert(const std::string& name, const std::shared_ptr<Value>& value, SymbolType symbol_type, bool is_const, std::string elem_type) {
      if (symbol_type == SymbolType::Func) {
        table[name] = std::make_unique<SymbolFunc>(name, value, symbol_type, is_const, elem_type);
        return;
      } else if (symbol_type == SymbolType::Array) {
        table[name] = std::make_unique<SymbolArray>(name, value, symbol_type, is_const, elem_type);
      } else {
        table[name] = std::make_unique<Symbol>(name, value, symbol_type, is_const, elem_type);
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

    // 插入符号到当前作用域
    void Insert(const std::string& name, const std::shared_ptr<Value>& value, 
                                SymbolType symbol_type, bool is_const, std::string elem_type) {
      scopes[current_scope_level]->Insert(name, value, symbol_type, is_const, elem_type);
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
        if (symbol->symbol_type == SymbolType::Func) {
          std::cout << "func : "<< name << std::endl; 
          // << " " << symbol->value->name << " " << symbol->is_function << " " << symbol->is_const << " " << symbol->type << " " << func_symbol->args_type.size() << std::endl;
        } else if (symbol->symbol_type == SymbolType::Var) {
          std::cout << "var : "<< name << std::endl;
          // << " " << symbol->value->name << " " << symbol->is_function << " " << symbol->is_const << " " << symbol->type << std::endl;
        } else {
          std::cout << "array : "<< name << std::endl;
        }
      }
    }

    // 作用域栈
    std::vector<std::unique_ptr<Scope>> scopes;
    int current_scope_level = -1;
};
