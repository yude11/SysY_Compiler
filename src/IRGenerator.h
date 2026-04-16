#pragma once

#include <stack>
#include <memory>

#include "visitor.h"
#include "IR.h"
#include "AST.h"


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
    void Visit(UnaryOpAST* ast) override;

    // 输出IR表示
    void OutputIR(std::string output);

    std::unique_ptr<Program> program;
    std::unique_ptr<Function> current_func;
    std::unique_ptr<BasicBlock> current_block;
    std::stack<std::unique_ptr<Value>> current_value_stack;

    // 表示临时变量的编号
    int count = 0;
};

// 输出文字形式的IR到文件中
class IROutputer : public IRVisitor {
  public:
    IROutputer();

    IROutputer(std::string output);
    
    ~IROutputer();
    // 对IR节点进行访问函数
    void Visit(Value_REF* ref) override;
    void Visit(Value_RETURN* return_val) override;
    void Visit(Value_INTEGER* integer) override;
    void Visit(Value_BINARY* binary) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
};
