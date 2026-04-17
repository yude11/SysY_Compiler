#include "AssemblyGenerator.h"
#include "IR.h"
#include "log.h"


AssemblyGenerator::AssemblyGenerator(std::string output) {
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
  // 访问基本块体语句
  for (auto& stmt : block->stmts) {
    stmt->Accept(this);
  }
}

void AssemblyGenerator::Visit(Value_RETURN *return_val) {
  LOG("Value_RETURN");
  // 访问返回语句
  if (return_val->val->type == Value_Type::KOOPA_RVT_INTEGER) {
    fs << " li    a0, " << return_val->val->name << std::endl;
  } else {
    auto r = reg_allocator.Lookup(return_val->val.get());
    assert(r != "");
    fs << " mv    a0, " << r << std::endl;
  }
  fs << " ret" << std::endl;
}

void AssemblyGenerator::Visit(Value_INTEGER *integer) {
  LOG("Value_INTEGER");
  // 访问整数值
  auto r = reg_allocator.Lookup(integer);
  if (r == "") {
    if (integer->val == 0) {
      r = reg_allocator.AllocZero(integer);
      return ;
    } else {
      r = reg_allocator.Alloc(integer);
    }
  }
  fs << " li    " << r << ", " << integer->val << std::endl;
}

void AssemblyGenerator::Visit(Value_BINARY *binary) {
  LOG("Value_BINARY");
  if (binary->lhs->type == Value_Type::KOOPA_RVT_INTEGER) {
    binary->lhs->Accept(this);
  }
  if (binary->rhs->type == Value_Type::KOOPA_RVT_INTEGER) {
    binary->rhs->Accept(this);
  }

  auto l_r = reg_allocator.Lookup(binary->lhs.get());
  assert(l_r != "");
  auto r_r = reg_allocator.Lookup(binary->rhs.get());
  assert(r_r != "");
  // 访问二元表达式
  switch (binary->type) {
    case Binary_Op_Type::KOOPA_RBO_EQ: {
      // 为当前Value分配寄存器
      reg_allocator.Copy(binary->lhs.get(), binary);
      fs << " xor   " << l_r << ", " << l_r << ", " << r_r << std::endl;
      fs << " seqz  " << l_r << ", " << l_r  << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_SUB: {
      // 为右操作数分配寄存器
      auto res = reg_allocator.Alloc(binary);
      fs << " sub   " << res << ", " << l_r << ", " << r_r << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_MUL: {
      // 为右操作数分配寄存器
      reg_allocator.Copy(binary->rhs.get(), binary);
      fs << " mul   " << r_r << ", " << l_r << ", " << r_r << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_DIV: {
      // 为右操作数分配寄存器
      reg_allocator.Copy(binary->rhs.get(), binary);
      fs << " div   " << r_r << ", " << l_r << ", " << r_r << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_ADD: {
      // 为右操作数分配寄存器
      reg_allocator.Copy(binary->rhs.get(), binary);
      fs << " add   " << r_r << ", " << l_r << ", " << r_r << std::endl;
      break;
    }
    default:
      break;
  }
}

void AssemblyGenerator::Visit(Value_REF *ref) {
  // 访问引用
  fs << reg_allocator.Lookup(ref);
}
