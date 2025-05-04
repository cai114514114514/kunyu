/**
 * 坤舆编程语言 - 抽象语法树节点实现
 */

#include "../includes/ast.h"
#include <stdlib.h>
#include <string.h>

/**
 * 析构函数声明
 */
static void destroy_program(AstNode *node);
static void destroy_expression_stmt(AstNode *node);
static void destroy_var_decl(AstNode *node);
static void destroy_block(AstNode *node);
static void destroy_if(AstNode *node);
static void destroy_loop(AstNode *node);
static void destroy_function(AstNode *node);
static void destroy_return(AstNode *node);
static void destroy_print(AstNode *node);
static void destroy_literal(AstNode *node);
static void destroy_variable(AstNode *node);
static void destroy_binary(AstNode *node);
static void destroy_unary(AstNode *node);
static void destroy_call(AstNode *node);
static void destroy_grouping(AstNode *node);
static void destroy_assign(AstNode *node);

/**
 * 创建程序节点
 */
AstNode* create_program() {
    Program *program = (Program *)malloc(sizeof(Program));
    if (program == NULL) {
        return NULL;
    }
    
    program->base.type = NODE_PROGRAM;
    program->base.line = 0;
    program->base.column = 0;
    program->base.destructor = destroy_program;
    
    program->statements = NULL;
    program->stmt_count = 0;
    
    return (AstNode *)program;
}

/**
 * 添加语句到程序
 */
void program_add_statement(AstNode *program, AstNode *stmt) {
    if (program == NULL || stmt == NULL || program->type != NODE_PROGRAM) {
        return;
    }
    
    Program *prog = (Program *)program;
    
    // 重新分配内存以容纳新的语句
    AstNode **new_statements = (AstNode **)realloc(
        prog->statements, 
        sizeof(AstNode *) * (prog->stmt_count + 1)
    );
    
    if (new_statements == NULL) {
        return;
    }
    
    prog->statements = new_statements;
    prog->statements[prog->stmt_count] = stmt;
    prog->stmt_count++;
}

/**
 * 创建表达式语句
 */
AstNode* create_expression_stmt(AstNode *expr) {
    if (expr == NULL) {
        return NULL;
    }
    
    ExpressionStmt *stmt = (ExpressionStmt *)malloc(sizeof(ExpressionStmt));
    if (stmt == NULL) {
        return NULL;
    }
    
    stmt->base.base.type = NODE_PROGRAM;
    stmt->base.base.line = expr->line;
    stmt->base.base.column = expr->column;
    stmt->base.base.destructor = destroy_expression_stmt;
    
    stmt->base.stmt_type = STMT_EXPRESSION;
    stmt->expr = expr;
    
    return (AstNode *)stmt;
}

/**
 * 创建变量声明
 */
AstNode* create_var_decl(const char *name, AstNode *initializer, bool is_constant) {
    VarDeclStmt *decl = (VarDeclStmt *)malloc(sizeof(VarDeclStmt));
    if (decl == NULL) {
        return NULL;
    }
    
    decl->base.base.type = NODE_VARDECL;
    decl->base.base.line = initializer ? initializer->line : 0;
    decl->base.base.column = initializer ? initializer->column : 0;
    decl->base.base.destructor = destroy_var_decl;
    
    decl->base.stmt_type = STMT_VAR_DECL;
    
    // 复制变量名
    decl->name = strdup(name);
    if (decl->name == NULL) {
        free(decl);
        return NULL;
    }
    
    decl->initializer = initializer;
    decl->is_constant = is_constant;
    
    return (AstNode *)decl;
}

/**
 * 创建代码块
 */
AstNode* create_block(AstNode **statements, int stmt_count) {
    BlockStmt *block = (BlockStmt *)malloc(sizeof(BlockStmt));
    if (block == NULL) {
        return NULL;
    }
    
    block->base.base.type = NODE_BLOCK;
    block->base.base.line = 0;
    block->base.base.column = 0;
    block->base.base.destructor = destroy_block;
    
    block->base.stmt_type = STMT_BLOCK;
    
    // 复制语句数组
    if (stmt_count > 0) {
        block->statements = (AstNode **)malloc(sizeof(AstNode *) * stmt_count);
        if (block->statements == NULL) {
            free(block);
            return NULL;
        }
        
        for (int i = 0; i < stmt_count; i++) {
            block->statements[i] = statements[i];
        }
    } else {
        block->statements = NULL;
    }
    
    block->stmt_count = stmt_count;
    
    return (AstNode *)block;
}

/**
 * 创建条件语句
 */
AstNode* create_if(AstNode *condition, AstNode *then_branch, AstNode *else_branch) {
    if (condition == NULL || then_branch == NULL) {
        return NULL;
    }
    
    IfStmt *stmt = (IfStmt *)malloc(sizeof(IfStmt));
    if (stmt == NULL) {
        return NULL;
    }
    
    stmt->base.base.type = NODE_IF;
    stmt->base.base.line = condition->line;
    stmt->base.base.column = condition->column;
    stmt->base.base.destructor = destroy_if;
    
    stmt->base.stmt_type = STMT_IF;
    
    stmt->condition = condition;
    stmt->then_branch = then_branch;
    stmt->else_branch = else_branch;
    
    return (AstNode *)stmt;
}

/**
 * 创建循环语句
 */
AstNode* create_loop(AstNode *condition, AstNode *body) {
    if (condition == NULL || body == NULL) {
        return NULL;
    }
    
    LoopStmt *stmt = (LoopStmt *)malloc(sizeof(LoopStmt));
    if (stmt == NULL) {
        return NULL;
    }
    
    stmt->base.base.type = NODE_LOOP;
    stmt->base.base.line = condition->line;
    stmt->base.base.column = condition->column;
    stmt->base.base.destructor = destroy_loop;
    
    stmt->base.stmt_type = STMT_LOOP;
    
    stmt->condition = condition;
    stmt->body = body;
    
    return (AstNode *)stmt;
}

/**
 * 创建函数声明
 */
AstNode* create_function(const char *name, char **params, int param_count, AstNode *body) {
    if (body == NULL) {
        return NULL;
    }
    
    FunctionStmt *stmt = (FunctionStmt *)malloc(sizeof(FunctionStmt));
    if (stmt == NULL) {
        return NULL;
    }
    
    stmt->base.base.type = NODE_FUNCDECL;
    stmt->base.base.line = body->line;
    stmt->base.base.column = body->column;
    stmt->base.base.destructor = destroy_function;
    
    stmt->base.stmt_type = STMT_FUNCTION;
    
    // 复制函数名
    stmt->name = strdup(name);
    if (stmt->name == NULL) {
        free(stmt);
        return NULL;
    }
    
    // 复制参数名数组
    if (param_count > 0) {
        stmt->params = (char **)malloc(sizeof(char *) * param_count);
        if (stmt->params == NULL) {
            free(stmt->name);
            free(stmt);
            return NULL;
        }
        
        for (int i = 0; i < param_count; i++) {
            stmt->params[i] = strdup(params[i]);
            if (stmt->params[i] == NULL) {
                // 清理已分配的内存
                for (int j = 0; j < i; j++) {
                    free(stmt->params[j]);
                }
                free(stmt->params);
                free(stmt->name);
                free(stmt);
                return NULL;
            }
        }
    } else {
        stmt->params = NULL;
    }
    
    stmt->param_count = param_count;
    stmt->body = body;
    
    return (AstNode *)stmt;
}

/**
 * 创建返回语句
 */
AstNode* create_return(AstNode *value) {
    ReturnStmt *stmt = (ReturnStmt *)malloc(sizeof(ReturnStmt));
    if (stmt == NULL) {
        return NULL;
    }
    
    stmt->base.base.type = NODE_RETURN;
    stmt->base.base.line = value ? value->line : 0;
    stmt->base.base.column = value ? value->column : 0;
    stmt->base.base.destructor = destroy_return;
    
    stmt->base.stmt_type = STMT_RETURN;
    
    stmt->value = value; // 返回值可以为NULL
    
    return (AstNode *)stmt;
}

/**
 * 创建输出语句
 */
AstNode* create_print(AstNode *value) {
    if (value == NULL) {
        return NULL;
    }
    
    PrintStmt *stmt = (PrintStmt *)malloc(sizeof(PrintStmt));
    if (stmt == NULL) {
        return NULL;
    }
    
    stmt->base.base.type = NODE_PRINT;
    stmt->base.base.line = value->line;
    stmt->base.base.column = value->column;
    stmt->base.base.destructor = destroy_print;
    
    stmt->base.stmt_type = STMT_PRINT;
    
    stmt->value = value;
    
    return (AstNode *)stmt;
}

/**
 * 创建字面量表达式
 */
AstNode* create_literal(KunyuTokenType token_type, const char *value) {
    LiteralExpr *expr = (LiteralExpr *)malloc(sizeof(LiteralExpr));
    if (expr == NULL) {
        return NULL;
    }
    
    expr->base.base.type = NODE_LITERAL;
    expr->base.base.line = 0;
    expr->base.base.column = 0;
    expr->base.base.destructor = destroy_literal;
    
    expr->base.expr_type = EXPR_LITERAL;
    
    expr->token_type = token_type;
    
    // 复制值
    expr->value = strdup(value);
    if (expr->value == NULL) {
        free(expr);
        return NULL;
    }
    
    return (AstNode *)expr;
}

/**
 * 创建变量引用表达式
 */
AstNode* create_variable(const char *name) {
    VariableExpr *expr = (VariableExpr *)malloc(sizeof(VariableExpr));
    if (expr == NULL) {
        return NULL;
    }
    
    expr->base.base.type = NODE_IDENTIFIER;
    expr->base.base.line = 0;
    expr->base.base.column = 0;
    expr->base.base.destructor = destroy_variable;
    
    expr->base.expr_type = EXPR_VARIABLE;
    
    // 复制变量名
    expr->name = strdup(name);
    if (expr->name == NULL) {
        free(expr);
        return NULL;
    }
    
    return (AstNode *)expr;
}

/**
 * 创建二元表达式
 */
AstNode* create_binary(AstNode *left, BinaryOpType op, AstNode *right) {
    if (left == NULL || right == NULL) {
        return NULL;
    }
    
    BinaryExpr *expr = (BinaryExpr *)malloc(sizeof(BinaryExpr));
    if (expr == NULL) {
        return NULL;
    }
    
    expr->base.base.type = NODE_BINARY;
    expr->base.base.line = left->line;
    expr->base.base.column = left->column;
    expr->base.base.destructor = destroy_binary;
    
    expr->base.expr_type = EXPR_BINARY;
    
    expr->op = op;
    expr->left = left;
    expr->right = right;
    
    return (AstNode *)expr;
}

/**
 * 创建一元表达式
 */
AstNode* create_unary(UnaryOpType op, AstNode *operand) {
    if (operand == NULL) {
        return NULL;
    }
    
    UnaryExpr *expr = (UnaryExpr *)malloc(sizeof(UnaryExpr));
    if (expr == NULL) {
        return NULL;
    }
    
    expr->base.base.type = NODE_UNARY;
    expr->base.base.line = operand->line;
    expr->base.base.column = operand->column;
    expr->base.base.destructor = destroy_unary;
    
    expr->base.expr_type = EXPR_UNARY;
    
    expr->op = op;
    expr->operand = operand;
    
    return (AstNode *)expr;
}

/**
 * 创建函数调用表达式
 */
AstNode* create_call(const char *name, AstNode **args, int arg_count) {
    CallExpr *expr = (CallExpr *)malloc(sizeof(CallExpr));
    if (expr == NULL) {
        return NULL;
    }
    
    expr->base.base.type = NODE_CALL;
    expr->base.base.line = 0;
    expr->base.base.column = 0;
    expr->base.base.destructor = destroy_call;
    
    expr->base.expr_type = EXPR_CALL;
    
    // 复制函数名
    expr->name = strdup(name);
    if (expr->name == NULL) {
        free(expr);
        return NULL;
    }
    
    // 复制参数数组
    if (arg_count > 0) {
        expr->args = (AstNode **)malloc(sizeof(AstNode *) * arg_count);
        if (expr->args == NULL) {
            free(expr->name);
            free(expr);
            return NULL;
        }
        
        for (int i = 0; i < arg_count; i++) {
            expr->args[i] = args[i];
        }
    } else {
        expr->args = NULL;
    }
    
    expr->arg_count = arg_count;
    
    return (AstNode *)expr;
}

/**
 * 创建分组表达式
 */
AstNode* create_grouping(AstNode *expr) {
    if (expr == NULL) {
        return NULL;
    }
    
    GroupingExpr *grouping = (GroupingExpr *)malloc(sizeof(GroupingExpr));
    if (grouping == NULL) {
        return NULL;
    }
    
    grouping->base.base.type = NODE_GROUPING;
    grouping->base.base.line = expr->line;
    grouping->base.base.column = expr->column;
    grouping->base.base.destructor = destroy_grouping;
    
    grouping->base.expr_type = EXPR_GROUPING;
    
    grouping->expr = expr;
    
    return (AstNode *)grouping;
}

/**
 * 创建赋值表达式
 */
AstNode* create_assign(const char *name, AstNode *value) {
    if (value == NULL) {
        return NULL;
    }
    
    AssignExpr *expr = (AssignExpr *)malloc(sizeof(AssignExpr));
    if (expr == NULL) {
        return NULL;
    }
    
    expr->base.base.type = NODE_ASSIGN;
    expr->base.base.line = value->line;
    expr->base.base.column = value->column;
    expr->base.base.destructor = destroy_assign;
    
    expr->base.expr_type = EXPR_ASSIGN;
    
    // 复制变量名
    expr->name = strdup(name);
    if (expr->name == NULL) {
        free(expr);
        return NULL;
    }
    
    expr->value = value;
    
    return (AstNode *)expr;
}

/**
 * 释放AST节点及其子节点
 */
void ast_free(AstNode *node) {
    if (node == NULL) {
        return;
    }
    
    // 调用节点的析构函数
    if (node->destructor != NULL) {
        node->destructor(node);
    }
    
    free(node);
}

/**
 * 析构函数实现
 */

static void destroy_program(AstNode *node) {
    Program *program = (Program *)node;
    
    // 释放所有语句
    for (int i = 0; i < program->stmt_count; i++) {
        ast_free(program->statements[i]);
    }
    
    // 释放语句数组
    free(program->statements);
}

static void destroy_expression_stmt(AstNode *node) {
    ExpressionStmt *stmt = (ExpressionStmt *)node;
    
    // 释放表达式
    ast_free(stmt->expr);
}

static void destroy_var_decl(AstNode *node) {
    VarDeclStmt *decl = (VarDeclStmt *)node;
    
    // 释放变量名
    free(decl->name);
    
    // 释放初始值表达式
    if (decl->initializer != NULL) {
        ast_free(decl->initializer);
    }
}

static void destroy_block(AstNode *node) {
    BlockStmt *block = (BlockStmt *)node;
    
    // 释放所有语句
    for (int i = 0; i < block->stmt_count; i++) {
        ast_free(block->statements[i]);
    }
    
    // 释放语句数组
    free(block->statements);
}

static void destroy_if(AstNode *node) {
    IfStmt *stmt = (IfStmt *)node;
    
    // 释放条件表达式
    ast_free(stmt->condition);
    
    // 释放then分支
    ast_free(stmt->then_branch);
    
    // 释放else分支(如果有)
    if (stmt->else_branch != NULL) {
        ast_free(stmt->else_branch);
    }
}

static void destroy_loop(AstNode *node) {
    LoopStmt *stmt = (LoopStmt *)node;
    
    // 释放条件表达式
    ast_free(stmt->condition);
    
    // 释放循环体
    ast_free(stmt->body);
}

static void destroy_function(AstNode *node) {
    FunctionStmt *stmt = (FunctionStmt *)node;
    
    // 释放函数名
    free(stmt->name);
    
    // 释放参数名
    for (int i = 0; i < stmt->param_count; i++) {
        free(stmt->params[i]);
    }
    
    // 释放参数数组
    free(stmt->params);
    
    // 释放函数体
    ast_free(stmt->body);
}

static void destroy_return(AstNode *node) {
    ReturnStmt *stmt = (ReturnStmt *)node;
    
    // 释放返回值表达式(如果有)
    if (stmt->value != NULL) {
        ast_free(stmt->value);
    }
}

static void destroy_print(AstNode *node) {
    PrintStmt *stmt = (PrintStmt *)node;
    
    // 释放输出值表达式
    ast_free(stmt->value);
}

static void destroy_literal(AstNode *node) {
    LiteralExpr *expr = (LiteralExpr *)node;
    
    // 释放值
    free(expr->value);
}

static void destroy_variable(AstNode *node) {
    VariableExpr *expr = (VariableExpr *)node;
    
    // 释放变量名
    free(expr->name);
}

static void destroy_binary(AstNode *node) {
    BinaryExpr *expr = (BinaryExpr *)node;
    
    // 释放左操作数
    ast_free(expr->left);
    
    // 释放右操作数
    ast_free(expr->right);
}

static void destroy_unary(AstNode *node) {
    UnaryExpr *expr = (UnaryExpr *)node;
    
    // 释放操作数
    ast_free(expr->operand);
}

static void destroy_call(AstNode *node) {
    CallExpr *expr = (CallExpr *)node;
    
    // 释放函数名
    free(expr->name);
    
    // 释放参数表达式
    for (int i = 0; i < expr->arg_count; i++) {
        ast_free(expr->args[i]);
    }
    
    // 释放参数数组
    free(expr->args);
}

static void destroy_grouping(AstNode *node) {
    GroupingExpr *expr = (GroupingExpr *)node;
    
    // 释放表达式
    ast_free(expr->expr);
}

static void destroy_assign(AstNode *node) {
    AssignExpr *expr = (AssignExpr *)node;
    
    // 释放变量名
    free(expr->name);
    
    // 释放值表达式
    ast_free(expr->value);
} 