#include "AssemblyGenerator.h"
#include "IR.h"


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
  for (auto func : program->functions) {
    fs << " .global " << func->name << std::endl;
  }
  for (auto func : program->functions) {
    func->Accept(this);
  } 
}

void AssemblyGenerator::Visit(Function *func) {
  // 访问函数体基本块
  fs << func->name << ":" << std::endl;
  for (auto block : func->blocks) {
    block->Accept(this);
  }

}

void AssemblyGenerator::Visit(BasicBlock *block) {
  // 访问基本块体语句
  for (auto stmt : block->stmts) {
    if (stmt->name == "return") {
      fs << " li a0, " << ((Value_RETURN*)stmt)->val << std::endl;
      fs << " ret" << std::endl;
    }
  }
}

void AssemblyGenerator::Visit(Value *val) {
  // 访问所有值
  val->Accept(this);
}
