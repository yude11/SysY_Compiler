#include "IRGenerator.h"
#include "IR.h"
#include "AST.h"
#include "type.h"
#include "log.h"



IRgenerator::IRgenerator() : program(std::make_unique<Program>()), symbol_table(std::make_unique<SymbolTable>()) {}

IRgenerator::~IRgenerator() {}

void IRgenerator::Visit(CompUnitAST* ast) {
  LOG("CompUnitAST");
  current_func = std::make_unique<Function>();
  ast->func_def->Accept(this);
  // 当current_func构造完成后，将其加入到program中
  program->functions.push_back(std::move(current_func));
}

void IRgenerator::Visit(FuncTypeAST* ast) {
  LOG("FuncTypeAST");
  // std::cout << "FuncTypeAST { " << ast->type << " }" << std::endl;
  current_func->type = Type(ast->type);
}

void IRgenerator::Visit(FuncDefAST* ast) {
  LOG("FuncDefAST");
  // std::cout << "FuncDefAST { " << ast->type << " }" << std::endl;
  current_block = std::make_unique<BasicBlock>("entry");
  ast->func_type->Accept(this);
  current_func->name = ast->ident;
  // 访问block中的语句，构造current_block
  ast->block->Accept(this);
  // 当current_block构造完成后，将其加入到current_func中
  current_func->blocks.push_back(std::move(current_block));
}

void IRgenerator::Visit(BlockAST* ast) {
  LOG("BlockAST");
  symbol_table->Push();
  // std::cout << "BlockAST { " << ast->type << " }" << std::endl;
  for (auto& item : *(ast->block_items)) {
    item->Accept(this);
    if (current_block->IsTerminated()) {
      break;
    }
  }
  symbol_table->Pop();
}

void IRgenerator::Visit(StmtAST* ast) {
  LOG("StmtAST");
  switch (ast->type) {
    case Stmt_Type::AST_STMT_RETURN: {
      auto& return_stmt = std::get<StmtAST::RETURN_STMT>(ast->stmt);
      return_stmt->Accept(this);
        // 从栈中弹出值
      assert(!current_value_stack.empty());
      auto val = current_value_stack.top();
      current_value_stack.pop();
      // 将val加入到block
      current_block->stmts.push_back(std::make_shared<Value_RETURN>(val));
      // 标记当前块已终止
      current_block->Terminate();
      break;
    }
    case Stmt_Type::AST_STMT_ASSIGN: {
      auto& assign_stmt = std::get<StmtAST::Assign_STMT>(ast->stmt);
      /// 首先进行语义判断，确保lval是可赋的变量
      auto lval = static_cast<LValAST*>(assign_stmt.lval.get());
      // 1. 判断lval是否在符号表中
      if (!symbol_table->IsContains(lval->ident)) {
        std::cerr << "error : " << lval->ident << " is not defined" << std::endl;
        assert(0);
      }
      // 2. 判断lval是否为常量
      auto symbol = symbol_table->FindSymbol(lval->ident);
      if (symbol.is_const) {
        std::cerr << "error : " << lval->ident << " is const" << std::endl;
        assert(0);
      }
      /// 获取变量地址（左值不需要 load）
      auto variable = symbol_table->FindValue(lval->ident);
      /// 计算右值表达式
      assign_stmt.exp->Accept(this);
      // 从栈中弹出值
      assert(!current_value_stack.empty());
      auto val = current_value_stack.top();
      current_value_stack.pop();
      /// 生成store指令
      current_block->stmts.push_back(std::make_shared<Value_STORE>(val, variable));
      break;
    }
    case Stmt_Type::AST_STMT_BLOCK: {
      auto& block_stmt = std::get<StmtAST::Block_STMT>(ast->stmt);
      block_stmt->Accept(this);
      break;
    }
    case Stmt_Type::AST_STMT_IF_ELSE: {
      auto& if_else_stmt = std::get<StmtAST::IfElse_STMT>(ast->stmt);
      
      // Step 1: 计算条件表达式
      if_else_stmt.exp->Accept(this);
      assert(!current_value_stack.empty());
      auto cond_val = current_value_stack.top();
      current_value_stack.pop();
      
      // Step 2: 生成基本块名称
      std::string then_name = "then" + std::to_string(if_count++);
      std::string end_name = "end" + std::to_string(end_count++);
      std::string else_name = if_else_stmt.else_stmt != nullptr 
                              ? "else" + std::to_string(else_count++) 
                              : end_name;
      
      // Step 3: 生成分支指令
      //   br %cond, %then, %else (或 %end 如果没有else)
      current_block->stmts.push_back(std::make_shared<Value_BRANCH>(cond_val, then_name, else_name));
      current_func->blocks.push_back(std::move(current_block));
      
      // Step 4: 生成 then 基本块
      //   ... if语句体 ...
      //   jump %end (如果不是return结尾)
      current_block = std::make_unique<BasicBlock>(then_name);
      if_else_stmt.if_stmt->Accept(this);
      if (!current_block->IsTerminated()) {
        current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
      }
      current_func->blocks.push_back(std::move(current_block));
      
      // Step 5: 生成 else 基本块 (如果有)
      //   ... else语句体 ...
      //   jump %end (如果不是return结尾)
      if (if_else_stmt.else_stmt != nullptr) {
        current_block = std::make_unique<BasicBlock>(else_name);
        if_else_stmt.else_stmt->Accept(this);
        if (!current_block->IsTerminated()) {
          current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
        }
        current_func->blocks.push_back(std::move(current_block));
      }
      
      // Step 6: 生成 end 基本块
      current_block = std::make_unique<BasicBlock>(end_name);
      break;
    }
    case Stmt_Type::AST_STMT_WHILE: {
      auto& while_stmt = std::get<StmtAST::While_STMT>(ast->stmt);
      
      // 1. 生成基本块信息
      while_entry_stack.push(while_count);
      std::string while_entry = "while_entry" + std::to_string(while_count);
      std::string then_name = "while_body" + std::to_string(while_count);
      std::string end_name = "while_end" + std::to_string(while_count++);
      
      // 2. 生成jump指令到while_entry
      current_block->stmts.push_back(std::make_shared<Value_JUMP>(while_entry));
      current_func->blocks.push_back(std::move(current_block));
      
      // 3. 生成个基本快
      // 生成entry基本块
      current_block = std::make_unique<BasicBlock>(while_entry);
      while_stmt.exp->Accept(this);
      assert(!current_value_stack.empty());
      auto cond_val = current_value_stack.top();
      current_value_stack.pop();
      current_block->stmts.push_back(std::make_shared<Value_BRANCH>(cond_val, then_name, end_name));
      current_func->blocks.push_back(std::move(current_block));
      // 生成then基本块
      current_block = std::make_unique<BasicBlock>(then_name);
      while_stmt.while_stmt->Accept(this);
      if (!current_block->IsTerminated()) {
        current_block->stmts.push_back(std::make_shared<Value_JUMP>(while_entry));
      }
      current_func->blocks.push_back(std::move(current_block));
      // 生成end基本块
      while_entry_stack.pop();
      current_block = std::make_unique<BasicBlock>(end_name);
      break;
    }
    case Stmt_Type::AST_STMT_BREAK: {
      std::string end_name = "while_end" + std::to_string(while_entry_stack.top());
      current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
      // 标记当前块已终止
      current_block->Terminate();
      break;
    }
    case Stmt_Type::AST_STMT_CONTINUE: {
      std::string while_entry_name = "while_entry" + std::to_string(while_entry_stack.top());
      current_block->stmts.push_back(std::make_shared<Value_JUMP>(while_entry_name));
      // 标记当前块已终止
      current_block->Terminate();
      break;
    }
    default:
      break;
  }
}

void IRgenerator::Visit(NumberAST* ast) {
  LOG("NumberAST");
  current_value_stack.push(std::make_shared<Value_INTEGER>(ast->num));
}

void IRgenerator::Visit(ExpAST* ast) {
  LOG("ExpAST");
  ast->l_or_exp->Accept(this);
}

void IRgenerator::Visit(PrimaryExpAST* ast) {
  LOG("PrimaryExpAST");
  ast->mem->Accept(this);
}

void IRgenerator::Visit(UnaryExpAST* ast) {
  LOG("UnaryExpAST");
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

void IRgenerator::Visit(LOrExpAST* ast) {
  LOG("LOrExpAST");
  if (ast->type == 0) {
    // 单一表达式，直接访问子节点
    auto& l_or = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    l_or->Accept(this);
  } else {
    // LOrExp || LAndExp
    // 短路求值算法: lhs || rhs
    //   int result = 1;
    //   if (lhs == 0) result = (rhs != 0);
    //   return result;
    auto& l_or = std::get<LOrExpAST::LOrExp>(ast->mem);
    
    // Step 1: 分配临时变量，默认值为 1 (true)
    //   @result = alloc i32
    //   store 1, @result
    auto alloc = std::make_shared<Value_ALLOC>("@result", "i32");
    symbol_table->Insert(alloc->name, alloc, false, "i32");
    auto assign = std::make_shared<Value_STORE>(std::make_shared<Value_INTEGER>(1), alloc);
    current_block->stmts.push_back(alloc);
    current_block->stmts.push_back(assign);
    
    // Step 2: 计算 lhs 和 rhs 的值
    l_or.l_or_exp->Accept(this);  // lhs
    auto lhs_val = current_value_stack.top();
    current_value_stack.pop();
    l_or.l_and_exp->Accept(this); // rhs
    auto rhs_val = current_value_stack.top();
    current_value_stack.pop();
    
    // Step 3: 生成分支指令
    //   %cond = eq lhs, 0
    //   br %cond, %then, %end
    auto then_name = "then" + std::to_string(if_count++);
    auto end_name = "end" + std::to_string(end_count++);
    auto eq = std::make_shared<Value_BINARY>("%"+std::to_string(count++), lhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_EQ);
    current_block->stmts.push_back(eq);
    auto if_branch = std::make_shared<Value_BRANCH>(eq, then_name, end_name);
    current_block->stmts.push_back(if_branch);
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 4: 生成 then 基本块 (lhs 为 false 时执行)
    //   %rhs_bool = ne rhs, 0
    //   store %rhs_bool, @result
    //   jump %end
    current_block = std::make_unique<BasicBlock>(then_name);
    auto neq = std::make_shared<Value_BINARY>("%"+std::to_string(count++), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(neq);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(neq, alloc));
    current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 5: 生成 end 基本块，加载结果并压栈
    //   %result = load @result
    current_block = std::make_unique<BasicBlock>(end_name);
    auto load = std::make_shared<Value_LOAD>("%"+std::to_string(count++), alloc);
    current_block->stmts.push_back(load);
    current_value_stack.push(load);
  }
}

void IRgenerator::Visit(LAndExpAST* ast) {
  LOG("LAndExpAST");
  if (ast->type == 0) {
    // 单一表达式，直接访问子节点
    auto& l_and = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    l_and->Accept(this);
  } else {
    // LAndExp && EqExp
    // 短路求值算法: lhs && rhs
    //   int result = 0;
    //   if (lhs != 0) result = (rhs != 0);
    //   return result;
    auto& l_and = std::get<LAndExpAST::LAndExp>(ast->mem);
    
    // Step 1: 分配临时变量，默认值为 0 (false)
    //   @result = alloc i32
    //   store 0, @result
    auto alloc = std::make_shared<Value_ALLOC>("@result", "i32");
    symbol_table->Insert(alloc->name, alloc, false, "i32");
    auto assign = std::make_shared<Value_STORE>(std::make_shared<Value_INTEGER>(0), alloc);
    current_block->stmts.push_back(alloc);
    current_block->stmts.push_back(assign);
    
    // Step 2: 计算 lhs 和 rhs 的值
    l_and.l_and_exp->Accept(this);  // lhs
    auto lhs_val = current_value_stack.top();
    current_value_stack.pop();
    l_and.eq_exp->Accept(this);     // rhs
    auto rhs_val = current_value_stack.top();
    current_value_stack.pop();
    
    // Step 3: 生成分支指令
    //   %cond = ne lhs, 0    (或 eq lhs, 1)
    //   br %cond, %then, %end
    auto then_name = "then" + std::to_string(if_count++);
    auto end_name = "end" + std::to_string(end_count++);
    auto eq = std::make_shared<Value_BINARY>("%"+std::to_string(count++), lhs_val, std::make_shared<Value_INTEGER>(1), Binary_Op_Type::KOOPA_RBO_EQ);
    current_block->stmts.push_back(eq);
    auto if_branch = std::make_shared<Value_BRANCH>(eq, then_name, end_name);
    current_block->stmts.push_back(if_branch);
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 4: 生成 then 基本块 (lhs 为 true 时执行)
    //   %rhs_bool = ne rhs, 0
    //   store %rhs_bool, @result
    //   jump %end
    current_block = std::make_unique<BasicBlock>(then_name);
    auto neq = std::make_shared<Value_BINARY>("%"+std::to_string(count++), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(neq);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(neq, alloc));
    current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 5: 生成 end 基本块，加载结果并压栈
    //   %result = load @result
    current_block = std::make_unique<BasicBlock>(end_name);
    auto load = std::make_shared<Value_LOAD>("%"+std::to_string(count++), alloc);
    current_block->stmts.push_back(load);
    current_value_stack.push(load);
  }
}

void IRgenerator::Visit(EqExpAST* ast) {
  LOG("EqExpAST");
  if (ast->type == 0) {
    // EqExp
    auto& eq = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    eq->Accept(this);
  } else {
    // EqExp BinaryOp EqExp
    auto& eq = std::get<EqExpAST::EqExp>(ast->mem);
    eq.eq_exp->Accept(this);
    eq.rel_exp->Accept(this);
    // 先访问左右操作数，再访问 eq_op
    eq.eq_op->Accept(this);
  }
}

void IRgenerator::Visit(RelExpAST* ast) {
  LOG("RelExpAST");
  if (ast->type == 0) {
    // RelExp
    auto& rel = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    rel->Accept(this);
  } else {
    // RelExp BinaryOp RelExp
    auto& rel = std::get<RelExpAST::RelExp>(ast->mem);
    rel.rel_exp->Accept(this);
    rel.add_exp->Accept(this);
    // 先访问左右操作数，再访问 rel_op
    rel.rel_op->Accept(this);
  }
}

void IRgenerator::Visit(AddExpAST* ast) {
  LOG("AddExpAST");
  if (ast->type == 0) {
    // MulExp
    auto& mul = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    mul->Accept(this);
  } else {
    // AddExp BinaryOp MulExp
    auto& add = std::get<AddExpAST::AddExp>(ast->mem);
    add.add_exp->Accept(this);
    // 先访问左右操作数，再访问 add_op
    add.mul_exp->Accept(this);
    add.add_op->Accept(this);
  }
}

void IRgenerator::Visit(MulExpAST* ast) {
  LOG("MulExpAST");
  if (ast->type == 0) {
    // UnaryExp
    auto& unary = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    unary->Accept(this);
  } else {
    // MulExp MulOp UnaryExp
    auto& mul = std::get<MulExpAST::MulExp>(ast->mem);
    // 先访问左右操作数，再访问 mul_op
    mul.mul_exp->Accept(this);
    mul.unary_exp->Accept(this);
    mul.mul_op->Accept(this);
  }
}

void IRgenerator::Visit(UnaryOpAST* ast) {
  LOG("UnaryOpAST");
  // ast->unary_exp->Accept(this);
  std::shared_ptr<Value> rhs = current_value_stack.top();
  current_value_stack.pop();

  // 如果右操作数是常量，直接计算结果
  if (rhs->type == Value_Type::KOOPA_RVT_INTEGER) {
    LOG("rhs is integer");
    auto r = static_cast<Value_INTEGER*>(rhs.get());
    switch (ast->op) {
      case Op_Type::AST_UNARY_OP_NEG:
        current_value_stack.push(std::make_shared<Value_INTEGER>(-(r->val)));
        break;
      case Op_Type::AST_UNARY_OP_NOT:
        current_value_stack.push(std::make_shared<Value_INTEGER>(r->val == 0));
        break;
      case Op_Type::AST_UNARY_OP_POS:
        current_value_stack.push(std::make_shared<Value_INTEGER>(r->val));
        break;
      default:
        break;
    }
    return;
  }

  // 如果右操作数不是常量，翻译为指令
  std::string name = "%" + std::to_string(count++);
  switch (ast->op) {
    case Op_Type::AST_UNARY_OP_NEG: {
      // 将val实例加入到block
      auto val = std::make_shared<Value_BINARY>(name, 
                              std::make_shared<Value_INTEGER>(0),
                              rhs,
                              Binary_Op_Type::KOOPA_RBO_SUB
                            );
      current_block->stmts.push_back(val);
      // 生成的val加入栈中
      current_value_stack.push(val);
      break;
    }
    case Op_Type::AST_UNARY_OP_NOT: {
      // 将val实例加入到block
      auto val = std::make_shared<Value_BINARY>(name, 
                              rhs,
                              std::make_shared<Value_INTEGER>(0),
                              Binary_Op_Type::KOOPA_RBO_EQ
                            );
      current_block->stmts.push_back(val);
      // 生成的val加入栈
      current_value_stack.push(val);
      break;
      }
    case Op_Type::AST_UNARY_OP_POS:
      // 正号不产生新指令，把原值放回栈
      current_value_stack.push(rhs);
      break;
    default:
      break;
  }
}

void IRgenerator::Visit(BinaryOpAST* ast) {
  LOG("BinaryOpAST");
  assert(current_value_stack.size() >= 2);
  std::shared_ptr<Value> rhs = current_value_stack.top();
  current_value_stack.pop();
  std::shared_ptr<Value> lhs = current_value_stack.top();
  current_value_stack.pop();
  std::string name = "%" + std::to_string(count++);
  Binary_Op_Type op_type;
  // 如果左操作数和右操作数都是常量，直接计算结果
  if (lhs->type == Value_Type::KOOPA_RVT_INTEGER && rhs->type == Value_Type::KOOPA_RVT_INTEGER) {
    auto l = static_cast<Value_INTEGER*>(lhs.get());
    auto r = static_cast<Value_INTEGER*>(rhs.get());
    switch (ast->op) {
      case Op_Type::AST_BINARY_OP_ADD:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val + r->val));
        break;
      case Op_Type::AST_BINARY_OP_SUB:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val - r->val));
        break;
      case Op_Type::AST_BINARY_OP_MUL:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val * r->val));
        break;
      case Op_Type::AST_BINARY_OP_DIV:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val / r->val));
        break;
      case Op_Type::AST_BINARY_OP_MOD:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val % r->val));
        break;
      case Op_Type::AST_BINARY_OP_EQ:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val == r->val));
        break;
      case Op_Type::AST_BINARY_OP_NE:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val != r->val));
        break;
      case Op_Type::AST_BINARY_OP_GT:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val > r->val));
        break;
      case Op_Type::AST_BINARY_OP_LT:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val < r->val));
        break;
      case Op_Type::AST_BINARY_OP_LE:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val <= r->val));
        break;
      case Op_Type::AST_BINARY_OP_GE:
        current_value_stack.push(std::make_shared<Value_INTEGER>(l->val >= r->val));
        break;
      default:
        // should not reach here
        assert(0);
        break;
    }
    return ;
  }
  // 如果左操作数和右操作数不是常量，翻译为指令
  switch (ast->op) {
    case Op_Type::AST_BINARY_OP_ADD:
      op_type = Binary_Op_Type::KOOPA_RBO_ADD;
        break;
    case Op_Type::AST_BINARY_OP_SUB: 
      op_type = Binary_Op_Type::KOOPA_RBO_SUB;
      break;
    case Op_Type::AST_BINARY_OP_MUL:
      op_type = Binary_Op_Type::KOOPA_RBO_MUL;
      break;
    case Op_Type::AST_BINARY_OP_DIV:
      op_type = Binary_Op_Type::KOOPA_RBO_DIV;
      break;
    case Op_Type::AST_BINARY_OP_MOD:
      op_type = Binary_Op_Type::KOOPA_RBO_MOD;
      break;
    case Op_Type::AST_BINARY_OP_EQ:
      op_type = Binary_Op_Type::KOOPA_RBO_EQ;
      break;
    case Op_Type::AST_BINARY_OP_NE:
      op_type = Binary_Op_Type::KOOPA_RBO_NOT_EQ;
      break;
    case Op_Type::AST_BINARY_OP_GT:
      op_type = Binary_Op_Type::KOOPA_RBO_GT;
      break;
    case Op_Type::AST_BINARY_OP_LT:
      op_type = Binary_Op_Type::KOOPA_RBO_LT;
      break;
    case Op_Type::AST_BINARY_OP_LE:
      op_type = Binary_Op_Type::KOOPA_RBO_LE;
      break;
    case Op_Type::AST_BINARY_OP_GE:
      op_type = Binary_Op_Type::KOOPA_RBO_GE;
      break;
    default:
      break; 
  }
  // 将val实例加入到block
  auto val = std::make_shared<Value_BINARY>(name, lhs, 
                              rhs, op_type);
  current_block->stmts.push_back(val);

  // 生成的val加入栈
  current_value_stack.push(val);
}

void IRgenerator::Visit(LGBinaryOpAST* ast) {
  LOG("LGBinaryOpAST");
  // 已经在上一级节点实现, NEVER REACH HERE
}

void IRgenerator::Visit(BlockItemAST* ast) {
  LOG("BlockItemAST");
  if (ast->type == 0) {
    // Stmt
    std::get<std::unique_ptr<BaseAST>>(ast->mem)->Accept(this);
  } else {
    // Exp
    std::get<std::unique_ptr<BaseAST>>(ast->mem)->Accept(this); 
  }
}

void IRgenerator::Visit(ConstDeclAST* ast) {
  LOG("ConstDeclAST");
  for (auto& item : *(ast->const_def_list)) {
    auto const_def = static_cast<ConstDefAST*>(item.get());
    const_def->const_exp->Accept(this);
    symbol_table->Insert(const_def->ident, current_value_stack.top(), true, ast->elem_type);
    current_value_stack.pop();
  }
}

void IRgenerator::Visit(ConstDefAST* ast) {
  LOG("ConstDefAST");
  // 上一层直接处理了，所以应该不会到达这里
}

void IRgenerator::Visit(LValAST* ast) {
  LOG("LValAST");
  auto val = symbol_table->FindValue(ast->ident);
  if (!symbol_table->FindSymbol(ast->ident).is_const) {
    /// 载入变量，生成load指令
    auto load_val = std::make_shared<Value_LOAD>("%" + std::to_string(count++), val);
    current_block->stmts.push_back(load_val);
    /// 压入load的结果
    current_value_stack.push(load_val);
  } else {
    /// 常量直接压入值
    current_value_stack.push(val);
  }
}



void IRgenerator::Visit(OpAST* ast) {
  LOG("OpAST");
  // NEVER REACH HERE
}

void IRgenerator::Visit(VarDefAST* ast) {
  LOG("VarDefAST");
  // 上一层直接处理了，所以应该不会到达这里
}

void IRgenerator::Visit(VarDeclAST* ast) {
  LOG("VarDeclAST");
  // VarDef
  for (auto& item : *(ast->var_def_list)) {
    auto var_def = static_cast<VarDefAST*>(item.get());
    // 生成alloc指令
    auto alloc = std::make_shared<Value_ALLOC>("@" + var_def->ident, ast->elem_type);
    current_block->stmts.push_back(alloc);
    if (var_def->type == 1) {
      // 初始化
      var_def->var_exp->Accept(this);
      // 为变量赋值，生成store指令
      auto store = std::make_shared<Value_STORE>(current_value_stack.top(), alloc);
      current_block->stmts.push_back(store);
      current_value_stack.pop();
      // 在符号表中插入变量
    }
    symbol_table->Insert(var_def->ident, alloc, false, ast->elem_type);
  }

}

void IRgenerator::Visit(NullAST* ast) {
  // LOG("NullAST");
}

void IRgenerator::OutputIR(std::string output) {
  IROutputer out(output, std::move(symbol_table));
  program->Accept(&out);
}

IROutputer::IROutputer() {}

IROutputer::IROutputer(std::string output, std::unique_ptr<SymbolTable> symbol_table) : fs(output, std::ios::out), symbol_table(std::move(symbol_table)) {}

IROutputer::~IROutputer() {}

void IROutputer::Visit(Value_INTEGER* integer) {
    fs << integer->val;
}

void IROutputer::Visit(Value_RETURN* return_val) {
    fs << "  ret " << return_val->val->name;
    // return_val->val->Accept(this); 
    fs << std::endl;
}

void IROutputer::Visit(Value_BINARY* binary) {
  switch (binary->type) {
    case Binary_Op_Type::KOOPA_RBO_EQ:
      fs << "  " << binary->name << " = eq ";
      break;
    case Binary_Op_Type::KOOPA_RBO_NOT_EQ:
      fs << "  " << binary->name << " = ne ";
      break;
    case Binary_Op_Type::KOOPA_RBO_SUB:
      fs << "  " << binary->name << " = sub ";
      break;
    case Binary_Op_Type::KOOPA_RBO_MUL:
      fs << "  " << binary->name << " = mul ";
      break;
    case Binary_Op_Type::KOOPA_RBO_DIV:
      fs << "  " << binary->name << " = div ";
      break;
    case Binary_Op_Type::KOOPA_RBO_ADD:
      fs << "  " << binary->name << " = add ";
      break;
    case Binary_Op_Type::KOOPA_RBO_AND:
      fs << "  " << binary->name << " = and ";
      break;
    case Binary_Op_Type::KOOPA_RBO_MOD:
      fs << "  " << binary->name << " = mod ";
      break;
    case Binary_Op_Type::KOOPA_RBO_OR:
      fs << "  " << binary->name << " = or ";
      break;
    case Binary_Op_Type::KOOPA_RBO_GE:
      fs << "  " << binary->name << " = ge ";
      break;
    case Binary_Op_Type::KOOPA_RBO_GT:
      fs << "  " << binary->name << " = gt ";
      break;
    case Binary_Op_Type::KOOPA_RBO_LE:
      fs << "  " << binary->name << " = le ";
      break;
      case Binary_Op_Type::KOOPA_RBO_LT:
      fs << "  " << binary->name << " = lt ";
      break;
    default:
      break;
  }
  fs << binary->lhs->name << ", ";
  fs << binary->rhs->name << std::endl;
}

void IROutputer::Visit(Value_REF* ref) {
  fs << ref->name;
}

void IROutputer::Visit(BasicBlock* block) {
  fs << "%" +block->name << ": " << std::endl;
  for (auto& stmt : block->stmts) {
    stmt->Accept(this);
    if (stmt->type == Value_Type::KOOPA_RVT_RETURN) {
      break;
    }
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

void IROutputer::Visit(Value_ALLOC* alloc) {
  fs << "  " << symbol_table->GetName(alloc) << " = alloc " << alloc->elem_type << std::endl;
}

void IROutputer::Visit(Value_LOAD* load) {
  fs << "  " << load->name << " = load " << symbol_table->GetName(load->src.get()) << std::endl;
}

void IROutputer::Visit(Value_STORE* store) {
  std::string value_name;
  if (store->value->type == Value_Type::KOOPA_RVT_ALLOC) {
    value_name = symbol_table->GetName(store->value.get());
  } else {
    value_name = store->value->name;  // 对于INTEGER等直接使用name
  }
  fs << "  store " << value_name << ", " << symbol_table->GetName(store->dst.get()) << std::endl;
}

void IROutputer::Visit(Value_JUMP* jump) {
  fs << "  jump " << "%" + jump->dst_name << std::endl;
}

void IROutputer::Visit(Value_BRANCH* branch) {
  std::string cond_name;
  if (branch->cond->type == Value_Type::KOOPA_RVT_ALLOC) {
    cond_name = symbol_table->GetName(branch->cond.get());
  } else {
    cond_name = branch->cond->name;  // 对于INTEGER等直接使用name
  }
  fs << "  br " << cond_name << ", " << "%" + branch->then_name << ", " << "%" + branch->else_name << std::endl;
}
