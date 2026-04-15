#include "IRGenerator.h"
#include "IR.h"
#include "AST.h"

IRgenerator::IRgenerator() : program(new Program()) {}

IRgenerator::~IRgenerator() {
      delete program;
    }

void IRgenerator::Visit(CompUnitAST* ast) {
  ast->func_def->Accept(this);

  program->functions.push_back(current_func);
}

void IRgenerator::Visit(FuncTypeAST* ast) {
  // std::cout << "FuncTypeAST { " << ast->type << " }" << std::endl;
}

void IRgenerator::Visit(FuncDefAST* ast) {
  // std::cout << "FuncDefAST { " << ast->type << " }" << std::endl;
  ast->block->Accept(this);
  current_func = new Function("i32", ast->ident, {});
  current_func->blocks.push_back(current_block);
}

void IRgenerator::Visit(BlockAST* ast) {
  // std::cout << "BlockAST { " << ast->type << " }" << std::endl;
  ast->stmt->Accept(this);
  current_block = new BasicBlock({});
  current_block->stmts.push_back(current_value);
}

void IRgenerator::Visit(StmtAST* ast) {
  // std::cout << "StmtAST { " << ast->type << " }" << std::endl;
  current_value  = new Value_RETURN(ast->number->num);
}

void IRgenerator::Visit(NumberAST* ast) {
  // std::cout << "NumberAST { " << ast->type << " }" << std::endl;
}

void IRgenerator::OutputIR(std::string output) {
  IROutputer out(output);
  program->Accept(&out);
}

IROutputer::IROutputer() {}

IROutputer::IROutputer(std::string output) : fs(output, std::ios::out) {}

IROutputer::~IROutputer() {}

void IROutputer::Visit(Value* val) {
  if (val->name == "return") {
    fs << "  ret " << ((Value_RETURN*)val)->val << std::endl;
  }
}

void IROutputer::Visit(BasicBlock* block) {
  fs << "%entry: " << std::endl;
  for (auto stmt : block->stmts) {
    stmt->Accept(this);
  }
}

void IROutputer::Visit(Function* func) {
  fs << "fun @" << func->name << "(): " << func->type.name << " {" << std::endl;
  for (auto block : func->blocks) {
    block->Accept(this);
  }
  fs << "}" << std::endl;
}

void IROutputer::Visit(Program* program) {
  for (auto func : program->functions) {
    func->Accept(this);
  }
}










