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

void AssemblyGenerator::Visit(Program *program) {
  LOG("Program");
  // 访问所有全局变量

  // 访问所有函数
  for (auto& func : program->functions) {
    fs << " .text" << std::endl;
    fs << " .global " << func->name << std::endl;
    func->Accept(this);
    fs << std::endl;
    reg_allocator->Reset();
  } 
}

void AssemblyGenerator::Visit(Function *func) {
  LOG("Function");
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
    }
    reg_allocator->Clear();
  }
  for (auto& block : func->blocks) {
    block->Accept(this);
  }

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
  if (reg_allocator->ra_used) {
    fs << " lw    ra, " << (reg_allocator->stack_offset - 4) << "(sp)" << std::endl;
  }
  if (return_val->val == nullptr) {
    fs << " ret" << std::endl;
    return;
  }
  // 访问返回语句
  if (return_val->val->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    a0, " << return_val->val->name << std::endl;
  } else {
    // 从栈中加载栈的偏移
    int offset = reg_allocator->GetLoc(return_val->val.get());
    // 从栈中加载值
    fs << " lw    a0, " << offset << "(sp)" << std::endl;
  }
  reg_allocator->Clear();
  // 恢复栈指针
  if (reg_allocator->stack_offset != 0) {
    fs << " addi  sp, sp, " << reg_allocator->stack_offset << std::endl;
  }
  fs << " ret" << std::endl;
}

void AssemblyGenerator::Visit(Value_BINARY *binary) {
  LOG("Value_BINARY");
  auto l_r = reg_allocator->Lookup(binary->lhs.get());
  auto r_r = reg_allocator->Lookup(binary->rhs.get());
  if (l_r == "") {
    l_r = reg_allocator->Alloc(binary->lhs.get());
    if (binary->lhs->type == Value_Type::KOOPA_RVT_INTEGER) {
      fs << " li    " << l_r << ", " << binary->lhs->name << std::endl;
    } else {
      reg_allocator->GetLoc(binary->lhs.get());
      fs << " lw    " << l_r << ", " << reg_allocator->GetLoc(binary->lhs.get()) << "(sp)" << std::endl;
    }
  }
  if (r_r == "") {
    r_r = reg_allocator->Alloc(binary->rhs.get());
    if (binary->rhs->type == Value_Type::KOOPA_RVT_INTEGER) {
      fs << " li    " << r_r << ", " << binary->rhs->name << std::endl;
    } else {
      reg_allocator->GetLoc(binary->rhs.get());
      fs << " lw    " << r_r << ", " << reg_allocator->GetLoc(binary->rhs.get()) << "(sp)" << std::endl;
    }
  }
  // auto l_r = reg_allocator->Alloc(binary->lhs.get());
  // assert(l_r != "");
  // auto r_r = reg_allocator->Alloc(binary->rhs.get());
  // assert(r_r != "");
  // if (binary->lhs->type == Value_Type::KOOPA_RVT_INTEGER) {
  //   fs << " li    " << l_r << ", " << binary->lhs->name << std::endl;
  // } else {
  //   reg_allocator->GetLoc(binary->lhs.get());
  //   fs << " lw    " << l_r << ", " << reg_allocator->GetLoc(binary->lhs.get()) << "(sp)" << std::endl;
  // }
  // if (binary->rhs->type == Value_Type::KOOPA_RVT_INTEGER) {
  //   fs << " li    " << r_r << ", " << binary->rhs->name << std::endl;
  // } else {
  //   reg_allocator->GetLoc(binary->rhs.get());
  //   fs << " lw    " << r_r << ", " << reg_allocator->GetLoc(binary->rhs.get()) << "(sp)" << std::endl;
  // }
  auto offset = reg_allocator->GetLoc(binary);
  // 访问二元表达式
  switch (binary->type) {
    case Binary_Op_Type::KOOPA_RBO_EQ: {
      // 为当前Value分配寄存器
    auto res = reg_allocator->Alloc(binary);
      fs << " xor   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " seqz  " << res << ", " << res  << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_NOT_EQ: {
      // 为当前Value分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " xor   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " snez  " << res << ", " << res  << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_SUB: {
      // 为右操作数分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " sub   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_MUL: {
      // 为右操作数分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " mul   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_DIV: {
      // 为右操作数分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " div   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_ADD: {
      // 为右操作数分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " add   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_MOD: {
      // 为当前Value分配寄存器
      auto res = reg_allocator->Alloc(binary); 
      fs << " rem   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_OR: {
      // 为右操作数分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " or    " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_AND: {
      // 为右操作数分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " and   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_GE: {
      // 为当前Value分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " slt   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " xori  " << res << ", " << res << ", 1"<< std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_LE: {
      // 为当前Value分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " slt   " << res << ", " << r_r << ", " << l_r << std::endl;
      fs << " xori  " << res << ", " << res << ", 1"<< std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_LT: {
      // 为当前Value分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " slt   " << res << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_GT: {
      // 为当前Value分配寄存器
      auto res = reg_allocator->Alloc(binary);
      fs << " slt   " << res << ", " << r_r << ", " << l_r << std::endl;
      fs << " sw    " << res << ", " << offset << "(sp)" << std::endl;
      break;
    }
    default:
      break;
  }
  reg_allocator->Clear();
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
  // 访问载入指令 %0 = load @x (将 @x 中的值存到 %0 中)
  // 1. 访问 @x 并分配寄存器
  auto r = reg_allocator->Alloc(load->src.get());
  if (load->src->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    " << r << ", " << load->src->name << std::endl;
  } else {
    reg_allocator->GetLoc(load->src.get());
    fs << " lw    " << r << ", " << reg_allocator->GetLoc(load->src.get()) << "(sp)" << std::endl;
  }
  // 2. 从栈中加载当前 %0 的偏移，并将 @x 的值存到 %0 中
  int offset = reg_allocator->GetLoc(load);
  fs << " sw    " << r << ", " << offset << "(sp)" << std::endl;
  
  reg_allocator->Clear();
}

void AssemblyGenerator::Visit(Value_STORE *store) {
  LOG("Value_STORE");
  // 访问存储指令 store %0, @x (将 %0 中的值存到 @x 中)
  // 1. 访问 %0 并分配寄存器
    auto r = reg_allocator->Lookup(store->value.get());
  if (r == "") {
    r = reg_allocator->Alloc(store->value.get());
    if (store->value->type == Value_Type::KOOPA_RVT_INTEGER) {
      fs << " li    " << r << ", " << store->value->name << std::endl;
    } else {
      reg_allocator->GetLoc(store->value.get());
      fs << " lw    " << r << ", " << reg_allocator->GetLoc(store->value.get()) << "(sp)" << std::endl;
    }
  }
  // 2. 从栈中加载当前 @x 的偏移，将 %0 中的值存到 @x 中
  int offset = reg_allocator->GetLoc(store->dst.get());
  fs << " sw    " << r << ", " << offset << "(sp)" << std::endl;

  reg_allocator->Clear();
}

void AssemblyGenerator::Visit(Value_JUMP *jump) {
  LOG("Value_JUMP");
  // 访问跳转指令 jump %end
  fs << " j " << jump->dst_name << std::endl;
}

void AssemblyGenerator::Visit(Value_BRANCH *branch) {
  LOG("Value_BRANCH");
  // 访问分支指令 br %0, %then, %else
  // 1. 访问 %0 并分配寄存器
  auto r = reg_allocator->Alloc(branch->cond.get());
  if (branch->cond->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    " << r << ", " << branch->cond->name << std::endl;
  } else {
    fs << " lw    " << r << ", " << reg_allocator->GetLoc(branch->cond.get()) << "(sp)" << std::endl;
  }
  // 2. 访问 %then, %else 并分配寄存器
  fs << " bnez  " << r << ", " << branch->then_name << std::endl;
  fs << " j     " << branch->else_name << std::endl;
}

void AssemblyGenerator::Visit(Value_Call *call) {
  LOG("Value_Call");
  // 访问调用指令 call %0, @func
  // 1. 存放参数
  // 当参数编号小于8时，存放到a0-a7寄存器中
  for (size_t i = 0; i < call->args.size() && i < 8; i++) {
    auto arg = call->args[i].get();
    std::string r = "a" + std::to_string(i);
    if (arg->type == Value_Type::KOOPA_RVT_INTEGER) {
      fs << " li    " << r << ", " << arg->name << std::endl;
    } else {
      auto r1 = reg_allocator->Lookup(arg);
      if (r1 == "") {
        // 如果不在寄存器中，从栈中读取到寄存器中
        fs << " lw    " << r << ", " << reg_allocator->GetLoc(arg) << "(sp)" << std::endl;
      } else {
        // 如果在寄存器中，直接将值移动到a0-a7寄存器中
        fs << " mv    " << r << ", " << r1 << std::endl;
      }
    }
  }
  // 当参数编号大于等于8时，存放到栈中
  for (int i = 8; i < call->args.size(); i++) {
    auto arg = call->args[i].get();
    if (arg->type == Value_Type::KOOPA_RVT_INTEGER) {
      auto r = reg_allocator->Alloc(arg);
      fs << " li    " << r << ", " << arg->name << std::endl;
      fs << " sw    " << r << ", " << reg_allocator->param_offset + 4 * (i - 8) << "(sp)" << std::endl;
    } else {
      auto r = reg_allocator->Lookup(arg);
      if (r == "") {
        // 如果不在寄存器中，从栈中读取到寄存器中
        r = reg_allocator->Alloc(arg);
        fs << " lw    " << r << ", " << reg_allocator->GetLoc(arg) << "(sp)" << std::endl;
      }
      fs << " sw    " << r << ", " << reg_allocator->param_offset + 4 * (i - 8) << "(sp)" << std::endl;
    }
    reg_allocator->Clear();
  }
  fs << " call " << call->ident.substr(1) << std::endl;
  reg_allocator->Clear();
  // 2. 如果有返回值，将返回值存到栈中
  if (call->name != "null") {
    fs << " sw    a0, " << reg_allocator->GetLoc(call) << "(sp)" << std::endl;
  }
}
