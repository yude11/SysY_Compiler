#pragma once

#include <unordered_map>
#include <fstream>
#include <string>
#include <stack>

#include "visitor.h"
#include "type.h"
#include "IR.h"
#include "log.h"
#include "SymbolTable.h"



// 寄存器分配器
class RegAllocator {
public:
  // 值到寄存器的映射
  std::unordered_map<Value*, std::string> value_to_reg;
  // 寄存器到值的映射 
  std::unordered_map<std::string, Value*> reg_to_value;
  // 可用寄存器栈
  std::stack<std::string> free_regs;

  std::unordered_map<Value*, int> value_to_loc;

  int stack_offset = 0;  // 溢出时的栈偏移

  RegAllocator() {
    // 初始化可用寄存器
    for (int i = 0; i < 6; i++) {
      free_regs.push("a" + std::to_string(i));
    }
    for (int i = 6; i >= 0; i--) {
      free_regs.push("t" + std::to_string(i));
    }
  }
  
  // 为值分配寄存器
  std::string Alloc(Value* value) {
    auto r = Lookup(value);
    if (r != "") {
      LOG("Duplicate Alloc");
      return r;
    }
    std::string reg = free_regs.top();
    free_regs.pop();
    value_to_reg[value] = reg;
    reg_to_value[reg] = value;
    return reg;
  }

  std::string AllocZero(Value* value) {
    value_to_reg[value] = "x0";
    reg_to_value["x0"] = value;
    return "x0";
  }

  void Copy(Value* src, Value* dst) {
    value_to_reg[dst] = value_to_reg[src];
    reg_to_value[value_to_reg[dst]] = dst;
  }
  
  // 查找值对应的寄存器
  std::string Lookup(Value* value) {
    auto it = value_to_reg.find(value);
    if (it != value_to_reg.end()) {
      return it->second;
    }
    return "";  // 未找到
  }

  int GetLoc(Value* value) {
    auto it = value_to_loc.find(value);
    if (it != value_to_loc.end()) {
      return it->second;
    }
    return -1;  // 未找到
  }

  void Scan(Function* func) {
    for (int i = 0; i < func->blocks.size(); i++) {
      auto block = func->blocks[i].get();
      for (auto stmt : block->stmts) {
        switch (stmt->type) {
        case Value_Type::KOOPA_RVT_ALLOC:
        case Value_Type::KOOPA_RVT_LOAD:
        case Value_Type::KOOPA_RVT_BINARY:
          value_to_loc[stmt.get()] = stack_offset;
          stack_offset+=4;
          break;
        case Value_Type::KOOPA_RVT_INTEGER:
          assert(0 && "error: error instruction type: INTEGER");
          break;
        default:
          break;
        }
      }
    }
  }

  void Clear() {
    value_to_reg.clear();
    reg_to_value.clear();
    while (!free_regs.empty()) {
      free_regs.pop();
    }

    for (int i = 6; i >= 0; i--) {
      free_regs.push("t" + std::to_string(i));
    }
  }
  
  // // 溢出到栈
  // std::string Spill(std::string value_name) {
  //   // 选择一个寄存器溢出
  //   std::string reg = "t0";  // 简化：总是选 t0
  //   std::string old_value = reg_to_value[reg];
    
  //   // 保存到栈
  //   // sw t0, offset(sp)
    
  //   // 更新映射
  //   value_to_reg.erase(old_value);
  //   value_to_reg[value_name] = reg;
  //   reg_to_value[reg] = value_name;
    
  //   return reg;
  // }
  
  // 释放寄存器
  void Free(Value* value) {
    std::string reg = value_to_reg[value];
    value_to_reg.erase(value);
    reg_to_value.erase(reg);
    free_regs.push(reg);
  }
};



class AssemblyGenerator : public IRVisitor {
  public:
    AssemblyGenerator(std::string output, std::unique_ptr<SymbolTable> symbol_table);
    ~AssemblyGenerator();

    void Visit(Value_RETURN* return_val) override;
    void Visit(Value_INTEGER* integer) override;
    void Visit(Value_BINARY* binary) override;
    void Visit(Value_ALLOC* alloc) override;
    void Visit(Value_LOAD* load) override;
    void Visit(Value_Call* call) override;
    void Visit(Value_STORE* store) override;
    void Visit(Value_JUMP* jump) override;
    void Visit(Value_BRANCH* branch) override;
    void Visit(Value_REF* ref) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
    std::unique_ptr<RegAllocator> reg_allocator;
    std::unique_ptr<SymbolTable> symbol_table;

};

