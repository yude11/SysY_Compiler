#pragma once

#include <stack>
#include <memory>

#include "visitor.h"
#include "IR.h"
#include "AST.h"
#include "SymbolTable.h"
#include "IRNameManager.h"


// 内存IR生成器
class IRgenerator : public ASTVisitor {
  public:
    IRgenerator();

    ~IRgenerator();
    // 对各种AST节点进行访问函数
    void Visit(CompUnitAST* ast) override;
    void Visit(FuncTypeAST* ast) override;
    void Visit(FuncDefAST* ast) override;
    void Visit(BlockAST* ast) override;
    void Visit(NumberAST* ast) override;
    void Visit(StmtAST* ast) override;
    void Visit(ExpAST* ast) override;
    void Visit(PrimaryExpAST* ast) override;
    void Visit(UnaryExpAST* ast) override;
    void Visit(OpAST* ast) override;
    void Visit(UnaryOpAST* ast) override;
    void Visit(BinaryOpAST* ast) override;
    void Visit(LGBinaryOpAST* ast) override;
    void Visit(AddExpAST* ast) override;
    void Visit(MulExpAST* ast) override;
    void Visit(LOrExpAST* ast) override;
    void Visit(LAndExpAST* ast) override;
    void Visit(EqExpAST* ast) override;
    void Visit(RelExpAST* ast) override;
    void Visit(BlockItemAST* ast) override;
    void Visit(ConstDeclAST* ast) override;
    void Visit(ConstDefAST* ast) override;
    void Visit(LValAST* ast) override;
    void Visit(VarDefAST* ast) override;
    void Visit(VarDeclAST* ast) override;
    void Visit(InitValAST* ast) override;
    void Visit(ConstInitValAST* ast) override;
    void Visit(NullAST* ast) override;
    void Visit(FuncCallAST* ast) override;

    // 输出IR表示
    void OutputIR(std::string output);

    // 解析数组初始化值
    void ParseArrayInitVals(InitValAST* ast, std::vector<std::shared_ptr<Value>>& init_vals, const std::vector<int>& dims, int level);
    void ParseArrayInitVals(ConstInitValAST* ast, std::vector<std::shared_ptr<Value>>& init_vals, const std::vector<int>& dims, int level);

    std::unique_ptr<Program> program;
    std::unique_ptr<Function> current_func;
    std::unique_ptr<BasicBlock> current_block;
    std::stack<std::shared_ptr<Value>> current_value_stack;

    // while 栈用于支持break和continue指令
    std::stack<int> while_entry_stack;

    // 符号表
    std::unique_ptr<SymbolTable> symbol_table;

    // IR命名管理器
    std::unique_ptr<IRNameManager> name_manager;
};

// 输出文字形式的IR到文件中
class IROutputer : public IRVisitor {
  public:
    IROutputer();

    IROutputer(std::string output, std::unique_ptr<SymbolTable> symbol_table);
    
    ~IROutputer();
    // 对IR节点进行访问函数
    void Visit(Value_FUNC_ARG_REF* ref) override;
    void Visit(Value_RETURN* return_val) override;
    void Visit(Value_INTEGER* integer) override;
    void Visit(Value_BINARY* binary) override;
    void Visit(Value_ALLOC* alloc) override;
    void Visit(Value_GLOBOL_ALLOC* glob_alloc) override;
    void Visit(Value_GET_ELEM_PTR* get_elem_ptr) override;
    
    // 辅助函数：递归输出多维数组初始化值
    void OutputArrayInit(const std::vector<std::shared_ptr<Value>>& init, 
                         const std::vector<int>& dims, 
                         int level, 
                         std::fstream& fs,
                         int& index);
    void Visit(Value_LOAD* load) override;
    void Visit(Value_Call* call) override;
    void Visit(Value_STORE* store) override;
    void Visit(Value_BRANCH* branch) override;
    void Visit(Value_JUMP* jump) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function_Decl* func_decl) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
    std::unique_ptr<SymbolTable> symbol_table;
};

class ExpComputer : public ASTVisitor {
  public:
    ExpComputer(SymbolTable* symbol_table);

    ~ExpComputer();

    int Evaluate(BaseAST* ast);
    // 对各种AST节点进行访问函数
    void Visit(VarDeclAST* ast) override;

    void Visit(ConstDeclAST* ast) override;
    void Visit(CompUnitAST* ast) override;
    void Visit(FuncTypeAST* ast) override;
    void Visit(FuncDefAST* ast) override;
    void Visit(BlockAST* ast) override;
    void Visit(NumberAST* ast) override;
    void Visit(StmtAST* ast) override;
    void Visit(ExpAST* ast) override;
    void Visit(PrimaryExpAST* ast) override;
    void Visit(UnaryExpAST* ast) override;
    void Visit(OpAST* ast) override;
    void Visit(UnaryOpAST* ast) override;
    void Visit(BinaryOpAST* ast) override;
    void Visit(LGBinaryOpAST* ast) override;
    void Visit(AddExpAST* ast) override;
    void Visit(MulExpAST* ast) override;
    void Visit(LOrExpAST* ast) override;
    void Visit(LAndExpAST* ast) override;
    void Visit(EqExpAST* ast) override;
    void Visit(RelExpAST* ast) override;
    void Visit(BlockItemAST* ast) override;
    void Visit(InitValAST* ast) override;
    void Visit(ConstInitValAST* ast) override;
    void Visit(ConstDefAST* ast) override;
    void Visit(LValAST* ast) override;
    void Visit(VarDefAST* ast) override;
    
    void Visit(NullAST* ast) override;
    void Visit(FuncCallAST* ast) override;

    

    std::unique_ptr<std::stack<int>> val_stack;
    SymbolTable* symbol_table;
    
    struct GlobalVarInfo {
      int init_val;
      std::string type;
      bool is_const;
    };
    std::unordered_map<std::string, GlobalVarInfo> global_var_infos;
};