#include "IRGenerator.h"
#include "IR.h"
#include "AST.h"
#include "type.h"
#include "log.h"



IRgenerator::IRgenerator() : program(std::make_unique<Program>()) {}

IRgenerator::~IRgenerator() {}

void IRgenerator::Visit(CompUnitAST* ast) {
  // LOG("CompUnitAST");
  current_func = std::make_unique<Function>();
  ast->func_def->Accept(this);
  // 当current_func构造完成后，将其加入到program中
  program->functions.push_back(std::move(current_func));
}

void IRgenerator::Visit(FuncTypeAST* ast) {
  // LOG("FuncTypeAST");
  // std::cout << "FuncTypeAST { " << ast->type << " }" << std::endl;
  current_func->type = Type(ast->type);
}

void IRgenerator::Visit(FuncDefAST* ast) {
  // std::cout << "FuncDefAST { " << ast->type << " }" << std::endl;
  current_block = std::make_unique<BasicBlock>("null");
  ast->func_type->Accept(this);
  current_func->name = ast->ident;
  // 访问block中的语句，构造current_block
  ast->block->Accept(this);
  // 当current_block构造完成后，将其加入到current_func中
  current_func->blocks.push_back(std::move(current_block));
}

void IRgenerator::Visit(BlockAST* ast) {
  // std::cout << "BlockAST { " << ast->type << " }" << std::endl;
  ast->stmt->Accept(this);
}

void IRgenerator::Visit(StmtAST* ast) {
  switch (ast->type) {
    case Stmt_Type::AST_STMT_RETURN: {
      ast->number->Accept(this);
      // 从栈中弹出值(为引用)
      auto val = std::move(current_value_stack.top());
      current_value_stack.pop();
      // 将val加入到block
      current_block->stmts.push_back(std::make_unique<Value_RETURN>(std::move(val)));
      break;
    }
    case Stmt_Type::AST_STMT_ASSIGN: {
      // TODO
      break;
    }
  }
}

void IRgenerator::Visit(NumberAST* ast) {
  current_value_stack.push(std::make_unique<Value_INTEGER>(ast->num));
}

void IRgenerator::Visit(ExpAST* ast) {
  ast->unary_exp->Accept(this);
}

void IRgenerator::Visit(PrimaryExpAST* ast) {
    ast->mem->Accept(this);
}

void IRgenerator::Visit(UnaryExpAST* ast) {
  // LOG("UnaryExpAST");
  if (ast->type == 0) {
        // PrimaryExp
        auto& primary = std::get<std::unique_ptr<BaseAST>>(ast->mem);
        primary->Accept(this);
    } else {
        // UnaryOp UnaryExp
        auto& unary = std::get<UnaryExpAST::UnaryExp>(ast->mem);
        unary.unary_exp->Accept(this);
        // 根据 unary.unary_op 的类型修改 current_value
        unary.unary_op->Accept(this);
    }
}

void IRgenerator::Visit(UnaryOpAST* ast) {
  // LOG("UnaryOpAST");
  // ast->unary_exp->Accept(this);
  std::unique_ptr<Value> rhs = std::move(current_value_stack.top());
  current_value_stack.pop();
  std::string name = "%" + std::to_string(count++);
  switch (ast->unaryop) {
    case Unary_Op_Type::AST_UNARY_OP_NEG: {
      // 将val实例加入到block
      auto val = std::make_unique<Value_BINARY>(name, 
                              std::unique_ptr<Value>(new Value_INTEGER(0)),
                              std::move(rhs), 
                              Binary_Op_Type::KOOPA_RBO_SUB
                            );
      current_block->stmts.push_back(std::move(val));
      
      // 生成引用加入栈中
      auto ref = std::make_unique<Value_REF>(name);
      current_value_stack.push(std::move(ref));
      break;
    }
    case Unary_Op_Type::AST_UNARY_OP_NOT: {
      // 将val实例加入到block
      current_block->stmts.push_back(
        std::make_unique<Value_BINARY>(name, 
          std::move(rhs), 
          std::unique_ptr<Value>(new Value_INTEGER(0)),
          Binary_Op_Type::KOOPA_RBO_EQ)
        );
        // 生成引用加入栈中
        auto ref = std::make_unique<Value_REF>(name);
        current_value_stack.push(std::move(ref));
        break;
      }
    case Unary_Op_Type::AST_UNARY_OP_POS:
      // 正号不产生新指令，把原值放回栈
      current_value_stack.push(std::move(rhs));
      break;
    default:
      break;
  }
}


void IRgenerator::OutputIR(std::string output) {
  IROutputer out(output);
  program->Accept(&out);
}

IROutputer::IROutputer() {}

IROutputer::IROutputer(std::string output) : fs(output, std::ios::out) {}

IROutputer::~IROutputer() {}

void IROutputer::Visit(Value_INTEGER* integer) {
    fs << integer->val;
}

void IROutputer::Visit(Value_RETURN* return_val) {
    fs << "  ret ";
    return_val->val->Accept(this); 
    fs << std::endl;
}

void IROutputer::Visit(Value_BINARY* binary) {
  switch (binary->type) {
    case Binary_Op_Type::KOOPA_RBO_EQ:
      fs << "  " << binary->name << " = eq ";
      binary->lhs->Accept(this);
      fs << ", ";
      binary->rhs->Accept(this);
      fs << std::endl;
      break;
    case Binary_Op_Type::KOOPA_RBO_SUB:
      fs << "  " << binary->name << " = sub ";
      binary->lhs->Accept(this);
      fs << ", ";
      binary->rhs->Accept(this);
      fs << std::endl;
      break;
    default:
      break;
  }
}

void IROutputer::Visit(Value_REF* ref) {
  fs << ref->name;
}

void IROutputer::Visit(BasicBlock* block) {
  fs << "%entry: " << std::endl;
  for (auto& stmt : block->stmts) {
    stmt->Accept(this);
  }
}

void IROutputer::Visit(Function* func) {
  fs << "fun @" << func->name << "(): " << func->type.name << " {" << std::endl;
  for (auto& block : func->blocks) {
    block->Accept(this);
  }
  fs << "}" << std::endl;
}

void IROutputer::Visit(Program* program) {
  for (auto& func : program->functions) {
    func->Accept(this);
  }
}

