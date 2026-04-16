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
  // 访问函数体基本块
  fs << func->name << ":" << std::endl;
  for (auto& block : func->blocks) {
    block->Accept(this);
  }

}

void AssemblyGenerator::Visit(BasicBlock *block) {
  // 访问基本块体语句
  for (auto& stmt : block->stmts) {
    stmt->Accept(this);
  }
}

void AssemblyGenerator::Visit(Value_RETURN *return_val) {
  // 访问返回语句
  fs << " li a0, ";
  return_val->val->Accept(this);
  fs << std::endl;
  fs << " ret" << std::endl;
}

void AssemblyGenerator::Visit(Value_INTEGER *integer) {
  // 访问整数值
  fs << integer->val;
}

void AssemblyGenerator::Visit(Value_BINARY *binary) {
  // 访问二元表达式
  switch (binary->type) {
    case Binary_Op_Type::KOOPA_RBO_EQ: {
      std::string r = "t"+binary->name.substr(1);
      fs << " li  " << r << ", ";
      binary->lhs->Accept(this);
      fs << std::endl;
      fs << " xor " << r << ", " << r << ", " << "x0" << std::endl;
      fs << " seqz " << r << ", " << r  << std::endl;
      break;
    }
    case Binary_Op_Type::KOOPA_RBO_SUB: {
      std::string r = "t"+binary->name.substr(1);
      fs << " sub "+r+", a0, ";
      binary->rhs->Accept(this);
      fs << std::endl;
      break;
    }
    default:
      break;
  }
}

void AssemblyGenerator::Visit(Value_REF *ref) {
  // 访问引用

  fs << "t"+ref->name.substr(1);
}
