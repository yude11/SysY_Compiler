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
class ExpAST;
class PrimaryExpAST;
class UnaryExpAST;
class UnaryOpAST;
class BinaryOpAST;
class OpAST;
class AddExpAST;
class MulExpAST;


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
    virtual void Visit(ExpAST* ast) = 0;
    virtual void Visit(PrimaryExpAST* ast) = 0;
    virtual void Visit(UnaryExpAST* ast) = 0;
    virtual void Visit(OpAST* ast) = 0;
    virtual void Visit(UnaryOpAST* ast) = 0;
    virtual void Visit(BinaryOpAST* ast) = 0;
    virtual void Visit(AddExpAST* ast) = 0;
    virtual void Visit(MulExpAST* ast) = 0;
};

class Value;
class Value_RETURN;
class Value_INTEGER;
class Value_BINARY;
class Value_REF;
class BasicBlock;
class Function;
class Program;

class IRVisitor : public Visitor {
  public:
    virtual ~IRVisitor() {}
    virtual void Visit(Value_RETURN* return_val) = 0;
    virtual void Visit(Value_INTEGER* integer) = 0;
    virtual void Visit(Value_BINARY* binary) = 0;
    virtual void Visit(Value_REF* ref) = 0;
    virtual void Visit(BasicBlock* block) = 0;
    virtual void Visit(Function* func) = 0;
    virtual void Visit(Program* program) = 0;
};