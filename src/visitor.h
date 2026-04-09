#pragma once

#include <memory>

#include "IR.h"

class BaseAST;
class CompUnitAST;
class FuncTypeAST;
class FuncDefAST;
class BlockAST;
class NumberAST;
class StmtAST;


class ASTVisitor {
  public:
    virtual ~ASTVisitor() {}
    virtual void Visit(CompUnitAST* ast) = 0;
    virtual void Visit(FuncTypeAST* ast) = 0;
    virtual void Visit(FuncDefAST* ast) = 0;
    virtual void Visit(BlockAST* ast) = 0;
    virtual void Visit(NumberAST* ast) = 0;
    virtual void Visit(StmtAST* ast) = 0;
};

class IRgenerator : public ASTVisitor {
  public:
    IRgenerator();
    ~IRgenerator();
    void Visit(CompUnitAST* ast) override;
    void Visit(FuncTypeAST* ast) override;
    void Visit(FuncDefAST* ast) override;
    void Visit(BlockAST* ast) override;
    void Visit(NumberAST* ast) override;
    void Visit(StmtAST* ast) override;

    void GenerateIR();

    Program* program;
    Function* current_func;
    BasicBlock* current_block;
    Value* current_value;
};
