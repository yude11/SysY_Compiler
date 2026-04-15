#pragma once

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

    // 输出IR表示
    void OutputIR(std::string output);

    Program* program;
    Function* current_func;
    BasicBlock* current_block;
    Value* current_value;
};

// 输出文字形式的IR到文件中
class IROutputer : public IRVisitor {
  public:
    IROutputer();

    IROutputer(std::string output);
    
    ~IROutputer();
    // 对IR节点进行访问函数
    void Visit(Value* val) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
};
