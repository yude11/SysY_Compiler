%code requires { 
  // 声明在 .tab.hpp 中需要的头文件
  #include <memory>
  #include <string>

  #include "AST.h"
  #include "type.h"

  // 位置信息类型
  struct YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
  };
}

%{ 
// 复制到生成的 .tab.cpp 文件开头

#include <iostream>
#include <memory>
#include <string>
#include "AST.h"

// 声明 lexer 函数和错误处理函数（Bison 会生成 YYSTYPE 和 YYLTYPE）
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }
%locations
%define api.location.type {YYLTYPE}

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<std::unique_ptr<BaseAST>>* ast_list;
  FuncDefAST::FuncParams* func_param_list;
  FuncDefAST::FuncParam *func_param_val;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN GE LE EQ NE LAND LOR CONST IF ELSE WHILE BREAK CONTINUE VOID
%token <str_val> IDENT
%token <int_val> INT_CONST





// 非终结符的类型定义
// %type <str_val> 
%type <ast_val> FuncDef FuncType Block Stmt Number PrimaryExp Exp
%type <ast_list> FuncDefList 
%type <ast_val> LOrExp LAndExp EqExp RelExp UnaryExp AddExp MulExp UnaryOp 
%type <ast_val> AddOp MulOp RelOp EqOp LAndOp LOrOp BlockItem ConstDef Decl ConstDecl 
%type <ast_val> ConstInitVal LVal ConstExp VarDecl VarDef InitVal IfElse

%type <ast_list> BlockItemList ConstDefList VarDefList FuncRParams
%type <str_val> BType
%type <func_param_val> FuncFParam
%type <func_param_list> FuncFParams



//优先级声明
%precedence LOWER_THAN_ELSE    // 最低
%precedence ELSE               // 更高


%%

// 开始符, CompUnit 是多个函数定义的列表
// 使用 vector 结构避免递归和所有权问题
CompUnit
  : FuncDefList {
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_defs = std::move(*$1);  // 转移 vector 所有权
    delete $1;  // 释放临时的 vector 指针
    ast = std::move(comp_unit);
  }
  ;

// FuncDefList 是函数定义列表，使用 vector 收集
FuncDefList
  : FuncDef {
    auto list = new std::vector<std::unique_ptr<BaseAST>>();
    list->emplace_back($1);  // 添加第一个函数
    $$ = list;
  }
  | FuncDefList FuncDef {
    $1->emplace_back($2);  // 追加到已有列表
    $$ = $1;
  }
  ;

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : FuncType IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | FuncType IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($6);
    ast->func_params = std::unique_ptr<FuncDefAST::FuncParams>($4);
    $$ = ast;
  }
  ;

// 同上, 不再解释
FuncType
  : INT {
    $$ = new FuncTypeAST("i32");
  }
  | VOID {
    $$ = new FuncTypeAST("void");
  }
  ;

  FuncFParams
    : FuncFParam {
      auto param_list = new FuncDefAST::FuncParams();
      param_list->push_back(unique_ptr<FuncDefAST::FuncParam>($1));
      $$ = param_list;
    }
    | FuncFParams ',' FuncFParam {
      auto param_list = $1;
      param_list->push_back(unique_ptr<FuncDefAST::FuncParam>($3));
      $$ = param_list;
    }
    ;

  FuncFParam
    : BType IDENT {
      auto param = new FuncDefAST::FuncParam();
      param->type = *unique_ptr<string>($1);
      param->ident = *unique_ptr<string>($2);
      $$ = param;
    }
    ;

Block
  : '{' '}' {
    auto ast = new BlockAST();
    ast->block_items = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>(new std::vector<std::unique_ptr<BaseAST>>());
    $$ = ast;
  }
  | '{' BlockItemList '}' {
    auto ast = new BlockAST();
    ast->block_items = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>($2);
    $$ = ast;
  }
  ;

BlockItemList
  : BlockItem {
    auto ast_list = new std::vector<std::unique_ptr<BaseAST>>();
    if ($1 != nullptr) {
      ast_list->push_back(unique_ptr<BaseAST>($1));
    }
    $$ = ast_list;
  }
  | BlockItemList BlockItem {
    auto ast_list = $1;
    if ($2 != nullptr) {
      ast_list->push_back(unique_ptr<BaseAST>($2));
    }
    $$ = ast_list;
  }
  ;

BlockItem
  : Decl {
    auto ast = new BlockItemAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Stmt {
    if ($1 == nullptr) {
      $$ = nullptr;
    } else {
      auto ast = new BlockItemAST();
      ast->type = 1;
      ast->mem = unique_ptr<BaseAST>($1);
      $$ = ast;
    }
  }

Stmt
  : RETURN Exp ';' {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_RETURN;
    ast->stmt = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_RETURN;
    $$ = ast;
  }
  | LVal '=' Exp ';' {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_ASSIGN;
    StmtAST::Assign_STMT assign;
    assign.lval = std::unique_ptr<BaseAST>($1);
    assign.exp = std::unique_ptr<BaseAST>($3);
    ast->stmt = std::move(assign);
    $$ = ast;
  }
  | ';' {
    $$ = nullptr;
  } 
  | Block {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_BLOCK;
    ast->stmt = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_EXP;
    ast->stmt = std::unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | IfElse {
    $$ = $1;
  }
  | WHILE '(' Exp ')' Stmt {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_WHILE;
    StmtAST::While_STMT while1;
    while1.exp = std::unique_ptr<BaseAST>($3);
    if ($5 == nullptr) {
      while1.while_stmt = std::make_unique<NullAST>();
    } else {
      while1.while_stmt = std::unique_ptr<BaseAST>($5);
    }
    ast->stmt = std::move(while1);
    $$ = ast;
  }
  | BREAK ';' {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_BREAK;
    $$ = ast;
  }
  | CONTINUE ';' {
    auto ast = new StmtAST();
    ast->type = Stmt_Type::AST_STMT_CONTINUE;
    $$ = ast;
  }
  ;

IfElse
  : IF '(' Exp ')' Stmt %prec LOWER_THAN_ELSE {
    if ($5 == nullptr) {
      $$ = nullptr;
    } else {
      auto ast = new StmtAST();
      ast->type = Stmt_Type::AST_STMT_IF_ELSE;
      StmtAST::IfElse_STMT if_else;
      if_else.exp = std::unique_ptr<BaseAST>($3);
      if_else.if_stmt = std::unique_ptr<BaseAST>($5);
      ast->stmt = std::move(if_else);
      $$ = ast;
    }
  }
  | IF '(' Exp ')' Stmt ELSE Stmt {
    if ($5 == nullptr && $7 == nullptr) {
      $$ = nullptr;
    } else {
      auto ast = new StmtAST();
      ast->type = Stmt_Type::AST_STMT_IF_ELSE;
      StmtAST::IfElse_STMT if_else;
      if_else.exp = std::unique_ptr<BaseAST>($3);
      if ($5 == nullptr) {
        if_else.if_stmt = std::make_unique<NullAST>();
      } else {
        if_else.if_stmt = std::unique_ptr<BaseAST>($5);
      }
      if ($7 == nullptr) {
        if_else.else_stmt = std::make_unique<NullAST>();
      } else {
        if_else.else_stmt = std::unique_ptr<BaseAST>($7);
      }
      ast->stmt = std::move(if_else);
      $$ = ast;
    }
  }
  ;

Decl
  : ConstDecl {
    $$ = $1;
  }
  | VarDecl {
    $$ = $1;
  }
  ;

VarDecl
  : BType VarDef ';' {
    auto ast = new VarDeclAST();
    ast->elem_type = *($1);
    // 创建一个空的list, 并添加一个元素
    auto vec = new std::vector<std::unique_ptr<BaseAST>>();
    vec->push_back(std::unique_ptr<BaseAST>($2));
    ast->var_def_list = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>(vec);
    $$ = ast;
  }
  | BType VarDef ',' VarDefList ';' {
    auto ast = new VarDeclAST();
    ast->elem_type = *($1);
    ast->var_def_list = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>($4);
    // 在开头插入
    ast->var_def_list->insert(ast->var_def_list->begin(), 
                std::unique_ptr<BaseAST>($2));
    $$ = ast;
  }
  ;

VarDefList
  : VarDef {
    auto ast_list = new std::vector<std::unique_ptr<BaseAST>>();
    ast_list->push_back(unique_ptr<BaseAST>($1));
    $$ = ast_list;
  }
  | VarDefList ',' VarDef {
    auto ast_list = $1;
    ast_list->push_back(unique_ptr<BaseAST>($3));
    $$ = ast_list;
  }
  ;

VarDef
  : IDENT {
    auto ast = new VarDefAST();
    ast->type = 0;
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '=' InitVal {
    auto ast = new VarDefAST();
    ast->type = 1;
    ast->ident = *unique_ptr<string>($1);
    ast->var_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

InitVal
  : Exp {
    $$ = $1;
  }
  ;


ConstDecl
  : CONST BType ConstDef ';' {
    auto ast = new ConstDeclAST();
    ast->elem_type = *($2);
    // 创建一个空的list, 并添加一个元素
    auto vec = new std::vector<std::unique_ptr<BaseAST>>();
    vec->push_back(std::unique_ptr<BaseAST>($3));
    ast->const_def_list = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>(vec);
    $$ = ast;
  }
  | CONST BType ConstDef ',' ConstDefList ';' {
    auto ast = new ConstDeclAST();
    ast->elem_type = *($2);
    ast->const_def_list = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>($5);
    // 在开头插入
    ast->const_def_list->insert(ast->const_def_list->begin(), 
                std::unique_ptr<BaseAST>($3));
    $$ = ast;
  }
  ;

ConstDefList
  : ConstDef {
    auto ast_list = new std::vector<std::unique_ptr<BaseAST>>();
    ast_list->push_back(unique_ptr<BaseAST>($1));
    $$ = ast_list;
  }
  | ConstDefList ',' ConstDef {
    auto ast_list = $1;
    ast_list->push_back(unique_ptr<BaseAST>($3));
    $$ = ast_list;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->const_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

ConstInitVal
  : ConstExp {
    $$ = $1;
  }
  ;

BType 
  : INT {
    $$ = new std::string("i32");
  }
  ;

//  表达式相关规则
Exp
  : LOrExp {
    auto ast = new ExpAST();
    ast->l_or_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
    // $$ = $1;
  }

ConstExp
  : Exp {
    $$ = $1;
  }
  ;

RelExp
  : AddExp {
    auto ast = new RelExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RelExp RelOp AddExp {
    auto ast = new RelExpAST();
    ast->type = 1;
    ast->mem = RelExpAST::RelExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2), unique_ptr<BaseAST>($3)};
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    auto ast = new EqExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | EqExp EqOp RelExp {
    auto ast = new EqExpAST();
    ast->type = 1;
    ast->mem = EqExpAST::EqExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2), unique_ptr<BaseAST>($3)};
    $$ = ast;
  }
  ;

LAndExp
  : EqExp {
    auto ast = new LAndExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LAndExp LAndOp EqExp {
    auto ast = new LAndExpAST();
    ast->type = 1;
    ast->mem = LAndExpAST::LAndExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2), unique_ptr<BaseAST>($3)};
    $$ = ast;
  }
  ;

LOrExp
  : LAndExp {
    auto ast = new LOrExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp LOrOp LAndExp {
    auto ast = new LOrExpAST();
    ast->type = 1;
    ast->mem = LOrExpAST::LOrExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2), unique_ptr<BaseAST>($3)};
    $$ = ast;
  }
  ;

MulExp
  : UnaryExp {
    auto ast = new MulExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | MulExp MulOp UnaryExp {
    auto ast = new MulExpAST();
    ast->type = 1;
    ast->mem = MulExpAST::MulExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2), unique_ptr<BaseAST>($3)};
    $$ = ast;
  }

AddExp
  : MulExp {
    auto ast = new AddExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp AddOp MulExp {
    auto ast = new AddExpAST();
    ast->type = 1;
    ast->mem = AddExpAST::AddExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2), unique_ptr<BaseAST>($3)};
    $$ = ast;
  }

UnaryExp
  : PrimaryExp {
    auto ast = new UnaryExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | UnaryOp UnaryExp {
    auto ast = new UnaryExpAST();
    ast->type = 1;
    ast->mem = UnaryExpAST::UnaryExp{unique_ptr<BaseAST>($1), unique_ptr<BaseAST>($2)};
    $$ = ast;
  }
  | IDENT '(' FuncRParams ')' {
    auto ast = new FuncCallAST();
    ast->ident = *unique_ptr<string>($1);
    ast->params = std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>($3);
    $$ = ast;
  }
  | IDENT '(' ')' {
    auto ast = new FuncCallAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  ;

  FuncRParams
    : Exp {
      auto ast_list = new std::vector<std::unique_ptr<BaseAST>>();
      ast_list->push_back(unique_ptr<BaseAST>($1));
      $$ = ast_list;
    }
    | FuncRParams ',' Exp {
      auto ast_list = $1;
      ast_list->push_back(unique_ptr<BaseAST>($3));
      $$ = ast_list;
    }
    ;

PrimaryExp
  : '(' Exp ')' {
    auto ast = new PrimaryExpAST();
    ast->type = 0;
    ast->mem = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | Number {
    auto ast = new PrimaryExpAST();
    ast->type = 1;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LVal {
    auto ast = new PrimaryExpAST();
    ast->type = 2;
    ast->mem = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

EqOp
  : EQ {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_EQ);
    $$ = ast;
  }
  | NE {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_NE);
    $$ = ast;
  }
  ;

RelOp
  : '<' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_LT);
    $$ = ast;
  }
  | '>' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_GT);
    $$ = ast;
  }
  | LE {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_LE);
    $$ = ast;
  }
  | GE {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_GE);
    $$ = ast;
  }
  ;

LAndOp
  : LAND {
    auto ast = new LGBinaryOpAST(Op_Type::AST_BINARY_OP_LA);
    $$ = ast;
  }
  ;
  
LOrOp
  : LOR {
    auto ast = new LGBinaryOpAST(Op_Type::AST_BINARY_OP_LO);
    $$ = ast;
  }
  ;

UnaryOp
  : '+' {
    auto ast = new UnaryOpAST(Op_Type::AST_UNARY_OP_POS);
    $$ = ast;
  }
  | '-' {
    auto ast = new UnaryOpAST(Op_Type::AST_UNARY_OP_NEG);
    $$ = ast;
  }
  | '!' {
    auto ast = new UnaryOpAST(Op_Type::AST_UNARY_OP_NOT);
    $$ = ast;
  }
  ;

MulOp
  : '*' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_MUL);
    $$ = ast;
  }
  | '/' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_DIV);
    $$ = ast;
  }
  | '%' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_MOD);
    $$ = ast;
  }
  ;

AddOp
  : '+' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_ADD);
    $$ = ast;
  }
  | '-' {
    auto ast = new BinaryOpAST(Op_Type::AST_BINARY_OP_SUB);
    $$ = ast;
  }
  ;

LVal
  : IDENT {
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  ;

Number
  : INT_CONST {
    auto ast = new NumberAST();
    ast->num = $1;
    $$ = ast;
  }
  ;

%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  extern YYLTYPE yylloc;
  cerr << yylloc.first_line << ":" << yylloc.first_column << ": error: " << s << endl;
}