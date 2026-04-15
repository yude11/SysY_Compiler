#pragma once

#include "visitor.h"


class AssemblyGenerator : public IRVisitor {
  public:
    AssemblyGenerator(std::string output);
    ~AssemblyGenerator();
    
    void Visit(Value* val) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
};
