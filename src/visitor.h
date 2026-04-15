#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <cassert>
#include <fstream>

class BaseAST;
class CompUnitAST;
class FuncTypeAST;
class FuncDefAST;
class BlockAST;
class NumberAST;
class StmtAST;

class Visitor {};

class ASTVisitor : public Visitor {
  public:
    virtual ~ASTVisitor() {}
    virtual void Visit(CompUnitAST* ast) = 0;
    virtual void Visit(FuncTypeAST* ast) = 0;
    virtual void Visit(FuncDefAST* ast) = 0;
    virtual void Visit(BlockAST* ast) = 0;
    virtual void Visit(NumberAST* ast) = 0;
    virtual void Visit(StmtAST* ast) = 0;
};

class Value;
class BasicBlock;
class Function;
class Program;

class IRVisitor : public Visitor {
  public:
    virtual ~IRVisitor() {}
    virtual void Visit(Value* val) = 0;
    virtual void Visit(BasicBlock* block) = 0;
    virtual void Visit(Function* func) = 0;
    virtual void Visit(Program* program) = 0;
};