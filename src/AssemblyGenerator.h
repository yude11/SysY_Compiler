#pragma once

#include "visitor.h"


class AssemblyGenerator : public IRVisitor {
  public:
    AssemblyGenerator(std::string output);
    ~AssemblyGenerator();
    
    void Visit(Value_RETURN* return_val) override;
    void Visit(Value_INTEGER* integer) override;
    void Visit(Value_BINARY* binary) override;
    void Visit(Value_REF* ref) override;
    void Visit(BasicBlock* block) override;
    void Visit(Function* func) override;
    void Visit(Program* program) override;

    std::fstream fs;
};
