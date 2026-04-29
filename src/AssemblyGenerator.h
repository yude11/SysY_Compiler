#pragma once

#include <unordered_map>
#include <fstream>
#include <string>
#include <stack>
#include <variant>

#include "visitor.h"
#include "type.h"
#include "IR.h"
#include "log.h"
#include "SymbolTable.h"



// 寄存器分配器
class RegAllocator {
public:
  // typedef std::variant<std::string, int> Location; 
  // 值到寄存器的映射
  std::unordered_map<Value*, std::string> value_to_reg;
  // std::unordered_map<Value*, int> where_is_value; // 0: 寄存器, 1: 栈
  // 值到栈偏移的映射
  std::unordered_map<Value*, int> value_to_loc;
  // 可用临时寄存器栈
  std::stack<std::string> free_regs;
  // 可用参数寄存器栈
  std::stack<std::string> param_regs;
  // 标记ra寄存器是否被使用
  bool ra_used = false;
  
  int stack_offset = 0;  // 溢出时的栈偏移
  int param_offset = 0;  // 参数溢出的位置

  RegAllocator() {
    // 初始化可用寄存器
    for (int i = 6; i >= 0; i--) {
      free_regs.push("t" + std::to_string(i));
    }
    // 参数寄存器
    for (int i = 7; i >= 0; i--) {
      param_regs.push("a" + std::to_string(i));
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
    return reg;
  }

  std::string AllocParams(const std::vector<std::shared_ptr<Value>>& args) {
    for (size_t i = 0; i < args.size() && i < 8; i++) {
      // auto arg = args[i].get();
      std::string reg = param_regs.top();
      param_regs.pop();
      // value_to_loc[arg] = reg;
    }
    for (size_t i = 8; i < args.size(); i++) {
      auto arg = args[i].get();
      value_to_loc[arg] = param_offset + 4 * (i - 8);
    }
    return "";
  }

  std::string AllocZero(Value* value) {
    value_to_reg[value] = "x0";
    return "x0";
  }

  void Copy(Value* src, Value* dst) {
    value_to_reg[dst] = value_to_reg[src];
  }
  
  // 查找值对应的寄存器
  std::string Lookup(Value* value) {
    if (value_to_reg.find(value) != value_to_reg.end()) {
      return value_to_reg[value];
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

  bool Scan(Function* func) {
    // 1. 如果函数传参的数量大于8，额外的参数也需要分配栈空间
    int max_params = -1;
    bool has_call = false;
    for (auto& block : func->blocks) {
      for (auto& stmt : block->stmts) {
        if (stmt->type == Value_Type::KOOPA_RVT_CALL) {
          has_call = true;
          // 统计函数调用参数数量
          auto call = static_cast<Value_Call*>(stmt.get());
          int param_count = call->args.size();
          if (param_count > max_params) {
            max_params = param_count;
          }
        }
      }
    }
    param_offset = stack_offset;
    if (max_params > 8) {
      stack_offset += 4 * (max_params - 8);
    }
    // 2. 扫描函数体，统计需要分配寄存器的值的数量，计算总的栈空间需求
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
          case Value_Type::KOOPA_RVT_CALL: {
            auto call = static_cast<Value_Call*>(stmt.get());
            if (call->name != "null") {
              value_to_loc[stmt.get()] = stack_offset;
              stack_offset+=4;
            }
            break;
          }
          case Value_Type::KOOPA_RVT_INTEGER:
          assert(0 && "error: error instruction type: INTEGER");
          break;
          default:
          break;
        }
        
      }
    }
    // 3. 为ra寄存器分配4字节的栈空间
    // 判断函数中是否有调用指令，如果有则需要为ra分配栈空间
    if (has_call) {
      stack_offset += 4;
    }
    ra_used = has_call;
    // 4. 保持栈对齐
    if (stack_offset % 16 != 0) {
      // 保持栈对齐
      stack_offset += (16 - stack_offset % 16);
    }
    return has_call;
  }

  void Clear() {
    // 清除所有寄存器分配
    value_to_reg.clear();
    while (!free_regs.empty()) {
      free_regs.pop();
    }

    for (int i = 6; i >= 0; i--) {
      free_regs.push("t" + std::to_string(i));
    }
  }

  void Reset() {
    Clear();
    stack_offset = 0;
    param_offset = 0;
    while (!param_regs.empty()) {
      param_regs.pop();
    }
    for (int i = 7; i >= 0; i--) {
      param_regs.push("a" + std::to_string(i));
    }
  }
  
  // // 溢出到栈
  // std::string SpillParam(std::string value_name) {
  //   return reg;
  // }
  
  // 释放寄存器
  void Free(Value* value) {
  }
};



class AssemblyGenerator : public IRVisitor {
  public:
    AssemblyGenerator(std::string output);
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
    void Visit(Value_FUNC_ARG_REF* ref) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
    std::unique_ptr<RegAllocator> reg_allocator;
    // std::unique_ptr<SymbolTable> symbol_table;

};

