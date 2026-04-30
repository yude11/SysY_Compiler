#include "AssemblyGenerator.h"
#include "IR.h"
#include "log.h"


AssemblyGenerator::AssemblyGenerator(std::string output) {
  reg_allocator = std::make_unique<RegAllocator>();
  fs.open(output, std::ios::out);
}

AssemblyGenerator::~AssemblyGenerator() {
  fs.close();
}

std::string AssemblyGenerator::GetValueReg(Value* val) {
  if (val->type == Value_Type::KOOPA_RVT_INTEGER) {
    auto reg = reg_allocator->Alloc(val);
    fs << " li    " << reg << ", " << val->name << std::endl;
    return reg;
  }
  
  auto pos = reg_allocator->GetWhereIsValue(val);
  switch (pos) {
    case 0:  // 已在寄存器 t 中
      return reg_allocator->value_to_reg_t[val];
    case 1:  // 已在寄存器 a 中
      return reg_allocator->value_to_reg_a[val];
    case 2: {  // 在栈中，分配新寄存器并加载
      auto reg = reg_allocator->Alloc(val);
      fs << " lw    " << reg << ", " << reg_allocator->GetLoc(val) << "(sp)" << std::endl;
      return reg;
    }
    case 3: {  // 全局变量，分配新寄存器并加载值
      auto reg = reg_allocator->Alloc(val);
      fs << " la    " << reg << ", " << reg_allocator->global_to_label[val] << std::endl;
      fs << " lw    " << reg << ", 0(" << reg << ")" << std::endl;
      return reg;
    }
    default:
      assert(0 && "error: unknown where_is_value");
      return "";
  }
}

void AssemblyGenerator::LoadValueToReg(Value* val, const std::string& target_reg) {
  if (val->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    " << target_reg << ", " << val->name << std::endl;
    return;
  }
  
  auto pos = reg_allocator->GetWhereIsValue(val);
  switch (pos) {
    case 0:  // 已在寄存器 t 中
      fs << " mv    " << target_reg << ", " << reg_allocator->value_to_reg_t[val] << std::endl;
      break;
    case 1:  // 已在寄存器 a 中
      fs << " mv    " << target_reg << ", " << reg_allocator->value_to_reg_a[val] << std::endl;
      break;
    case 2:  // 在栈中
      fs << " lw    " << target_reg << ", " << reg_allocator->GetLoc(val) << "(sp)" << std::endl;
      break;
    case 3:  // 全局变量
      fs << " la    " << target_reg << ", " << reg_allocator->global_to_label[val] << std::endl;
      fs << " lw    " << target_reg << ", 0(" << target_reg << ")" << std::endl;
      break;
    default:
      assert(0 && "error: unknown where_is_value");
      break;
  }
}

void AssemblyGenerator::Visit(Program *program) {
  LOG("Program");
  // 访问所有全局变量
  fs << ".data" << std::endl;
  for (auto& glob_alloc : program->values) {
    glob_alloc->Accept(this);
  }
  fs << std::endl;
  // 访问所有函数
  for (auto& func : program->functions) {
    func->Accept(this);
    reg_allocator->ResetForNewFunction();
  } 
}

void AssemblyGenerator::Visit(Function *func) {
  LOG("Function");
  fs << " .text" << std::endl;
  fs << " .global " << func->name << std::endl;
  // 访问函数体基本块
  fs << func->name << ":" << std::endl;
  bool has_call = reg_allocator->Scan(func);
  if (reg_allocator->stack_offset != 0) {
    fs << " addi  sp, sp, -" << reg_allocator->stack_offset << std::endl;
    if (has_call) {
      fs << " sw    ra, " << (reg_allocator->stack_offset - 4) << "(sp)" << std::endl;
    }
    // 将参数存入栈中
    for (size_t i = 0; i < func->params.size() && i < 8; i++) {
      auto r = "a" + std::to_string(i);
      auto param = func->params[i].get();
      reg_allocator->AllocParams(param);
    }
    for (size_t i = 8; i < func->params.size(); i++) {
      reg_allocator->value_to_loc[func->params[i].get()] = reg_allocator->stack_offset + (i - 8) * 4;
      reg_allocator->where_is_value[func->params[i].get()] = 2;  // 标记为在栈中
    }
    reg_allocator->ClearTempRegs();
  }
  for (auto& block : func->blocks) {
    block->Accept(this);
  }
  fs << std::endl;
}

void AssemblyGenerator::Visit(Function_Decl *func_decl) {
  LOG("Function_Decl");
  // 函数声明不需要生成代码
}

void AssemblyGenerator::Visit(BasicBlock *block) {
  LOG("BasicBlock");
  // 扫描基本块体语句，分配栈空间
  // 访问基本块体语句
  if (block->name != "entry") {
    fs << block->name << ":" << std::endl;
  }
  for (auto& stmt : block->stmts) {
    if (stmt->type == Value_Type::KOOPA_RVT_ALLOC) {
      continue;
    } else {
      stmt->Accept(this);
    }
  }
}

void AssemblyGenerator::Visit(Value_RETURN *return_val) {
  LOG("Value_RETURN");
  // 检查是否需要恢复ra寄存器
  if (reg_allocator->ra_used) {
    fs << " lw    ra, " << (reg_allocator->stack_offset - 4) << "(sp)" << std::endl;
  }
  if (return_val->val == nullptr) {
    if (reg_allocator->stack_offset != 0) {
      fs << " addi  sp, sp, " << reg_allocator->stack_offset << std::endl;
    }
    fs << " ret" << std::endl;
    return;
  }
  auto reg = GetValueReg(return_val->val.get());
  if (reg != "a0") {
    fs << " mv    a0, " << reg << std::endl;
  }
  reg_allocator->ClearTempRegs();
  if (reg_allocator->stack_offset != 0) {
    fs << " addi  sp, sp, " << reg_allocator->stack_offset << std::endl;
  }
  fs << " ret" << std::endl;
}

void AssemblyGenerator::Visit(Value_BINARY *binary) {
  LOG("Value_BINARY");
  
  std::string l_r = GetValueReg(binary->lhs.get());
  std::string r_r = GetValueReg(binary->rhs.get());
  
  auto offset = reg_allocator->GetLoc(binary);
  auto res = reg_allocator->Alloc(binary);
  
  switch (binary->type) {
    case Binary_Op_Type::KOOPA_RBO_EQ:
      fs << " xor   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " seqz  " << res << ", " << res << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_NOT_EQ:
      fs << " xor   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " snez  " << res << ", " << res << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_SUB:
      fs << " sub   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_MUL:
      fs << " mul   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_DIV:
      fs << " div   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_ADD:
      fs << " add   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_MOD:
      fs << " rem   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_OR:
      fs << " or    " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_AND:
      fs << " and   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_GE:
      fs << " slt   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " xori  " << res << ", " << res << ", 1" << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_LE:
      fs << " slt   " << res << ", " << r_r << ", " << l_r << std::endl;
      fs << " xori  " << res << ", " << res << ", 1" << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_LT:
      fs << " slt   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_GT:
      fs << " slt   " << res << ", " << r_r << ", " << l_r << std::endl;
      break;
    default:
      break;
  }
  fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
  reg_allocator->ClearTempRegs();
}

void AssemblyGenerator::Visit(Value_FUNC_ARG_REF *ref) {
  // 访问引用
  fs << reg_allocator->Lookup(ref);
}

void AssemblyGenerator::Visit(Value_ALLOC *alloc) {
  LOG("Value_ALLOC");
  // 访问分配指令
  // 1. 从栈中加载栈的偏移
  int offset = reg_allocator->GetLoc(alloc);
  // 2. 分配寄存器，将对应偏移加载到寄存器中
  auto r = reg_allocator->Alloc(alloc);
  fs << " lw    " << r << ", " << offset << "(sp)" << std::endl;
}

void AssemblyGenerator::Visit(Value_INTEGER *integer) {
  LOG("Value_INTEGER");
  // 遇见整数值直接加载到寄存器
  std::string r = "";
  if (integer->val == 0) {
    r = reg_allocator->AllocZero(integer);
    return ;
  } else {
    r = reg_allocator->Alloc(integer);
  }
  fs << " li    " << r << ", " << integer->val << std::endl;
}

void AssemblyGenerator::Visit(Value_LOAD *load) {
  LOG("Value_LOAD");
  
  auto r = GetValueReg(load->src.get());
  int offset = reg_allocator->GetLoc(load);
  fs << " sw    " << r << ", " << offset << "(sp)" << std::endl;
  
  reg_allocator->ClearTempRegs();
}

void AssemblyGenerator::Visit(Value_STORE *store) {
  LOG("Value_STORE");
  
  auto r = GetValueReg(store->value.get());
  
  if (store->dst->type != Value_Type::KOOPA_RVT_INTEGER) {
    auto pos = reg_allocator->GetWhereIsValue(store->dst.get());
    if (pos == 3) {
      auto dst_reg = reg_allocator->Alloc(store->dst.get());
      fs << " la    " << dst_reg << ", " << reg_allocator->global_to_label[store->dst.get()] << std::endl;
      fs << " sw    " << r << ", 0(" << dst_reg << ")" << std::endl;
      reg_allocator->ClearTempRegs();
      return;
    }
  }
  
  int offset = reg_allocator->GetLoc(store->dst.get());
  fs << " sw    " << r << ", " << offset << "(sp)" << std::endl;

  reg_allocator->ClearTempRegs();
}

void AssemblyGenerator::Visit(Value_JUMP *jump) {
  LOG("Value_JUMP");
  // 访问跳转指令 jump %end
  fs << " j " << jump->dst_name << std::endl;
}

void AssemblyGenerator::Visit(Value_BRANCH *branch) {
  LOG("Value_BRANCH");
  
  auto r = GetValueReg(branch->cond.get());
  fs << " bnez  " << r << ", " << branch->then_name << std::endl;
  fs << " j     " << branch->else_name << std::endl;
}

void AssemblyGenerator::Visit(Value_Call *call) {
  LOG("Value_Call");
  
  for (size_t i = 0; i < call->args.size() && i < 8; i++) {
    std::string r = "a" + std::to_string(i);
    LoadValueToReg(call->args[i].get(), r);
  }
  for (int i = 8; i < call->args.size(); i++) {
    auto r = GetValueReg(call->args[i].get());
    fs << " sw    " << r << ", " << reg_allocator->param_offset + 4 * (i - 8) << "(sp)" << std::endl;
    reg_allocator->ClearTempRegs();
  }
  fs << " call " << call->ident.substr(1) << std::endl;
  reg_allocator->ClearTempRegs();
  if (call->name != "null") {
    fs << " sw    a0, " << reg_allocator->GetLoc(call) << "(sp)" << std::endl;
  }
}

void AssemblyGenerator::Visit(Value_GLOBOL_ALLOC* glob_alloc) {
  // 访问全局分配指令 glob_alloc %0 = @x
  reg_allocator->AllocGlobal(glob_alloc);
  fs << " .globl " << glob_alloc->name.substr(1) << std::endl;
  fs << glob_alloc->name.substr(1) << ":" << std::endl;
  fs << " .word " << glob_alloc->init->name << std::endl;
}
