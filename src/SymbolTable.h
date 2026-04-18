#pragma once

#include <unordered_map>

#include "IR.h"

class Symbol {
  public:
    std::string name; // 标识符
    std::shared_ptr<Value> value; // 表示存储位置
    bool is_const;
    std::string type;
};

class SymbolTable {
  public:
    SymbolTable() = default;
    ~SymbolTable() = default;

    // 查找符号表中是否存在该符号
    std::shared_ptr<Value> Find(const std::string& name) {
      if (table.find(name) == table.end()) {
        return nullptr;
      }
      return table.find(name)->second.value;
    }

    // 插入符号表
    void Insert(const std::string& name, const std::shared_ptr<Value>& value, bool is_const, std::string type) {
      table[name] = {name, value, is_const, type};
    }

    // 删除符号表中该符号
    void Remove(const std::string& name) {
      table.erase(name);
    }

    // 检查符号表中是否存在该符号
    bool IsContains(const std::string& name) {
      return table.find(name) != table.end();
    }

    void PrintTable() {
      for (auto& [name, symbol] : table) {
        std::cout << name << " " << symbol.value->name << " " << symbol.is_const << " " << symbol.type << std::endl;
      }
    }

    std::unordered_map<std::string, Symbol> table;
};
