#include "AssemblyGenerator.h"
#include "IR.h"
#include "log.h"


AssemblyGenerator::AssemblyGenerator(std::string output, std::unique_ptr<SymbolTable> symbol_table) {
  this->symbol_table = std::move(symbol_table);
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
  fs << " .text" << std::endl;
  for (auto& func : program->functions) {
    fs << " .global " << func->name << std::endl;
  }
  for (auto& func : program->functions) {
    func->Accept(this);
  } 
}

void AssemblyGenerator::Visit(Function *func) {
  LOG("Function");
  // 访问函数体基本块
  fs << func->name << ":" << std::endl;
  for (auto& block : func->blocks) {
    block->Accept(this);
  }

}

void AssemblyGenerator::Visit(BasicBlock *block) {
  LOG("BasicBlock");
  // 扫描基本块体语句，分配栈空间
  reg_allocator->Scan(block);
  fs << " addi  sp, sp, -" << reg_allocator->stack_offset << std::endl;
  // 访问基本块体语句
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
  fs << " addi  sp, sp, " << reg_allocator->stack_offset << std::endl;
  fs << " ret" << std::endl;
}

void AssemblyGenerator::Visit(Value_BINARY *binary) {
  LOG("Value_BINARY");
  auto l_r = reg_allocator->Alloc(binary->lhs.get());
  assert(l_r != "");
  auto r_r = reg_allocator->Alloc(binary->rhs.get());
  assert(r_r != "");
  if (binary->lhs->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    " << l_r << ", " << binary->lhs->name << std::endl;
  } else {
    reg_allocator->GetLoc(binary->lhs.get());
    fs << " lw    " << l_r << ", " << reg_allocator->GetLoc(binary->lhs.get()) << "(sp)" << std::endl;
  }
  if (binary->rhs->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    " << r_r << ", " << binary->rhs->name << std::endl;
  } else {
    reg_allocator->GetLoc(binary->rhs.get());
    fs << " lw    " << r_r << ", " << reg_allocator->GetLoc(binary->rhs.get()) << "(sp)" << std::endl;
  }
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
      reg_allocator->Copy(binary->lhs.get(), binary);
      fs << " xor   " << l_r << ", " << l_r << ", " << r_r << std::endl;
      fs << " snez  " << l_r << ", " << l_r  << std::endl;
      fs << " sw    " << l_r << ", " << offset << "(sp)" << std::endl;
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
      reg_allocator->Copy(binary->rhs.get(), binary);
      fs << " mul   " << r_r << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << r_r << ", " << offset << "(sp)" << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_DIV: {
      // 为右操作数分配寄存器
      reg_allocator->Copy(binary->rhs.get(), binary);
      fs << " div   " << r_r << ", " << l_r << ", " << r_r << std::endl;
      fs << " sw    " << r_r << ", " << offset << "(sp)" << std::endl;
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

void AssemblyGenerator::Visit(Value_REF *ref) {
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
  auto r = reg_allocator->Alloc(store->value.get());
  if (store->value->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    " << r << ", " << store->value->name << std::endl;
  } else {
    reg_allocator->GetLoc(store->value.get());
    fs << " lw    " << r << ", " << reg_allocator->GetLoc(store->value.get()) << "(sp)" << std::endl;
  }
  // 2. 从栈中加载当前 @x 的偏移，将 %0 中的值存到 @x 中
  int offset = reg_allocator->GetLoc(store->dst.get());
  fs << " sw    " << r << ", " << offset << "(sp)" << std::endl;

  reg_allocator->Clear();
}

