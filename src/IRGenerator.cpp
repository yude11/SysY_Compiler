#include "IRGenerator.h"
#include "IR.h"
#include "AST.h"
#include "type.h"
#include "log.h"

// 库函数列表
std::vector<std::vector<std::string>> builtin_functions = {
    {"getint", "i32"},
    {"getch", "i32"},
    {"getarray", "i32", "*i32"},
    {"putint", "void", "i32"},
    {"putch", "void", "i32"},
    {"putarray", "void", "i32", "*i32"},
    {"starttime", "void"},
    {"stoptime", "void"}
};

IRgenerator::IRgenerator() : program(std::make_unique<Program>()), symbol_table(std::make_unique<SymbolTable>()), name_manager(std::make_unique<IRNameManager>()) {}

IRgenerator::~IRgenerator() {}

void IRgenerator::Visit(CompUnitAST* ast) {
  LOG("CompUnitAST");
  // 添加库函数声明
  for (auto& func : builtin_functions) {
    auto func_decl = std::make_unique<Function_Decl>();
    func_decl->name = func[0];
    func_decl->type.name = func[1];
    for (size_t i = 2; i < func.size(); i++) {
      func_decl->params_type.push_back(func[i]);
    }
    // 将库函数加入符号表
    symbol_table->Insert(func_decl->name, nullptr, true, false, func_decl->type.name);
    program->functions.push_back(std::move(func_decl));
  }

  // 遍历所有函数定义和全局变量声明
  for (auto& func_def_or_decl : ast->func_defs_or_decls) {
    if (dynamic_cast<FuncDefAST*>(func_def_or_decl.get())) {
      current_func = std::make_unique<Function>();
      auto def = static_cast<FuncDefAST*>(func_def_or_decl.get());
      auto func_type = static_cast<FuncTypeAST*>(def->func_type.get());
      // 将函数加入到符号表中
      symbol_table->Insert(def->ident, nullptr, true, false, func_type->type);
      // 处理函数
      func_def_or_decl->Accept(this);
      program->functions.push_back(std::move(current_func));
    } else if (dynamic_cast<VarDeclAST*>(func_def_or_decl.get())) {
      // 处理全局变量声明
      GlobalVarComputer global_var_computer(symbol_table.get());
      func_def_or_decl->Accept(&global_var_computer);
      for (auto& [ident, info] : global_var_computer.global_var_infos) {
        auto init_value = std::make_shared<Value_INTEGER>(info.init_val);
        auto global_alloc = std::make_shared<Value_GLOBOL_ALLOC>(
          name_manager->AllocateNamed(ident), init_value);
        program->values.push_back(global_alloc);
        symbol_table->Insert(ident, global_alloc, false, info.is_const, info.type);
      }
    } else if (dynamic_cast<ConstDeclAST*>(func_def_or_decl.get())) {
      // 处理全局常量声明（常量不需要分配内存，直接存储值）
      GlobalVarComputer global_var_computer(symbol_table.get());
      func_def_or_decl->Accept(&global_var_computer);
      for (auto& [ident, info] : global_var_computer.global_var_infos) {
        auto const_value = std::make_shared<Value_INTEGER>(info.init_val);
        symbol_table->Insert(ident, const_value, false, true, info.type);
      }
    }
  }
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
  auto block = static_cast<BlockAST*>(ast->block.get());
  symbol_table->Push();
  if (ast->func_params) {
    for (auto& param : *ast->func_params) {
      // 将函数参数加入符号表
      auto param_ref = std::make_shared<Value_FUNC_ARG_REF>(name_manager->AllocateNamed(param->ident), param->type);
      auto val = std::make_shared<Value_ALLOC>(name_manager->AllocateTempHint(param->ident), param->type);
      current_block->stmts.push_back(val);
      current_block->stmts.push_back(std::make_shared<Value_STORE>(param_ref, val));
      symbol_table->Insert(param->ident, val, false, false, param->type);
      // 将参数加入到current_func的参数列表中
      current_func->params.push_back(param_ref);
    }
  }
  for (auto& item : *(block->block_items)) {
    item->Accept(this);
    if (current_block->IsTerminated()) {
      break;
    }
  }
  if (!current_block->IsTerminated()) {
    // 如果函数没有以return结尾，添加一个默认的return 0
    if (current_func->type.name == "void") {
      current_block->stmts.push_back(std::make_shared<Value_RETURN>(nullptr));
    } else {
      std::cerr << "warning: Non-void function does not end with return, adding default return 0" << std::endl;
      current_block->stmts.push_back(std::make_shared<Value_RETURN>(std::make_shared<Value_INTEGER>(0)));
    }
  }
  // 当current_block构造完成后，将其加入到current_func中
  current_func->blocks.push_back(std::move(current_block));
}

void IRgenerator::Visit(BlockAST* ast) {
  LOG("BlockAST");
  symbol_table->Push();
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
      if (symbol->is_const) {
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
      std::string then_name = name_manager->AllocateLabel("then");
      std::string end_name = name_manager->AllocateLabel("end");
      std::string else_name = if_else_stmt.else_stmt != nullptr 
                              ? name_manager->AllocateLabel("else")
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
      while_entry_stack.push(name_manager->GetLabelCount("while_entry"));
      std::string while_entry = name_manager->AllocateLabel("while_entry");
      std::string then_name = name_manager->AllocateLabel("while_body");
      std::string end_name = name_manager->AllocateLabel("while_end");
      
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
      std::string end_name = "while_end" + (while_entry_stack.top() ? "_"+std::to_string(while_entry_stack.top()) : "");
      current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
      // 标记当前块已终止
      current_block->Terminate();
      break;
    }
    case Stmt_Type::AST_STMT_CONTINUE: {
      std::string while_entry_name = "while_entry" + (while_entry_stack.top() ? "_"+std::to_string(while_entry_stack.top()) : "");
      current_block->stmts.push_back(std::make_shared<Value_JUMP>(while_entry_name));
      // 标记当前块已终止
      current_block->Terminate();
      break;
    }
    case Stmt_Type::AST_STMT_EXP: {
      auto& exp_stmt = std::get<StmtAST::Exp_STMT>(ast->stmt);
      exp_stmt->Accept(this);
      // 表达式结果不需要使用，从栈中弹出扔掉
      if (!current_value_stack.empty()) {
        current_value_stack.pop();
      }
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

    // Step 1: 只计算 lhs
    l_or.l_or_exp->Accept(this);
    auto lhs_val = current_value_stack.top();
    current_value_stack.pop();

    // 如果 lhs 是非零常量，直接返回 1（短路）
    if (lhs_val->type == Value_Type::KOOPA_RVT_INTEGER) {
      auto l = static_cast<Value_INTEGER*>(lhs_val.get());
      if (l->val != 0) {
        current_value_stack.push(std::make_shared<Value_INTEGER>(1));
        return;
      }
      // lhs 是 0，需要计算 rhs
      l_or.l_and_exp->Accept(this);
      auto rhs_val = current_value_stack.top();
      current_value_stack.pop();
      if (rhs_val->type == Value_Type::KOOPA_RVT_INTEGER) {
        auto r = static_cast<Value_INTEGER*>(rhs_val.get());
        current_value_stack.push(std::make_shared<Value_INTEGER>(r->val != 0 ? 1 : 0));
      } else {
        auto neq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
        current_block->stmts.push_back(neq);
        current_value_stack.push(neq);
      }
      return;
    }
    
    // Step 2: 分配临时变量，默认值为 1 (true)
    auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateTempHint("result"), "i32");
    symbol_table->Insert(alloc->name, alloc, false, false, "i32");
    current_block->stmts.push_back(alloc);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(std::make_shared<Value_INTEGER>(1), alloc));
    
    // Step 3: 生成分支指令
    auto then_name = name_manager->AllocateLabel("then");
    auto end_name = name_manager->AllocateLabel("end");
    auto eq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), lhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_EQ);
    current_block->stmts.push_back(eq);
    current_block->stmts.push_back(std::make_shared<Value_BRANCH>(eq, then_name, end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 4: 生成 then 基本块 (lhs 为 false 时才计算 rhs)
    current_block = std::make_unique<BasicBlock>(then_name);
    l_or.l_and_exp->Accept(this);  // 在 then 块中计算 rhs
    auto rhs_val = current_value_stack.top();
    current_value_stack.pop();
    auto neq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(neq);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(neq, alloc));
    current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 5: 生成 end 基本块
    current_block = std::make_unique<BasicBlock>(end_name);
    auto load = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), alloc);
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

    // Step 1: 只计算 lhs
    l_and.l_and_exp->Accept(this);
    auto lhs_val = current_value_stack.top();
    current_value_stack.pop();

    // 如果 lhs 是常量 0，直接返回 0（短路）
    if (lhs_val->type == Value_Type::KOOPA_RVT_INTEGER) {
      auto l = static_cast<Value_INTEGER*>(lhs_val.get());
      if (l->val == 0) {
        current_value_stack.push(std::make_shared<Value_INTEGER>(0));
        return;
      }
      // lhs 是非零常量，需要计算 rhs
      l_and.eq_exp->Accept(this);
      auto rhs_val = current_value_stack.top();
      current_value_stack.pop();
      if (rhs_val->type == Value_Type::KOOPA_RVT_INTEGER) {
        auto r = static_cast<Value_INTEGER*>(rhs_val.get());
        current_value_stack.push(std::make_shared<Value_INTEGER>(r->val != 0 ? 1 : 0));
      } else {
        auto neq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
        current_block->stmts.push_back(neq);
        current_value_stack.push(neq);
      }
      return;
    }

    // Step 2: 分配临时变量，默认值为 0 (false)
    auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateTempHint("result"), "i32");
    symbol_table->Insert(alloc->name, alloc, false, false, "i32");
    current_block->stmts.push_back(alloc);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(std::make_shared<Value_INTEGER>(0), alloc));
    
    // Step 3: 生成分支指令
    auto then_name = name_manager->AllocateLabel("then");
    auto end_name = name_manager->AllocateLabel("end");
    auto ne = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), lhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(ne);
    current_block->stmts.push_back(std::make_shared<Value_BRANCH>(ne, then_name, end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 4: 生成 then 基本块 (lhs 为 true 时才计算 rhs)
    current_block = std::make_unique<BasicBlock>(then_name);
    l_and.eq_exp->Accept(this);  // 在 then 块中计算 rhs
    auto rhs_val = current_value_stack.top();
    current_value_stack.pop();
    auto neq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(neq);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(neq, alloc));
    current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // Step 5: 生成 end 基本块
    current_block = std::make_unique<BasicBlock>(end_name);
    auto load = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), alloc);
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
  std::string name = name_manager->AllocateTemp();
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
  std::string name = name_manager->AllocateTemp();
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
  assert(0);
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
    symbol_table->Insert(const_def->ident, current_value_stack.top(), false, true, ast->elem_type);
    current_value_stack.pop();
  }
}

void IRgenerator::Visit(ConstDefAST* ast) {
  LOG("ConstDefAST");
  assert(0);
  // 上一层直接处理了，所以应该不会到达这里
}

void IRgenerator::Visit(LValAST* ast) {
  LOG("LValAST");
  auto val = symbol_table->FindValue(ast->ident);
  if (!symbol_table->FindSymbol(ast->ident)->is_const) {
    /// 载入变量，生成load指令
    auto load_val = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), val);
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
    auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateNamed(var_def->ident), ast->elem_type);
    current_block->stmts.push_back(alloc);
    if (var_def->type == 1) {
      // 初始化
      var_def->var_exp->Accept(this);
      // 为变量赋值，生成store指令
      auto store = std::make_shared<Value_STORE>(current_value_stack.top(), alloc);
      current_block->stmts.push_back(store);
      current_value_stack.pop();
    }
    // 在符号表中插入变量
    symbol_table->Insert(var_def->ident, alloc, false, false, ast->elem_type);
  }
}

void IRgenerator::Visit(NullAST* ast) {
  // LOG("NullAST");
  // 不生成任何东西，空操作
}

void IRgenerator::Visit(FuncCallAST* ast) {
  LOG("FuncCallAST");
  std::vector<std::shared_ptr<Value>> args;
  // 1. 解析出所有参数加入到args中
  if (ast->params) {
    for (auto& param : *ast->params) {
      // 访问参数表达式，计算参数值并压栈
      param->Accept(this);
      auto param_val = current_value_stack.top();
      current_value_stack.pop();
      args.push_back(param_val);
    }
  }
  // 2. 生成call指令
  // 查看函数的返回值类型，如果是void则不需要处理返回值
  auto func_symbol = symbol_table->FindSymbol(ast->ident);
  if (func_symbol->type != "void") {
    // 有返回值：分配一个临时符号作为返回值名字
    auto ret_name = name_manager->AllocateTemp();
    auto call_val = std::make_shared<Value_Call>(ret_name, "@" + ast->ident, args);
    current_value_stack.push(call_val);  // 把 call 指令本身压栈
    current_block->stmts.push_back(call_val);
  } else {
    // void 函数：name 为空
    LOG("void function call");
    auto call_val = std::make_shared<Value_Call>("null", "@" + ast->ident, args);
    current_block->stmts.push_back(call_val);
  }
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
    if (return_val->val == nullptr) {
        fs << "  ret" << std::endl;
        return;
    }
    fs << "  ret " << return_val->val->name << std::endl;
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

void IROutputer::Visit(Value_FUNC_ARG_REF* ref) {
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
  fs << std::endl;
  fs << "fun @" << func->name << "(";
  for (size_t i = 0; i < func->params.size(); ++i) {
    auto param = static_cast<Value_FUNC_ARG_REF*>(func->params[i].get());
    fs << param->name << ": " << param->elem_type;
    if (i != func->params.size() - 1) {
      fs << ", ";
    }
  }
  fs << ")";
  if (func->type.name != "void") {
    fs << ": " << func->type.name;
  }
  fs << " {" << std::endl;
  for (auto& block : func->blocks) {
    block->Accept(this);
  }
  fs << "}" << std::endl;
}

void IROutputer::Visit(Function_Decl *func_decl) {
  fs << "decl @" << func_decl->name << "(";
  for (size_t i = 0; i < func_decl->params_type.size(); ++i) {
    fs << func_decl->params_type[i];
    if (i != func_decl->params_type.size() - 1) {
      fs << ", ";
    }
  }
  fs << ")";
  if (func_decl->type.name != "void") {
    fs << ": " << func_decl->type.name;
  }
  fs << std::endl;
}

void IROutputer::Visit(Program* program) {
  for (auto& value : program->values) {
    value->Accept(this);
  }
  for (auto& func : program->functions) {
    func->Accept(this);
  }
}

void IROutputer::Visit(Value_ALLOC* alloc) {
  fs << "  " << alloc->name << " = alloc " << alloc->elem_type << std::endl;
}

void IROutputer::Visit(Value_LOAD* load) {
  fs << "  " << load->name << " = load " << load->src->name << std::endl;
}

void IROutputer::Visit(Value_STORE* store) {
  std::string value_name;
  value_name = store->value->name;
  fs << "  store " << value_name << ", " << store->dst->name << std::endl;
}

void IROutputer::Visit(Value_JUMP* jump) {
  fs << "  jump " << "%" + jump->dst_name << std::endl;
}

void IROutputer::Visit(Value_BRANCH* branch) {
  std::string cond_name;
  if (branch->cond->type == Value_Type::KOOPA_RVT_ALLOC) {
    cond_name = branch->cond->name;
  } else {
    cond_name = branch->cond->name;  // 对于INTEGER等直接使用name
  }
  fs << "  br " << cond_name << ", " << "%" + branch->then_name << ", " << "%" + branch->else_name << std::endl;
}

void IROutputer::Visit(Value_Call* call) {
  if (call->name != "null") {
    fs << "  " << call->name << " =";
  } else {
    fs << " ";
  }
  fs << " call " << call->ident << "(";
  for (size_t i = 0; i < call->args.size(); ++i) {
    auto arg = call->args[i];
    fs << arg->name;
    if (i != call->args.size() - 1) {
      fs << ", ";
    }
  }
  fs << ")" << std::endl;
}

void IROutputer::Visit(Value_GLOBOL_ALLOC* glob_alloc) {
  fs << "global " << glob_alloc->name << " = alloc i32, ";
  if (glob_alloc->init) {
    glob_alloc->init->Accept(this);
  } else {
    fs << "0";
  }
  fs << std::endl;
}

GlobalVarComputer::GlobalVarComputer(SymbolTable* symbol_table) 
  : val_stack(std::make_unique<std::stack<int>>()), symbol_table(symbol_table) {}

GlobalVarComputer::~GlobalVarComputer() {}

void GlobalVarComputer::Visit(VarDeclAST* ast) {
  for (auto& item : *(ast->var_def_list)) {
    auto var_def = static_cast<VarDefAST*>(item.get());
    int init_val = 0;
    if (var_def->type == 1 && var_def->var_exp) {
      var_def->var_exp->Accept(this);
      init_val = val_stack->top();
      val_stack->pop();
    }
    global_var_infos[var_def->ident] = {init_val, ast->elem_type, false};
  }
}

void GlobalVarComputer::Visit(ConstDeclAST* ast) {
  for (auto& item : *(ast->const_def_list)) {
    auto const_def = static_cast<ConstDefAST*>(item.get());
    const_def->const_exp->Accept(this);
    int val = val_stack->top();
    val_stack->pop();
    global_var_infos[const_def->ident] = {val, ast->elem_type, true};
  }
}

void GlobalVarComputer::Visit(ConstDefAST* ast) {
  ast->const_exp->Accept(this);
}

void GlobalVarComputer::Visit(VarDefAST* ast) {
  if (ast->type == 1 && ast->var_exp) {
    ast->var_exp->Accept(this);
  } else {
    val_stack->push(0);
  }
}

void GlobalVarComputer::Visit(LValAST* ast) {
  auto symbol = symbol_table->FindSymbol(ast->ident);
  if (symbol && symbol->is_const) {
    auto val = symbol->value;
    if (val && val->type == Value_Type::KOOPA_RVT_INTEGER) {
      val_stack->push(static_cast<Value_INTEGER*>(val.get())->val);
      return;
    }
  }
  val_stack->push(0);
}

void GlobalVarComputer::Visit(NumberAST* ast) {
  val_stack->push(ast->num);
}

void GlobalVarComputer::Visit(ExpAST* ast) {
  ast->l_or_exp->Accept(this);
}

void GlobalVarComputer::Visit(PrimaryExpAST* ast) {
  ast->mem->Accept(this);
}

void GlobalVarComputer::Visit(UnaryExpAST* ast) {
  if (ast->type == 0) {
    auto& primary = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    primary->Accept(this);
  } else {
    auto& unary = std::get<UnaryExpAST::UnaryExp>(ast->mem);
    unary.unary_exp->Accept(this);
    unary.unary_op->Accept(this);
  }
}

void GlobalVarComputer::Visit(UnaryOpAST* ast) {
  int rhs = val_stack->top();
  val_stack->pop();
  switch (ast->op) {
    case Op_Type::AST_UNARY_OP_NEG:
      val_stack->push(-rhs);
      break;
    case Op_Type::AST_UNARY_OP_NOT:
      val_stack->push(rhs == 0 ? 1 : 0);
      break;
    case Op_Type::AST_UNARY_OP_POS:
      val_stack->push(rhs);
      break;
    default:
      break;
  }
}

void GlobalVarComputer::Visit(AddExpAST* ast) {
  if (ast->type == 0) {
    auto& mul = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    mul->Accept(this);
  } else {
    auto& add = std::get<AddExpAST::AddExp>(ast->mem);
    add.add_exp->Accept(this);
    add.mul_exp->Accept(this);
    add.add_op->Accept(this);
  }
}

void GlobalVarComputer::Visit(MulExpAST* ast) {
  if (ast->type == 0) {
    auto& unary = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    unary->Accept(this);
  } else {
    auto& mul = std::get<MulExpAST::MulExp>(ast->mem);
    mul.mul_exp->Accept(this);
    mul.unary_exp->Accept(this);
    mul.mul_op->Accept(this);
  }
}

void GlobalVarComputer::Visit(RelExpAST* ast) {
  if (ast->type == 0) {
    auto& rel = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    rel->Accept(this);
  } else {
    auto& rel = std::get<RelExpAST::RelExp>(ast->mem);
    rel.rel_exp->Accept(this);
    rel.add_exp->Accept(this);
    rel.rel_op->Accept(this);
  }
}

void GlobalVarComputer::Visit(EqExpAST* ast) {
  if (ast->type == 0) {
    auto& eq = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    eq->Accept(this);
  } else {
    auto& eq = std::get<EqExpAST::EqExp>(ast->mem);
    eq.eq_exp->Accept(this);
    eq.rel_exp->Accept(this);
    eq.eq_op->Accept(this);
  }
}

void GlobalVarComputer::Visit(LOrExpAST* ast) {
  if (ast->type == 0) {
    auto& l_or = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    l_or->Accept(this);
  } else {
    auto& l_or = std::get<LOrExpAST::LOrExp>(ast->mem);
    l_or.l_or_exp->Accept(this);
    l_or.l_and_exp->Accept(this);
    int rhs = val_stack->top();
    val_stack->pop();
    int lhs = val_stack->top();
    val_stack->pop();
    val_stack->push(lhs || rhs ? 1 : 0);
  }
}

void GlobalVarComputer::Visit(LAndExpAST* ast) {
  if (ast->type == 0) {
    auto& l_and = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    l_and->Accept(this);
  } else {
    auto& l_and = std::get<LAndExpAST::LAndExp>(ast->mem);
    l_and.l_and_exp->Accept(this);
    l_and.eq_exp->Accept(this);
    int rhs = val_stack->top();
    val_stack->pop();
    int lhs = val_stack->top();
    val_stack->pop();
    val_stack->push(lhs && rhs ? 1 : 0);
  }
}

void GlobalVarComputer::Visit(BinaryOpAST* ast) {
  int rhs = val_stack->top();
  val_stack->pop();
  int lhs = val_stack->top();
  val_stack->pop();
  int result = 0;
  switch (ast->op) {
    case Op_Type::AST_BINARY_OP_ADD:
      result = lhs + rhs;
      break;
    case Op_Type::AST_BINARY_OP_SUB:
      result = lhs - rhs;
      break;
    case Op_Type::AST_BINARY_OP_MUL:
      result = lhs * rhs;
      break;
    case Op_Type::AST_BINARY_OP_DIV:
      result = lhs / rhs;
      break;
    case Op_Type::AST_BINARY_OP_MOD:
      result = lhs % rhs;
      break;
    case Op_Type::AST_BINARY_OP_EQ:
      result = (lhs == rhs) ? 1 : 0;
      break;
    case Op_Type::AST_BINARY_OP_NE:
      result = (lhs != rhs) ? 1 : 0;
      break;
    case Op_Type::AST_BINARY_OP_LT:
      result = (lhs < rhs) ? 1 : 0;
      break;
    case Op_Type::AST_BINARY_OP_LE:
      result = (lhs <= rhs) ? 1 : 0;
      break;
    case Op_Type::AST_BINARY_OP_GT:
      result = (lhs > rhs) ? 1 : 0;
      break;
    case Op_Type::AST_BINARY_OP_GE:
      result = (lhs >= rhs) ? 1 : 0;
      break;
    default:
      break;
  }
  val_stack->push(result);
}

void GlobalVarComputer::Visit(LGBinaryOpAST* ast) {
  int rhs = val_stack->top();
  val_stack->pop();
  int lhs = val_stack->top();
  val_stack->pop();
  int result = 0;
  switch (ast->op) {
    case Op_Type::AST_BINARY_OP_LA:
      result = lhs && rhs ? 1 : 0;
      break;
    case Op_Type::AST_BINARY_OP_LO:
      result = lhs || rhs ? 1 : 0;
      break;
    default:
      break;
  }
  val_stack->push(result);
}

void GlobalVarComputer::Visit(StmtAST* ast) {}

void GlobalVarComputer::Visit(BlockItemAST* ast) {}

void GlobalVarComputer::Visit(NullAST* ast) {}

void GlobalVarComputer::Visit(FuncCallAST* ast) {}

void GlobalVarComputer::Visit(OpAST* ast) {}

void GlobalVarComputer::Visit(CompUnitAST* ast) {}

void GlobalVarComputer::Visit(FuncTypeAST* ast) {}

void GlobalVarComputer::Visit(FuncDefAST* ast) {}

void GlobalVarComputer::Visit(BlockAST* ast) {}