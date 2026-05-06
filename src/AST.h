#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <cassert>
#include <variant>

#include "IR.h"
#include "visitor.h"

class TypeInfoModel {
  public:
    // 普通变量
    TypeInfoModel(BaseType type) : type(type), kind(TypeInfo::TypeKind::SCALAR) {}
    // 指针变量
    TypeInfoModel(BaseType type, TypeInfo::TypeKind kind) : type(type), kind(kind) {}
    // 数组变量, 指针变量
    TypeInfoModel(BaseType type, TypeInfo::TypeKind kind, 
      std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> dims) 
        : type(type), kind(kind), dims(std::move(dims)) {}
    BaseType type;
    TypeInfo::TypeKind kind;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> dims;
};

class BaseAST {
 public:
  virtual ~BaseAST() = default;

  virtual void Dump() const = 0;

  virtual void Accept(ASTVisitor *visitor) = 0;
};

class NullAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "NullAST";
    }
};

class CompUnitAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
   std::cout << "CompUnitAST { ";
   for (auto& func : func_defs_or_decls) {
     func->Dump();
   }
   std::cout << " }";
  }
  
  // 使用 vector 存储所有函数定义
  std::vector<std::unique_ptr<BaseAST>> func_defs_or_decls;
};

class FuncDefAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

 void Dump() const override {}

  std::unique_ptr<TypeInfoModel> func_type;
  std::string ident;
  std::unique_ptr<BaseAST> block;
  struct FuncParam {
    std::unique_ptr<TypeInfoModel> type;
    std::string ident;
  };
  typedef std::vector<std::unique_ptr<FuncParam>> FuncParams;
  std::unique_ptr<FuncParams> func_params;

};

// BLock = { BlockItem* }
class BlockAST : public BaseAST {
  public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }
  
  void Dump() const override {
    std::cout << "BlockAST { ";
    for (auto& item : *block_items) {
      item->Dump();
    }
    std::cout << " }";
  }
  
  std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> block_items;
};

// BlockItem = Decl | Stmt
class BlockItemAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "BlockItemAST { ";
      if (type == 0) {
        auto& item = std::get<std::unique_ptr<BaseAST>>(mem);
        item->Dump();
      } else {
        auto item = std::get<std::shared_ptr<BaseAST>>(mem);
        item->Dump();
      }
      std::cout << " }";
    }

    int type = -1;
    std::variant<std::unique_ptr<BaseAST>, std::shared_ptr<BaseAST>> mem;
};

class VarDeclAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "VarDeclAST { ";
      for (auto& var_def : *var_def_list) {
        var_def->Dump();
      }
      std::cout << " }";
    }

    // List的大小要大于等于1
    std::unique_ptr<TypeInfoModel> elem_type;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> var_def_list;
};

class VarDefAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "VarDefAST { ";
      // std::cout << ident << " = ";
      // for (auto& var_exp : *var_exps) {
      //   var_exp->Dump();
      // }
      std::cout << " }";
    }

    int type = -1;
    std::string ident;
    std::unique_ptr<BaseAST> var_exps;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> array_size;
};

class InitValAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    if (is_leaf) {
      std::cout << "Leaf: ";
      exp->Dump();
    } else {
      std::cout << "List: { ";
      for (auto& child : children) {
        child->Dump();
        std::cout << " ";
      }
      std::cout << "}";
    }
  }

  // true: 叶子节点（单个 Exp），false: 列表节点
  bool is_leaf = false;
  
  // 叶子时用
  std::unique_ptr<BaseAST> exp;
  
  // 列表时用，每个元素都是 InitValAST
  std::vector<std::unique_ptr<InitValAST>> children;
};



// ConstDecl = CONST BType ConstDef "," ConstDefList ";" | CONST BType ConstDef "," ConstDefList ";"
class ConstDeclAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "ConstDecl { ";
      for (auto& const_def : *const_def_list) {
        const_def->Dump();
      }
      std::cout << " }";
    }

    // List的大小要大于等于1
    std::unique_ptr<TypeInfoModel> elem_type;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> const_def_list;
};

// ConstDef = INDENT = ConstInitVal
// ConstInitVal = ConstExp
class ConstDefAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "ConstDefAST { ";
      // std::cout << ident << " = ";
      // const_exp->Dump();
      std::cout << " }";
    }
    
    int type = -1;
    std::string ident;
    std::unique_ptr<BaseAST> const_exps;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> array_size;
};

class ConstInitValAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      if (is_leaf) {
        std::cout << "Leaf: ";
        exp->Dump();
      } else {
        std::cout << "List: { ";
        for (auto& child : children) {
          child->Dump();
          std::cout << " ";
        }
        std::cout << "}";
      }
    }

    // true: 叶子节点（单个 Exp），false: 列表节点
    bool is_leaf = false;
    
    // 叶子时用
    std::unique_ptr<BaseAST> exp;
    
    // 列表时用，每个元素都是 ConstInitValAST
    std::vector<std::unique_ptr<ConstInitValAST>> children;
};

class NumberAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "NumberAST: " << "{" << num << "}";
  }

  int num;
};

class LValAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "LValAST { ";
      std::cout << ident;
      std::cout << " }";
    }

    int type = -1;
    std::string ident;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> array_index;
};

class StmtAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "StmtAST { ";
    switch (type) {
      case Stmt_Type::AST_STMT_RETURN: {
        auto& return_stmt = std::get<RETURN_STMT>(stmt);
        return_stmt->Dump();
        break;
      }
      case Stmt_Type::AST_STMT_ASSIGN: {
        auto& assign_stmt = std::get<Assign_STMT>(stmt);
        assign_stmt.lval->Dump();
        assign_stmt.exp->Dump();
        break;
      }
      case Stmt_Type::AST_STMT_BREAK: {
        std::cout << "BreakAST { }";
        break;
      }
      case Stmt_Type::AST_STMT_CONTINUE: {
        std::cout << "ContinueAST { }";
        break;
      }
      case Stmt_Type::AST_STMT_EXP: {
        auto& exp_stmt = std::get<Exp_STMT>(stmt);
        exp_stmt->Dump();
        break;
      }
      default:
        break;
    }
    std::cout << " }";
  }
  
  Stmt_Type type;
  typedef std::unique_ptr<BaseAST> RETURN_STMT;
  struct Assign_STMT {
    std::unique_ptr<BaseAST> lval;
    std::unique_ptr<BaseAST> exp;
  };
  typedef std::unique_ptr<BaseAST> Block_STMT;
  typedef std::unique_ptr<BaseAST> Exp_STMT;
  struct IfElse_STMT {
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> if_stmt;
    std::unique_ptr<BaseAST> else_stmt;
  };
  struct While_STMT {
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> while_stmt;
  };

  std::variant<Assign_STMT, Block_STMT, IfElse_STMT, While_STMT> stmt;
};

class OpAST : public BaseAST {
  public:
    OpAST(Op_Type op) : op(op) {}

    void Dump() const override {
      std::cout << "OpAST: " << "{" << op << "}";
    }

    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }
    Op_Type op;
};

class UnaryOpAST : public OpAST {
 public:
  UnaryOpAST(Op_Type op) : OpAST(op) {}

  void Dump() const override {
    std::cout << "UnaryOpAST { ";
    // unary_op->Dump();
    std::cout << " }";
  }

  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }
};

class BinaryOpAST : public OpAST {
  public:
    BinaryOpAST(Op_Type op) : OpAST(op) {}

    void Dump() const override {
      std::cout << "BinaryOpAST { ";
      // binary_op->Dump();
      std::cout << " }";
    }

    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }
};

class LGBinaryOpAST : public BinaryOpAST {
  public:
    LGBinaryOpAST(Op_Type op) : BinaryOpAST(op) {}

    void Dump() const override {
      std::cout << "LGBinaryOpAST { ";
      // binary_op->Dump();
      std::cout << " }";
    }

    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }
};

// PrimaryExp = ( Exp ) | Number
class PrimaryExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "PrimaryExpAST { ";
    // number->Dump();
    std::cout << " }";
  }

  // 用来指示mem的类型
  int type = -1;

  std::unique_ptr<BaseAST> mem;
 };

// UnaryExp = PrimaryExp | UnaryOp UnaryExp
class UnaryExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "UnaryExprAST { ";
      // unary_op->Dump();
      std::cout << " }";
    }

    // 用来表示mem的类型
    int type = -1;

    struct UnaryExp {
      std::unique_ptr<BaseAST> unary_op;
      std::unique_ptr<BaseAST> unary_exp;
    };

    std::variant<std::unique_ptr<BaseAST>
                , UnaryExp> mem;
    
};

class FuncCallAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }
  
  void Dump() const override {
    std::cout << "FuncCallAST { ";
    std::cout << ident;
    std::cout << ", ";
    // for (auto& param : *params) {
    //   std::cout << param;
    // }
    std::cout << " }";
  }
  

  typedef std::unique_ptr<BaseAST> param;
  typedef std::vector<param> Params;
  std::string ident;
  std::unique_ptr<Params> params;
};

// Exp = LOrExp
class ExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "ExpAST { ";
      l_or_exp->Dump();
      std::cout << " }";
    }

    std::unique_ptr<BaseAST> l_or_exp;
};

// AddExp : MulExp | AddExp AddOp MulExp
class AddExpAST : public BaseAST {
  public:
    void Accept(ASTVisitor *visitor) override {
      visitor->Visit(this);
    }

    void Dump() const override {
      std::cout << "AddExpAST { ";
      // add_exp->Dump();
      std::cout << " }";
    }

    int type = -1;
    struct AddExp {
      std::unique_ptr<BaseAST> add_exp;
      std::unique_ptr<BaseAST> add_op;
      std::unique_ptr<BaseAST> mul_exp;
    };
    std::variant<std::unique_ptr<BaseAST>
                , AddExp> mem;
};


// MulExp =  UnaryExp | MulExp MulOp UnaryExp
class MulExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "MulExpAST { ";
    if (type == 0) {
      std::get<std::unique_ptr<BaseAST>>(mem)->Dump();
    } else {
      std::get<MulExp>(mem).mul_exp->Dump();
      std::cout << ", ";
      std::get<MulExp>(mem).mul_op->Dump();
      std::cout << ", ";
      std::get<MulExp>(mem).unary_exp->Dump();
    }
    std::cout << " }";
  }

  int type = -1;
  struct MulExp {
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> mul_op;
    std::unique_ptr<BaseAST> unary_exp;
  };
  std::variant<std::unique_ptr<BaseAST>
                , MulExp> mem;
};


// RelExp = AddExp | RelExp BinaryOp AddExp
class RelExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "RelExpAST { ";
    if (type == 0) {
      std::get<std::unique_ptr<BaseAST>>(mem)->Dump();
    } else {
      std::get<RelExp>(mem).rel_exp->Dump();
      std::cout << ", ";
      std::get<RelExp>(mem).rel_op->Dump();
      std::cout << ", ";
      std::get<RelExp>(mem).add_exp->Dump();
    }
    std::cout << " }";
  }

  int type = -1;
  struct RelExp {
    std::unique_ptr<BaseAST> rel_exp;
    std::unique_ptr<BaseAST> rel_op;
    std::unique_ptr<BaseAST> add_exp;
  };
  std::variant<std::unique_ptr<BaseAST>
                , RelExp> mem;
};

// EqExp = RelExp | EqExp BinaryOp RelExp
class EqExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "EqExpAST { ";
    if (type == 0) {
      std::get<std::unique_ptr<BaseAST>>(mem)->Dump();
    } else {
      std::get<EqExp>(mem).eq_exp->Dump();
      std::cout << ", ";
      std::get<EqExp>(mem).eq_op->Dump();
      std::cout << ", ";
      std::get<EqExp>(mem).rel_exp->Dump();
    }
    std::cout << " }";
  }

  int type = -1;
  struct EqExp {
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> eq_op;
    std::unique_ptr<BaseAST> rel_exp;
  };
  std::variant<std::unique_ptr<BaseAST>
                , EqExp> mem;
};

// LAndExp = EqExp | LAndExp BinaryOp EqExp
class LAndExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "LAndExpAST { ";
    if (type == 0) {
      std::get<std::unique_ptr<BaseAST>>(mem)->Dump();
    } else {
      std::get<LAndExp>(mem).l_and_exp->Dump();
      std::cout << ", ";
      std::get<LAndExp>(mem).l_and_op->Dump();
      std::cout << ", ";
      std::get<LAndExp>(mem).eq_exp->Dump();
    }
    std::cout << " }";
  }

  int type = -1;
  struct LAndExp {
    std::unique_ptr<BaseAST> l_and_exp;
    std::unique_ptr<BaseAST> l_and_op;
    std::unique_ptr<BaseAST> eq_exp;
  };
  std::variant<std::unique_ptr<BaseAST>
                , LAndExp> mem;
};
               
// LOrExp = LAndExp | LOrExp BinaryOp LAndExp
class LOrExpAST : public BaseAST {
 public:
  void Accept(ASTVisitor *visitor) override {
    visitor->Visit(this);
  }

  void Dump() const override {
    std::cout << "LOrExpAST { ";
    if (type == 0) {
      std::get<std::unique_ptr<BaseAST>>(mem)->Dump();
    } else {
      std::get<LOrExp>(mem).l_or_exp->Dump();
      std::cout << ", ";
      std::get<LOrExp>(mem).l_or_op->Dump();
      std::cout << ", ";
      std::get<LOrExp>(mem).l_and_exp->Dump();
    }
    std::cout << " }";
  }

  int type = -1;
  struct LOrExp {
    std::unique_ptr<BaseAST> l_or_exp;
    std::unique_ptr<BaseAST> l_or_op;
    std::unique_ptr<BaseAST> l_and_exp;
  };
  std::variant<std::unique_ptr<BaseAST>
                , LOrExp> mem;
};
