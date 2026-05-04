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
  // 寄存器到值的映射
  // std::unordered_map<std::string, Value*> reg_to_value;
  // 值到寄存器的映射
  std::unordered_map<Value*, std::string> value_to_reg_t;
  std::unordered_map<Value*, std::string> value_to_reg_a; // 存储函数参数和返回值
  // 值到栈偏移的映射
  std::unordered_map<Value*, int> value_to_stack;
  // 全局变量到标签的映射
  std::unordered_map<Value*, std::string> global_value_to_label;
  std::unordered_map<Value*, int> where_is_value; // 0: 寄存器t, 1: 寄存器a, 2: 栈, 3: 全局变量

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
    value_to_reg_t[value] = reg;
    return reg;
  }

  // 为函数参数分配寄存器
  // 如果参数寄存器栈为空，溢出到栈
  // 如果参数寄存器栈不为空，分配参数寄存器
  std::string AllocParams(Value* arg) {
    auto r = Lookup(arg);
    if (r != "") {
      LOG("Duplicate Alloc");
      return r;
    }
    if (param_regs.empty()) {
      LOG("No more parameter registers available, spilling to stack");
      int offset = param_offset;
      param_offset += 4;
      value_to_stack[arg] = offset;
      where_is_value[arg] = 2;  // 标记为在栈中
      return std::to_string(offset) + "(sp)";
    } else {
      std::string reg = param_regs.top();
      param_regs.pop();
      value_to_reg_a[arg] = reg;
      where_is_value[arg] = 1;  // 标记为在寄存器 a 中
      return reg;
    }
  }

  void AllocGlobal(Value* glob_alloc) {
    global_value_to_label[glob_alloc] = glob_alloc->name.substr(1);
    where_is_value[glob_alloc] = 3;
  }

  std::string AllocZero(Value* value) {
    value_to_reg_t[value] = "x0";
    return "x0";
  }

  void Copy(Value* src, Value* dst) {
    value_to_reg_t[dst] = value_to_reg_t[src];
  }
  
  // 查找值对应的寄存器或栈位置
  std::string Lookup(Value* value) {
    if (value_to_reg_t.find(value) != value_to_reg_t.end()) {
      return value_to_reg_t[value];
    } else if (value_to_reg_a.find(value) != value_to_reg_a.end()) {
      return value_to_reg_a[value];
    }
    return "";  // 未找到
  }

  int GetLoc(Value* value) {
    auto it = value_to_stack.find(value);
    if (it != value_to_stack.end()) {
      return it->second;
    }
    assert(0 && "error: value not found in stack");
    return -1;  // 未找到
  }

  int GetWhereIsValue(Value* value) {
    if (where_is_value.find(value) != where_is_value.end()) {
      return where_is_value[value];
    }
    assert(0 && "error: value not found in where_is_value");
    return -1;  // 未找到
  }

  std::string GetLabel(Value* value) {
    return global_value_to_label[value];
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
      for (auto& stmt : block->stmts) {
        switch (stmt->type) {
          case Value_Type::KOOPA_RVT_ALLOC: {
            LOG("Alloc");
            value_to_stack[stmt.get()] = stack_offset;
            where_is_value[stmt.get()] = 2;  // 标记为在栈中
            auto alloc = static_cast<Value_ALLOC*>(stmt.get());
            if (alloc->dims.empty()) {
              stack_offset += 4;  // 单个变量
            } else {
              int size = 1;
              for (int dim : alloc->dims) {
                size *= dim;
              }
              stack_offset += size * 4;  // 数组
            }
            break;
          }
          case Value_Type::KOOPA_RVT_LOAD:
            LOG("Load");
          case Value_Type::KOOPA_RVT_BINARY:
          case Value_Type::KOOPA_RVT_GET_ELEM_PTR:
          value_to_stack[stmt.get()] = stack_offset;
          where_is_value[stmt.get()] = 2;  // 标记为在栈中
          stack_offset+=4;
          break;
          case Value_Type::KOOPA_RVT_CALL: {
            auto call = static_cast<Value_Call*>(stmt.get());
            if (call->name != "null") {
              value_to_stack[stmt.get()] = stack_offset;
              where_is_value[stmt.get()] = 2;  // 标记为在栈中
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

  void ClearTempRegs() {
    value_to_reg_t.clear();
    for (auto it = where_is_value.begin(); it != where_is_value.end(); ) {
      if (it->second == 0) {
        it = where_is_value.erase(it);
      } else {
        ++it;
      }
    }
    while (!free_regs.empty()) {
      free_regs.pop();
    }
    for (int i = 6; i >= 0; i--) {
      free_regs.push("t" + std::to_string(i));
    }
  }

  void ResetForNewFunction() {
    ClearTempRegs();
    value_to_reg_a.clear();
    value_to_stack.clear();
    for (auto it = where_is_value.begin(); it != where_is_value.end(); ) {
      if (it->second != 3) {
        it = where_is_value.erase(it);
      } else {
        ++it;
      }
    }
    stack_offset = 0;
    param_offset = 0;
    ra_used = false;
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
    void Visit(Value_GLOBOL_ALLOC* glob_alloc) override;
    void Visit(Value_GET_ELEM_PTR* get_elem_ptr) override;
    void Visit(Value_LOAD* load) override;
    void Visit(Value_Call* call) override;
    void Visit(Value_STORE* store) override;
    void Visit(Value_JUMP* jump) override;
    void Visit(Value_BRANCH* branch) override;
    void Visit(Value_FUNC_ARG_REF* ref) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function_Decl* func_decl) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    // 获取值所在的寄存器，如果不在寄存器中则分配新寄存器并加载，返回寄存器名
    std::string GetValueReg(Value* val);
    // 将值加载到*指定*寄存器（用于函数调用参数等场景）
    void LoadValueToReg(Value* val, const std::string& target_reg);
    
    // 获取符号的地址并返回
    // 1. 局部变量: 计算 sp + offset 得到地址
    // 2. 全局变量: 使用 la 加载标签地址
    // 3. getelemptr 结果: 从栈中加载已计算的地址
    // 4. 函数参数引用: 从栈或寄存器获取地址
    std::string GetValueAddr(Value* val);
    
    // 检查偏移量是否在有效范围内 (-2048 到 2047)
    bool IsValidOffset(int offset) {
      return offset >= -2048 && offset <= 2047;
    }
    
    // 计算栈地址 sp + offset 到指定寄存器，最后存储地址到 reg 中
    void GetStackAddr(int offset, const std::string& reg);
    
    // 将寄存器中的值存储到栈中，生成 sw 指令，处理大偏移量
    void StoreWord(const std::string& src_reg, int offset);
    
    // 将值从栈中加载到指定寄存器，生成 lw 指令，处理大偏移量
    void LoadWord(const std::string& dst_reg, int offset);

    std::fstream fs;
    std::unique_ptr<RegAllocator> reg_allocator;
};

