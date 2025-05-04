/**
 * 坤舆编程语言 - 抽象语法树节点定义
 */

#ifndef KUNYU_AST_H
#define KUNYU_AST_H

#include "kunyu.h"

/**
 * 表达式类型
 */
typedef enum {
    EXPR_LITERAL,        // 字面量
    EXPR_VARIABLE,       // 变量引用
    EXPR_BINARY,         // 二元表达式
    EXPR_UNARY,          // 一元表达式
    EXPR_CALL,           // 函数调用
    EXPR_GROUPING,       // 分组表达式
    EXPR_ASSIGN          // 赋值表达式
} ExprType;

/**
 * 语句类型
 */
typedef enum {
    STMT_EXPRESSION,     // 表达式语句
    STMT_VAR_DECL,       // 变量声明
    STMT_BLOCK,          // 代码块
    STMT_IF,             // 条件语句
    STMT_LOOP,           // 循环语句
    STMT_FUNCTION,       // 函数声明
    STMT_RETURN,         // 返回语句
    STMT_PRINT           // 输出语句
} StmtType;

/**
 * 二元运算符类型
 */
typedef enum {
    OP_ADD,              // +
    OP_SUB,              // -
    OP_MUL,              // *
    OP_DIV,              // /
    OP_MOD,              // %
    OP_EQ,               // ==
    OP_NE,               // !=
    OP_LT,               // <
    OP_LE,               // <=
    OP_GT,               // >
    OP_GE,               // >=
    OP_AND,              // &&
    OP_OR                // ||
} BinaryOpType;

/**
 * 一元运算符类型
 */
typedef enum {
    OP_NEG,              // -
    OP_NOT               // !
} UnaryOpType;

/**
 * 抽象语法树节点基类
 */
typedef struct AstNode {
    NodeType type;                       // 节点类型
    int line;                            // 行号
    int column;                          // 列号
    void (*destructor)(struct AstNode*); // 析构函数
} AstNode;

/**
 * 表达式节点基类
 */
typedef struct {
    AstNode base;                        // 基类
    ExprType expr_type;                  // 表达式类型
} ExprNode;

/**
 * 语句节点基类
 */
typedef struct {
    AstNode base;                        // 基类
    StmtType stmt_type;                  // 语句类型
} StmtNode;

/**
 * 字面量表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    KunyuTokenType token_type;           // 标记类型(数字、字符串等)
    char *value;                         // 值
} LiteralExpr;

/**
 * 变量引用表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    char *name;                          // 变量名
} VariableExpr;

/**
 * 二元表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    BinaryOpType op;                     // 运算符
    struct AstNode *left;                // 左操作数
    struct AstNode *right;               // 右操作数
} BinaryExpr;

/**
 * 一元表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    UnaryOpType op;                      // 运算符
    struct AstNode *operand;             // 操作数
} UnaryExpr;

/**
 * 函数调用表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    char *name;                          // 函数名
    struct AstNode **args;               // 参数列表
    int arg_count;                       // 参数数量
} CallExpr;

/**
 * 分组表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    struct AstNode *expr;                // 表达式
} GroupingExpr;

/**
 * 赋值表达式
 */
typedef struct {
    ExprNode base;                       // 基类
    char *name;                          // 变量名
    struct AstNode *value;               // 值
} AssignExpr;

/**
 * 表达式语句
 */
typedef struct {
    StmtNode base;                       // 基类
    struct AstNode *expr;                // 表达式
} ExpressionStmt;

/**
 * 变量声明
 */
typedef struct {
    StmtNode base;                       // 基类
    char *name;                          // 变量名
    struct AstNode *initializer;         // 初始值
    bool is_constant;                    // 是否是常量
} VarDeclStmt;

/**
 * 代码块
 */
typedef struct {
    StmtNode base;                       // 基类
    struct AstNode **statements;         // 语句列表
    int stmt_count;                      // 语句数量
} BlockStmt;

/**
 * 条件语句
 */
typedef struct {
    StmtNode base;                       // 基类
    struct AstNode *condition;           // 条件
    struct AstNode *then_branch;         // 满足条件时执行的语句
    struct AstNode *else_branch;         // 不满足条件时执行的语句
} IfStmt;

/**
 * 循环语句
 */
typedef struct {
    StmtNode base;                       // 基类
    struct AstNode *condition;           // 条件
    struct AstNode *body;                // 循环体
} LoopStmt;

/**
 * 函数声明
 */
typedef struct {
    StmtNode base;                       // 基类
    char *name;                          // 函数名
    char **params;                       // 参数名列表
    int param_count;                     // 参数数量
    struct AstNode *body;                // 函数体
} FunctionStmt;

/**
 * 返回语句
 */
typedef struct {
    StmtNode base;                       // 基类
    struct AstNode *value;               // 返回值
} ReturnStmt;

/**
 * 输出语句
 */
typedef struct {
    StmtNode base;                       // 基类
    struct AstNode *value;               // 输出值
} PrintStmt;

/**
 * 程序节点 - 根节点
 */
typedef struct {
    AstNode base;                        // 基类
    struct AstNode **statements;         // 语句列表
    int stmt_count;                      // 语句数量
} Program;

/**
 * 创建程序节点
 */
AstNode* create_program();

/**
 * 添加语句到程序
 */
void program_add_statement(AstNode *program, AstNode *stmt);

/**
 * 创建表达式语句
 */
AstNode* create_expression_stmt(AstNode *expr);

/**
 * 创建变量声明
 */
AstNode* create_var_decl(const char *name, AstNode *initializer, bool is_constant);

/**
 * 创建代码块
 */
AstNode* create_block(AstNode **statements, int stmt_count);

/**
 * 创建条件语句
 */
AstNode* create_if(AstNode *condition, AstNode *then_branch, AstNode *else_branch);

/**
 * 创建循环语句
 */
AstNode* create_loop(AstNode *condition, AstNode *body);

/**
 * 创建函数声明
 */
AstNode* create_function(const char *name, char **params, int param_count, AstNode *body);

/**
 * 创建返回语句
 */
AstNode* create_return(AstNode *value);

/**
 * 创建输出语句
 */
AstNode* create_print(AstNode *value);

/**
 * 创建字面量表达式
 */
AstNode* create_literal(KunyuTokenType token_type, const char *value);

/**
 * 创建变量引用表达式
 */
AstNode* create_variable(const char *name);

/**
 * 创建二元表达式
 */
AstNode* create_binary(AstNode *left, BinaryOpType op, AstNode *right);

/**
 * 创建一元表达式
 */
AstNode* create_unary(UnaryOpType op, AstNode *operand);

/**
 * 创建函数调用表达式
 */
AstNode* create_call(const char *name, AstNode **args, int arg_count);

/**
 * 创建分组表达式
 */
AstNode* create_grouping(AstNode *expr);

/**
 * 创建赋值表达式
 */
AstNode* create_assign(const char *name, AstNode *value);

/**
 * 释放AST节点
 */
void ast_free(AstNode *node);

#endif /* KUNYU_AST_H */ 