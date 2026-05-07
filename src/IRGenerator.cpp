#include "IRGenerator.h"
#include "IR.h"
#include "AST.h"
#include "type.h"
#include "log.h"

#include <algorithm>
#include <functional>

// 库函数列表
// FunctionSymbol(name, value, return_type, param_types)
std::vector<FunctionSymbol> builtin_functions = {
    // int getint()
    {"getint", nullptr, TypeInfo(BaseType::INT, {}), {}},
    // int getch()
    {"getch", nullptr, TypeInfo(BaseType::INT, {}), {}},
    // int getarray(int *arr)
    {"getarray", nullptr, TypeInfo(BaseType::INT, {}), 
     {TypeInfo(BaseType::INT, {{TypeInfo::TypeKind::POINTER, 0}})}},
    // void putint(int x)
    {"putint", nullptr, TypeInfo(BaseType::VOID, {}), 
     {TypeInfo(BaseType::INT, {})}},
    // void putch(int x)
    {"putch", nullptr, TypeInfo(BaseType::VOID, {}), 
     {TypeInfo(BaseType::INT, {})}},
    // void putarray(int n, int *arr)
    {"putarray", nullptr, TypeInfo(BaseType::VOID, {}), 
     {TypeInfo(BaseType::INT, {}), TypeInfo(BaseType::INT, {{TypeInfo::TypeKind::POINTER, 0}})}},
    // void starttime()
    {"starttime", nullptr, TypeInfo(BaseType::VOID, {}), {}},
    // void stoptime()
    {"stoptime", nullptr, TypeInfo(BaseType::VOID, {}), {}}
};

IRgenerator::IRgenerator() : program(std::make_unique<Program>()), symbol_table(std::make_unique<SymbolTable>()), name_manager(std::make_unique<IRNameManager>()) {}

IRgenerator::~IRgenerator() {}

TypeInfo IRgenerator::TypeInfoModelToTypeInfo(TypeInfoModel* type_model) {
  std::vector<std::pair<TypeInfo::TypeKind, int>> modifiers;
  
  // SCALAR: 没有修饰符
  if (type_model->kind == TypeInfo::TypeKind::SCALAR) {
    return TypeInfo(type_model->type, modifiers);
  }
  
  // POINTER: 添加指针修饰符
  if (type_model->kind == TypeInfo::TypeKind::POINTER) {
    modifiers.push_back({TypeInfo::TypeKind::POINTER, 0});
  }
  
  // ARRAY 或 POINTER（指向数组）: 添加数组维度
  // 计算每个维度的大小
  if (type_model->dims) {
    for (auto& dim_ast : *type_model->dims) {
      // dims 中的元素是表达式 AST，需要计算其值
      // 这里假设是常量表达式，可以用 ExpComputer 计算
      ExpComputer computer(symbol_table.get());
      int dim_size = computer.Evaluate(dim_ast.get());
      modifiers.push_back({TypeInfo::TypeKind::ARRAY, dim_size});
    }
  }
  
  return TypeInfo(type_model->type, modifiers);
}

template<typename T>
void IRgenerator::ParseArrayInitVals(T* ast, std::vector<std::shared_ptr<Value>>& init_vals, const std::vector<int>& dims, int level) {
  ExpComputer computer(symbol_table.get());
  
  // 叶子节点：直接计算表达式值（从最后一维开始填充）
  if (ast->is_leaf) {
    auto val = std::make_shared<Value_INTEGER>(computer.Evaluate(ast->exp.get()));
    init_vals.push_back(val);
    return;
  }
  
  // 非叶子节点：处理列表中的每个元素
  // 计算最后一维的长度
  int lastDimSize = dims.back();
  
  // 计算当前层需要处理的总元素个数
  int totalSize = 1;
  for (int i = level; i < dims.size(); i++) {
    totalSize *= dims[i];
  }
  
  // 记录当前层起始位置
  int levelStartIdx = init_vals.size();
  
  for (auto& child : ast->children) {
    if (child->is_leaf) {
      // 叶子节点：直接添加值
      auto val = std::make_shared<Value_INTEGER>(computer.Evaluate(child->exp.get()));
      init_vals.push_back(val);
    } else {
      // 非叶子节点：初始化列表
      // 检查当前填充的元素个数是否是最后一维长度的整数倍
      int currentOffset = init_vals.size() - levelStartIdx;
      if (currentOffset % lastDimSize != 0) {
        // 未对齐到边界，这是语义错误
        std::cerr << "Error: initialization list not aligned to dimension boundary" << std::endl;
        assert(0);
      }
      
      // 计算当前对齐到了哪一个边界
      // 找到能整除 currentOffset / lastDimSize 的最大维度
      int alignedSize = currentOffset / lastDimSize;
      int targetLevel = dims.size() - 1;  // 默认最后一维
      int product = 1;
      for (int i = dims.size() - 2; i >= level; i--) {
        product *= dims[i + 1];
        if (alignedSize % product == 0) {
          targetLevel = i;
        } else {
          break;
        }
      }
      
      // 递归处理子列表，从 targetLevel 开始
      ParseArrayInitVals(child.get(), init_vals, dims, targetLevel);
    }
  }
  
  // 处理完所有子节点后，补零填满当前层
  int currentSize = init_vals.size() - levelStartIdx;
  while (currentSize < totalSize) {
    init_vals.push_back(std::make_shared<Value_INTEGER>(0));
    currentSize++;
  }
}

void IRgenerator::Visit(CompUnitAST* ast) {
  LOG("CompUnitAST");
  // 添加库函数声明
  for (auto& func : builtin_functions) {
    auto func_decl = std::make_unique<Function_Decl>(
        func.return_type.base, func.return_type.modifiers, func.name);
    func_decl->params_type = func.param_types;
    // 将库函数加入符号表
    symbol_table->Insert(func.name, nullptr, func.return_type, func.param_types);
    program->functions.push_back(std::move(func_decl));
  }

  // 遍历所有函数定义和全局变量声明
  for (auto& func_def_or_decl : ast->func_defs_or_decls) {
    if (dynamic_cast<FuncDefAST*>(func_def_or_decl.get())) {
      auto def = static_cast<FuncDefAST*>(func_def_or_decl.get());
      // 构建函数类型
      TypeInfo func_type = TypeInfoModelToTypeInfo(def->func_type.get());
      current_func = std::make_unique<Function>(func_type.base, func_type.modifiers, def->ident);
      std::vector<TypeInfo> params_type;
      for (auto& param : *(def->func_params)) {
        params_type.push_back(TypeInfoModelToTypeInfo(param->type.get()));
      }
      // 将函数加入到符号表中
      symbol_table->Insert(def->ident, nullptr, 
                           TypeInfoModelToTypeInfo(def->func_type.get()), 
                           params_type);
      // 处理函数
      func_def_or_decl->Accept(this);
      program->functions.push_back(std::move(current_func));
    } else if (dynamic_cast<VarDeclAST*>(func_def_or_decl.get())) {
      auto decl = static_cast<VarDeclAST*>(func_def_or_decl.get());
      // 处理全局变量声明
      ExpComputer global_var_computer(symbol_table.get());
      for (auto& def : *(decl->var_def_list)) {
        auto var_def = static_cast<VarDefAST*>(def.get());
        std::vector<std::shared_ptr<Value>> init_vals;
        // 构建变量类型
        TypeInfo var_type = TypeInfoModelToTypeInfo(decl->elem_type.get());
        switch (var_def->type) {
          case 0: {
            // 普通变量，无初始化
            init_vals.push_back(std::make_shared<Value_INTEGER>(0));
            auto global_alloc = std::make_shared<Value_GLOBOL_ALLOC>(
                name_manager->AllocateNamed(var_def->ident), init_vals, var_type);
            program->values.push_back(global_alloc);
            symbol_table->Insert(var_def->ident, global_alloc, var_type, false);
            break;
          }
          case 1: {
            // 普通变量，有初始化
            auto exps = static_cast<InitValAST*>(var_def->var_exps.get());
            init_vals.push_back(std::make_shared<Value_INTEGER>(
                global_var_computer.Evaluate(exps->exp.get())));
            auto global_alloc = std::make_shared<Value_GLOBOL_ALLOC>(
                name_manager->AllocateNamed(var_def->ident), init_vals, var_type);
            program->values.push_back(global_alloc);
            symbol_table->Insert(var_def->ident, global_alloc, var_type, false);
            break;
          }
          case 2: {
            // 数组变量，无初始化
            std::vector<std::shared_ptr<Value>> init_vals;
            std::vector<int> dims;
            // 解析数组维度
            for (auto& dim : *(var_def->array_size)) {
              dims.push_back(global_var_computer.Evaluate(dim.get()));
            }
            int size = 1;
            for (auto& dim : dims) {
              size *= dim;
            }
            // 使用默认值初始化数组
            for (int i = 0; i < size; i++) {
              init_vals.push_back(std::make_shared<Value_INTEGER>(0));
            }
            // 构建数组类型
            TypeInfo array_type = var_type;
            for (int d : dims) {
              array_type.modifiers.push_back({TypeInfo::TypeKind::ARRAY, d});
            }
            // 插入符号表和IR
            auto global_alloc = std::make_shared<Value_GLOBOL_ALLOC>(
                name_manager->AllocateNamed(var_def->ident), init_vals, array_type);
            program->values.push_back(global_alloc);
            symbol_table->Insert(var_def->ident, global_alloc, array_type, false);
            break;
          }
          case 3: {
            // 数组变量，有初始化
            std::vector<std::shared_ptr<Value>> init_vals;
            std::vector<int> dims;
            // 解析数组维度
            for (auto& dim : *(var_def->array_size)) {
              dims.push_back(global_var_computer.Evaluate(dim.get()));
            }
            int size = 1;
            for (auto& dim : dims) {
              size *= dim;
            }
            // 解析初始化列表
            auto exps = static_cast<InitValAST*>(var_def->var_exps.get());
            ParseArrayInitVals(exps, init_vals, dims, 0);
            // 确保初始化值数量正确（补零）
            while (init_vals.size() < size) {
              init_vals.push_back(std::make_shared<Value_INTEGER>(0));
            }
            // 构建数组类型
            TypeInfo array_type = var_type;
            for (int d : dims) {
              array_type.modifiers.push_back({TypeInfo::TypeKind::ARRAY, d});
            }
            // 插入符号表和IR
            auto global_alloc = std::make_shared<Value_GLOBOL_ALLOC>(
                name_manager->AllocateNamed(var_def->ident), init_vals, array_type);
            program->values.push_back(global_alloc);
            symbol_table->Insert(var_def->ident, global_alloc, array_type, false);
            break;
          }
          default:
            break;
        }
      }
    } else if (dynamic_cast<ConstDeclAST*>(func_def_or_decl.get())) {
      auto decl = static_cast<ConstDeclAST*>(func_def_or_decl.get());
      // 处理全局常量声明（常量不需要分配内存，直接存储值）
      ExpComputer global_var_computer(symbol_table.get());
      // 构建基础类型
      TypeInfo base_type = TypeInfoModelToTypeInfo(decl->elem_type.get());
      for (auto& def : *(decl->const_def_list)) {
        auto const_def = static_cast<ConstDefAST*>(def.get());
        switch (const_def->type) {
          case 0: {
            // 普通常量
            auto exps = static_cast<ConstInitValAST*>(const_def->const_exps.get());
            auto const_value = std::make_shared<Value_INTEGER>(
                      global_var_computer.Evaluate(exps->exp.get()));
            symbol_table->Insert(const_def->ident, const_value, base_type, true);
            break;
          }
          case 1: {
            // 1. 解析数组的初始化值，返回数组的初始化数组
            std::vector<std::shared_ptr<Value>> init_vals;
            std::vector<int> dims;
            // 解析数组维度
            for (auto& dim : *(const_def->array_size)) {
              dims.push_back(global_var_computer.Evaluate(dim.get()));
            }
            // 解析初始化列表
            auto exps = static_cast<ConstInitValAST*>(const_def->const_exps.get());
            ParseArrayInitVals(exps, init_vals, dims, 0);
            // 构建数组类型
            TypeInfo array_type = base_type;
            for (int d : dims) {
              array_type.modifiers.push_back({TypeInfo::TypeKind::ARRAY, d});
            }
            // 2. 将数组加入符号表
            auto global_alloc = std::make_shared<Value_GLOBOL_ALLOC>(
                name_manager->AllocateNamed(const_def->ident), init_vals, array_type);
            program->values.push_back(global_alloc);
            symbol_table->Insert(const_def->ident, global_alloc, array_type, true);
            break;
          }
          default:
            break;
        }
      }
    }
  }
}

void IRgenerator::Visit(FuncDefAST* ast) {
  LOG("FuncDefAST");
  current_block = std::make_unique<BasicBlock>("entry");
  current_func->return_type = TypeInfoModelToTypeInfo(ast->func_type.get());
  current_func->name = ast->ident;
  // 访问block中的语句，构造current_block
  auto block = static_cast<BlockAST*>(ast->block.get());
  // 函数参数声明单独创建一个作用域
  symbol_table->Push();
  if (ast->func_params) {
    for (auto& param : *ast->func_params) {
      // 转换参数类型
      TypeInfo param_type = TypeInfoModelToTypeInfo(param->type.get());
      
      // 将函数参数加入符号表，函数参数作为一个值的引用
      auto param_ref = std::make_shared<Value_FUNC_ARG_REF>(
          name_manager->AllocateNamed(param->ident), param_type);
      
      // 获取数组维度（如果有）
      std::vector<int> dims = param_type.getArrayDims();
      
      // 为参数分配本地存储
      auto val = std::make_shared<Value_ALLOC>(name_manager->AllocateTempHint(param->ident), param_type);
      current_block->stmts.push_back(val);
      current_block->stmts.push_back(std::make_shared<Value_STORE>(param_ref, val));
      LOG("Parameter " + param->ident + " type=" + param_type.toKoopaString() + " dims=" + std::to_string(dims.size()));
      // 插入符号表
      symbol_table->Insert(param->ident, val, param_type, false);
      
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
    if (current_func->return_type.base == BaseType::VOID) {
      current_block->stmts.push_back(std::make_shared<Value_RETURN>(nullptr));
    } else {
      std::cerr << "warning: Non-void function does not end with return, adding default return 0" << std::endl;
      current_block->stmts.push_back(std::make_shared<Value_RETURN>(std::make_shared<Value_INTEGER>(0)));
    }
  }
  // 当current_block构造完成后，将其加入到current_func中
  current_func->blocks.push_back(std::move(current_block));
  symbol_table->Pop();
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
      std::shared_ptr<Value> val = nullptr;
        // 从栈中弹出值
      if(!current_value_stack.empty()) {
        val = current_value_stack.top();
        current_value_stack.pop();
      }
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
      auto symbol = static_cast<VariableSymbol*>(symbol_table->FindSymbol(lval->ident));
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
      if (lval->type == 0) {
        // 普通变量：直接 store
        current_block->stmts.push_back(std::make_shared<Value_STORE>(val, variable));
      } else {
        // 数组/指针元素：支持多维索引
        // 从符号表中获取数组维度
        std::vector<int> dims = symbol->type.getArrayDims();
        // 1. 先计算所有索引值
        std::vector<std::shared_ptr<Value>> indices;
        for (auto& index_ast : *(lval->array_index)) {
          index_ast->Accept(this);
          indices.push_back(current_value_stack.top());
          current_value_stack.pop();
        }
        // 2. 生成 getelemptr/getptr 指令链
        std::shared_ptr<Value> current_ptr = variable;
        for (size_t i = 0; i < indices.size(); i++) {
          // 计算 elem_size：剩余维度的乘积 * 4
          int elem_size = 4;
          for (size_t j = i + 1; j < dims.size(); j++) {
            elem_size *= dims[j];
          }
          // 根据类型选择指令：数组用 getelemptr，指针用 getptr
          if (i < symbol->type.modifiers.size() && 
              symbol->type.modifiers[i].first == TypeInfo::TypeKind::ARRAY) {
            auto get_elem_ptr = std::make_shared<Value_GET_ELEM_PTR>(name_manager->AllocateTemp(), current_ptr, indices[i], elem_size);
            current_block->stmts.push_back(get_elem_ptr);
            current_ptr = get_elem_ptr;
          } else {
            // 指针类型，生成 getptr 指令
            auto load_ptr = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), current_ptr);
            current_block->stmts.push_back(load_ptr);
            auto get_ptr = std::make_shared<Value_GET_PTR>(name_manager->AllocateTemp(), load_ptr, indices[i], elem_size);
            current_block->stmts.push_back(get_ptr);
            current_ptr = get_ptr;
          }
        }
        // 3. 生成 store 指令
        current_block->stmts.push_back(std::make_shared<Value_STORE>(val, current_ptr));
      }
      break;
    }
    case Stmt_Type::AST_STMT_BLOCK: {
      auto& block_stmt = std::get<StmtAST::Block_STMT>(ast->stmt);
      block_stmt->Accept(this);
      break;
    }
    case Stmt_Type::AST_STMT_IF_ELSE: {
      auto& if_else_stmt = std::get<StmtAST::IfElse_STMT>(ast->stmt);
      
      // 1. 计算条件表达式
      if_else_stmt.exp->Accept(this);
      assert(!current_value_stack.empty());
      auto cond_val = current_value_stack.top();
      current_value_stack.pop();
      
      // 2. 生成基本块名称
      std::string then_name = name_manager->AllocateLabel("if_then");
      std::string end_name = name_manager->AllocateLabel("if_end");
      std::string else_name = if_else_stmt.else_stmt != nullptr 
                              ? name_manager->AllocateLabel("if_else")
                              : end_name;
      
      // 3. 生成分支指令
      //   br %cond, %then, %else (或 %end 如果没有else)
      current_block->stmts.push_back(std::make_shared<Value_BRANCH>(cond_val, then_name, else_name));
      current_func->blocks.push_back(std::move(current_block));
      // 4. 生成 then 基本块
      //   ... if语句体 ...
      //   jump %end (如果不是return结尾)
      current_block = std::make_unique<BasicBlock>(then_name);
      if_else_stmt.if_stmt->Accept(this);
      if (!current_block->IsTerminated()) {
        current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
      }
      current_func->blocks.push_back(std::move(current_block));
      // 5. 生成 else 基本块 (如果有)
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
      
      // 6. 生成 end 基本块
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
      
      // 3. 生成基本块
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

    // 1. 只计算 lhs
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
    
    // 2. 分配临时变量，默认值为 1 (true)
    TypeInfo int_type(BaseType::INT);
    auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateTempHint("result"), int_type);
    symbol_table->Insert(alloc->name, alloc, int_type, false);
    current_block->stmts.push_back(alloc);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(std::make_shared<Value_INTEGER>(1), alloc));
    
    // 3. 生成分支指令
    auto then_name = name_manager->AllocateLabel("if_then");
    auto end_name = name_manager->AllocateLabel("if_end");
    auto eq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), lhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_EQ);
    current_block->stmts.push_back(eq);
    current_block->stmts.push_back(std::make_shared<Value_BRANCH>(eq, then_name, end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // 4. 生成 then 基本块 (lhs 为 false 时才计算 rhs)
    current_block = std::make_unique<BasicBlock>(then_name);
    l_or.l_and_exp->Accept(this);  // 在 then 块中计算 rhs
    auto rhs_val = current_value_stack.top();
    current_value_stack.pop();
    auto neq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(neq);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(neq, alloc));
    current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // 5. 生成 end 基本块
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

    // 1. 只计算 lhs
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

    // 2. 分配临时变量，默认值为 0 (false)
    TypeInfo int_type(BaseType::INT);
    auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateTempHint("result"), int_type);
    symbol_table->Insert(alloc->name, alloc, int_type, false);
    current_block->stmts.push_back(alloc);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(std::make_shared<Value_INTEGER>(0), alloc));
    
    // 3. 生成分支指令
    auto then_name = name_manager->AllocateLabel("if_then");
    auto end_name = name_manager->AllocateLabel("if_end");
    auto ne = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), lhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(ne);
    current_block->stmts.push_back(std::make_shared<Value_BRANCH>(ne, then_name, end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // 4. 生成 then 基本块 (lhs 为 true 时才计算 rhs)
    current_block = std::make_unique<BasicBlock>(then_name);
    l_and.eq_exp->Accept(this);  // 在 then 块中计算 rhs
    auto rhs_val = current_value_stack.top();
    current_value_stack.pop();
    auto neq = std::make_shared<Value_BINARY>(name_manager->AllocateTemp(), rhs_val, std::make_shared<Value_INTEGER>(0), Binary_Op_Type::KOOPA_RBO_NOT_EQ);
    current_block->stmts.push_back(neq);
    current_block->stmts.push_back(std::make_shared<Value_STORE>(neq, alloc));
    current_block->stmts.push_back(std::make_shared<Value_JUMP>(end_name));
    current_func->blocks.push_back(std::move(current_block));
    
    // 5. 生成 end 基本块
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
    switch (const_def->type) {
    case 0: {
      // 常量定义（普通变量）
      ExpComputer computer(symbol_table.get());
      int val = computer.Evaluate(const_def->const_exps.get());
      auto const_val = std::make_shared<Value_INTEGER>(val);
      TypeInfo const_type = TypeInfoModelToTypeInfo(ast->elem_type.get());
      symbol_table->Insert(const_def->ident, const_val, const_type, true);
      break;
    }
    case 1: {
      // 常量数组定义
      ExpComputer computer(symbol_table.get());
      // 1. 解析所有维度
      std::vector<int> dims;
      for (auto& dim_ast : *(const_def->array_size)) {
        dims.push_back(computer.Evaluate(dim_ast.get()));
      }
      // 计算总元素个数
      int total_size = 1;
      for (int d : dims) {
        total_size *= d;
      }
      // 2. 生成alloc指令
      TypeInfo array_type = TypeInfoModelToTypeInfo(ast->elem_type.get());
      for (int d : dims) {
        array_type.modifiers.push_back({TypeInfo::TypeKind::ARRAY, d});
      }
      auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateNamed(const_def->ident), array_type);
      current_block->stmts.push_back(alloc);
      // 插入符号表
      symbol_table->Insert(const_def->ident, alloc, array_type, true);
      // 3. 解析初始化列表（支持嵌套）
      std::vector<std::shared_ptr<Value>> init_vals;
      auto init_ast = static_cast<ConstInitValAST*>(const_def->const_exps.get());
      if (init_ast) {
        ParseArrayInitVals(init_ast, init_vals, dims, 0);
      }
      // 确保初始化值数量正确（补零）
      while (init_vals.size() < total_size) {
        init_vals.push_back(std::make_shared<Value_INTEGER>(0));
      }
      // 4. 生成store指令初始化数组（多维索引）
      for (int i = 0; i < total_size; i++) {
        // 将一维索引转换为多维索引（行优先）
        std::vector<int> indices(dims.size());
        int remaining = i;
        for (int j = dims.size() - 1; j >= 0; j--) {
          indices[j] = remaining % dims[j];
          remaining /= dims[j];
        }
        // 生成连续的 getelemptr 指令
        std::shared_ptr<Value> current_ptr = alloc;
        for (size_t k = 0; k < indices.size(); k++) {
          // 计算 elem_size：剩余维度的乘积 * 4
          int elem_size = 4;
          for (size_t j = k + 1; j < dims.size(); j++) {
            elem_size *= dims[j];
          }
          auto get_elem_ptr = std::make_shared<Value_GET_ELEM_PTR>(
              name_manager->AllocateTemp(), current_ptr, std::make_shared<Value_INTEGER>(indices[k]), elem_size);
          current_block->stmts.push_back(get_elem_ptr);
          current_ptr = get_elem_ptr;
        }
        // 生成 store 指令
        auto store = std::make_shared<Value_STORE>(init_vals[i], current_ptr);
        current_block->stmts.push_back(store);
      }
      break;
    }
    }
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
  auto symbol = symbol_table->FindSymbol(ast->ident);
  auto var_symbol = static_cast<VariableSymbol*>(symbol);
  
  if (var_symbol->type.isScalar()) {
    // 普通变量
    if (var_symbol->is_const) {
      // 常量直接压入值
      current_value_stack.push(val);
    } else {
      // 非常量变量，生成load指令
      auto load_val = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), val);
      current_block->stmts.push_back(load_val);
      current_value_stack.push(load_val);
    }
  } else {
    // 数组元素读取：支持多维索引（常量和非常量数组都需要 getelemptr + load）
    // 1. 先计算所有索引值
    auto index_list = static_cast<std::vector<std::unique_ptr<BaseAST>>*>(ast->array_index.get());
    std::vector<std::shared_ptr<Value>> indices;
    if (index_list) {
      for (auto& index_ast : *index_list) {
        index_ast->Accept(this);
        indices.push_back(current_value_stack.top());
        current_value_stack.pop();
      }
    }
    // 2. 生成 getelemptr/getptr 指令链
    std::shared_ptr<Value> current_ptr = val;
    std::vector<int> dims = var_symbol->type.getArrayDims();
    for (size_t i = 0; i < indices.size(); i++) {
      // 计算 elem_size：剩余维度的乘积 * 4
      int elem_size = 4;
      for (size_t j = i + 1; j < dims.size(); j++) {
        assert(dims[j] != -1 && "Array size is uncountable");
        elem_size *= dims[j];
      }
      // 根据类型选择指令：数组用 getelemptr，指针用 getptr
      if (i < var_symbol->type.modifiers.size() && 
              var_symbol->type.modifiers[i].first == TypeInfo::TypeKind::ARRAY) {
        // 以*[]为开头的数组类型，生成 getelemptr 指令
        auto get_elem_ptr = std::make_shared<Value_GET_ELEM_PTR>(name_manager->AllocateTemp(), current_ptr, indices[i], elem_size);
        current_block->stmts.push_back(get_elem_ptr);
        current_ptr = get_elem_ptr;
      } else {
        // 指针类型，生成 getptr 指令(以**为开头)
        auto load_ptr = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), current_ptr);
        current_block->stmts.push_back(load_ptr);
        auto get_ptr = std::make_shared<Value_GET_PTR>(name_manager->AllocateTemp(), load_ptr, indices[i], elem_size);
        current_block->stmts.push_back(get_ptr);
        current_ptr = get_ptr;
      }
    }
    // 3. 判断是否完全索引：未完全索引时压入指针，完全索引时 load
    if (indices.size() < dims.size()) {
      LOG("Not fully indexed, push pointer");
      // 未完全索引，结果仍是指向子数组的指针，不 load
      current_value_stack.push(current_ptr);
    } else {
      LOG("Fully indexed, generate load");
      // 完全索引到标量元素，生成 load 指令
      auto load_val = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), current_ptr);
      current_block->stmts.push_back(load_val);
      current_value_stack.push(load_val);
    }
  }
}



void IRgenerator::Visit(OpAST* ast) {
  LOG("OpAST");
  // NEVER REACH HERE
}

void IRgenerator::Visit(VarDefAST* ast) {
  LOG("VarDefAST");
  // NEVER REACH HERE
}

void IRgenerator::Visit(VarDeclAST* ast) {
  LOG("VarDeclAST");
  // VarDef
  for (auto& item : *(ast->var_def_list)) {
    auto var_def = static_cast<VarDefAST*>(item.get());
    // 构建变量类型
    TypeInfo var_type = TypeInfoModelToTypeInfo(ast->elem_type.get());
    switch (var_def->type) {
      case 0: {
        auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateNamed(var_def->ident), var_type);
        current_block->stmts.push_back(alloc);
        // 在符号表中插入变量
        symbol_table->Insert(var_def->ident, alloc, var_type, false);
        break;
      }
      case 1: {
        // 生成alloc指令
        auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateNamed(var_def->ident), var_type);
        current_block->stmts.push_back(alloc);
        // 初始化变量
        auto exp = static_cast<InitValAST*>(var_def->var_exps.get());
        exp->exp->Accept(this);
        // 为变量赋值，生成store指令
        auto store = std::make_shared<Value_STORE>(current_value_stack.top(), alloc);
        current_block->stmts.push_back(store);
        current_value_stack.pop();
        // 在符号表中插入变量
        symbol_table->Insert(var_def->ident, alloc, var_type, false);
        break;
      }
      case 2:
      case 3: {
        // 数组定义（case 2: 无初始化值，case 3: 有初始化值）
        ExpComputer computer(symbol_table.get());
        // 1. 解析所有维度
        std::vector<int> dims;
        for (auto& dim_ast : *(var_def->array_size)) {
          dims.push_back(computer.Evaluate(dim_ast.get()));
        }
        // 计算总元素个数
        int total_size = 1;
        for (int d : dims) {
          total_size *= d;
        }
        // 构建数组类型
        TypeInfo array_type = TypeInfoModelToTypeInfo(ast->elem_type.get());
        for (int d : dims) {
          array_type.modifiers.push_back({TypeInfo::TypeKind::ARRAY, d});
        }
        // 2. 生成alloc指令
        auto alloc = std::make_shared<Value_ALLOC>(name_manager->AllocateNamed(var_def->ident), array_type);
        current_block->stmts.push_back(alloc);
        // 插入符号表
        symbol_table->Insert(var_def->ident, alloc, array_type, false);
        // 3. 准备初始化值列表
        std::vector<std::shared_ptr<Value>> init_vals;
        if (var_def->type == 3) {
          // case 3: 解析初始化列表
          auto init_ast = static_cast<InitValAST*>(var_def->var_exps.get());
          if (init_ast) {
            ParseArrayInitVals(init_ast, init_vals, dims, 0);
          }
        }
        // 确保初始化值数量正确（补零）
        while (init_vals.size() < total_size) {
          init_vals.push_back(std::make_shared<Value_INTEGER>(0));
        }
        // 4. 生成store指令初始化数组（多维索引）
        for (int i = 0; i < total_size; i++) {
          // 将一维索引转换为多维索引（行优先）
          std::vector<int> indices(dims.size());
          int remaining = i;
          for (int j = dims.size() - 1; j >= 0; j--) {
            indices[j] = remaining % dims[j];
            remaining /= dims[j];
          }
          // 生成连续的 getelemptr 指令
          std::shared_ptr<Value> current_ptr = alloc;
          for (size_t k = 0; k < indices.size(); k++) {
            // 计算 elem_size：剩余维度的乘积 * 4
            int elem_size = 4;
            for (size_t j = k + 1; j < dims.size(); j++) {
              elem_size *= dims[j];
            }
            auto get_elem_ptr = std::make_shared<Value_GET_ELEM_PTR>(
                name_manager->AllocateTemp(), current_ptr, std::make_shared<Value_INTEGER>(indices[k]), elem_size);
            current_block->stmts.push_back(get_elem_ptr);
            current_ptr = get_elem_ptr;
          }
          // 生成 store 指令
          auto store = std::make_shared<Value_STORE>(init_vals[i], current_ptr);
          current_block->stmts.push_back(store);
        }
        break;
      }
      default:
        assert(0);
        break;
    }
  }
}

void IRgenerator::Visit(InitValAST* ast) {
  LOG("InitValAST");
  // 不生成任何东西，空操作
}

void IRgenerator::Visit(ConstInitValAST* ast) {
  LOG("ConstInitValAST");
  // 不生成任何东西，空操作
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
      // 数组退化：如果实参类型是 *[t, len] 而形参期望更简单的指针，
      // 则插入 getelemptr ..., 0 将数组指针退化为元素指针
      if (param_val->type_info.isPointer() && param_val->type_info.modifiers.size() >= 2) {
            LOG("Array decay for function argument");
        if (param_val->type_info.modifiers[1].first == TypeInfo::TypeKind::ARRAY) {
          auto decay = std::make_shared<Value_GET_ELEM_PTR>(
              name_manager->AllocateTemp(), param_val, std::make_shared<Value_INTEGER>(0), 4);
          current_block->stmts.push_back(decay);
          param_val = decay;
        } else {
          auto decay = std::make_shared<Value_LOAD>(name_manager->AllocateTemp(), param_val);
          current_block->stmts.push_back(decay);
          param_val = decay;
        }
      }
      args.push_back(param_val);
    }
  }
  // 2. 生成call指令
  // 查看函数的返回值类型，如果是void则不需要处理返回值
  auto func_symbol = static_cast<FunctionSymbol*>(symbol_table->FindSymbol(ast->ident));
  if (func_symbol->return_type.base != BaseType::VOID) {
    // 有返回值：分配一个临时符号作为返回值名字
    auto ret_name = name_manager->AllocateTemp();
    auto call_val = std::make_shared<Value_Call>(ret_name, "@" + ast->ident, args, func_symbol->return_type);
    current_value_stack.push(call_val);  // 把 call 指令本身压栈
    current_block->stmts.push_back(call_val);
  } else {
    // void 函数：name 为空，类型信息为空
    auto call_val = std::make_shared<Value_Call>("null", "@" + ast->ident, args, func_symbol->return_type);
    current_block->stmts.push_back(call_val);
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
    fs << param->name << ": " << param->type_info.toKoopaString();
    if (i != func->params.size() - 1) {
      fs << ", ";
    }
  }
  fs << ")";
  if (func->return_type.base != BaseType::VOID) {
    fs << ": " << func->return_type.toKoopaString();
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
    fs << func_decl->params_type[i].toKoopaString();
    if (i != func_decl->params_type.size() - 1) {
      fs << ", ";
    }
  }
  fs << ")";
  if (func_decl->return_type.base != BaseType::VOID) {
    fs << ": " << func_decl->return_type.toKoopaString();
  }
  fs << std::endl;
}

void IROutputer::Visit(Program* program) {
  for (auto& value : program->values) {
    value->Accept(this);
  }
  fs << std::endl;
  for (auto& func : program->functions) {
    func->Accept(this);
  }
}

void IROutputer::Visit(Value_ALLOC* alloc) {
  TypeInfo alloc_type = alloc->type_info;
  alloc_type.modifiers.erase(alloc_type.modifiers.begin());
  std::vector<int> dims = alloc_type.getArrayDims();
  if (dims.empty()) {
    fs << "  " << alloc->name << " = alloc " << alloc_type.toKoopaString() << std::endl;
  } else {
    // 多维数组：嵌套输出类型，如 [[i32, 3], 2] 表示 2x3 数组
    fs << "  " << alloc->name << " = alloc ";
    fs << alloc_type.toKoopaString();
    fs << std::endl;
  }
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

// 辅助函数：递归输出多维数组初始化值
void IROutputer::OutputArrayInit(const std::vector<std::shared_ptr<Value>>& init,
                                  const std::vector<int>& dims,
                                  int level,
                                  std::fstream& fs,
                                  int& index) {
  fs << "{";
  if (level == dims.size() - 1) {
    // 最内层：直接输出元素值
    for (int i = 0; i < dims[level]; i++) {
      fs << init[index]->name;
      index++;
      if (i < dims[level] - 1) {
        fs << ", ";
      }
    }
  } else {
    // 非最内层：递归输出子数组
    for (int i = 0; i < dims[level]; i++) {
      OutputArrayInit(init, dims, level + 1, fs, index);
      if (i < dims[level] - 1) {
        fs << ", ";
      }
    }
  }
  fs << "}";
}

void IROutputer::Visit(Value_GLOBOL_ALLOC* glob_alloc) {
  fs << "global " << glob_alloc->name << " = alloc ";
  TypeInfo alloc_type = glob_alloc->type_info;
  alloc_type.modifiers.erase(alloc_type.modifiers.begin());
  std::vector<int> dims = alloc_type.getArrayDims();
  if (dims.empty()) {
    // 普通变量
    fs << alloc_type.toKoopaString() << ", "<< glob_alloc->init[0]->name;
  } else {
    // 数组
    fs << alloc_type.toKoopaString();
    fs << ", ";
    // 递归输出多维数组初始化值
    int index = 0;
    OutputArrayInit(glob_alloc->init, dims, 0, fs, index);
  }
  fs << std::endl;
}

void IROutputer::Visit(Value_GET_ELEM_PTR* get_elem_ptr) {
  fs << "  " << get_elem_ptr->name << " = getelemptr " << get_elem_ptr->base->name << ", " << get_elem_ptr->index->name << std::endl;
}

void IROutputer::Visit(Value_GET_PTR* get_ptr) {
  fs << "  " << get_ptr->name << " = getptr " << get_ptr->base->name << ", " << get_ptr->index->name << std::endl;
}

ExpComputer::ExpComputer(SymbolTable* symbol_table) 
  : val_stack(std::make_unique<std::stack<int>>()), symbol_table(symbol_table) {}

ExpComputer::~ExpComputer() {}

int ExpComputer::Evaluate(BaseAST* ast) {
  int val = 0;
  ast->Accept(this);
  val = val_stack->top();
  val_stack->pop();
  return val;
}

void ExpComputer::Visit(LValAST* ast) {
  auto symbol = symbol_table->FindSymbol(ast->ident);
  auto var_symbol = static_cast<VariableSymbol*>(symbol);
  if (symbol && var_symbol->is_const) {
    auto val = symbol->value;
    if (val && val->type == Value_Type::KOOPA_RVT_INTEGER) {
      val_stack->push(static_cast<Value_INTEGER*>(val.get())->val);
      return;
    }
  }
  val_stack->push(0);
}

void ExpComputer::Visit(NumberAST* ast) {
  val_stack->push(ast->num);
}

void ExpComputer::Visit(ExpAST* ast) {
  ast->l_or_exp->Accept(this);
}

void ExpComputer::Visit(PrimaryExpAST* ast) {
  ast->mem->Accept(this);
}

void ExpComputer::Visit(UnaryExpAST* ast) {
  if (ast->type == 0) {
    auto& primary = std::get<std::unique_ptr<BaseAST>>(ast->mem);
    primary->Accept(this);
  } else {
    auto& unary = std::get<UnaryExpAST::UnaryExp>(ast->mem);
    unary.unary_exp->Accept(this);
    unary.unary_op->Accept(this);
  }
}

void ExpComputer::Visit(UnaryOpAST* ast) {
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

void ExpComputer::Visit(AddExpAST* ast) {
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

void ExpComputer::Visit(MulExpAST* ast) {
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

void ExpComputer::Visit(RelExpAST* ast) {
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

void ExpComputer::Visit(EqExpAST* ast) {
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

void ExpComputer::Visit(LOrExpAST* ast) {
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

void ExpComputer::Visit(LAndExpAST* ast) {
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

void ExpComputer::Visit(BinaryOpAST* ast) {
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

void ExpComputer::Visit(LGBinaryOpAST* ast) {
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

void ExpComputer::Visit(StmtAST* ast) {}

void ExpComputer::Visit(BlockItemAST* ast) {}

void ExpComputer::Visit(NullAST* ast) {}

void ExpComputer::Visit(FuncCallAST* ast) {}

void ExpComputer::Visit(OpAST* ast) {}

void ExpComputer::Visit(CompUnitAST* ast) {}

void ExpComputer::Visit(FuncDefAST* ast) {}

void ExpComputer::Visit(BlockAST* ast) {}

void ExpComputer::Visit(InitValAST* ast) {}

void ExpComputer::Visit(ConstInitValAST* ast) {
  if (ast->is_leaf && ast->exp) {
    ast->exp->Accept(this);
  } else {
    // 非叶子节点不应该在这里被计算
    assert(0);
    val_stack->push(0);
  }
}

void ExpComputer::Visit(VarDeclAST* ast) {}

void ExpComputer::Visit(ConstDeclAST* ast) {}

void ExpComputer::Visit(ConstDefAST* ast) {}

void ExpComputer::Visit(VarDefAST* ast) {}
