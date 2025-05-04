/**
 * 坤舆编程语言 - 语法分析器
 * 将标记流转换为抽象语法树
 */

#include "../includes/kunyu.h"
#include "../includes/ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * 语法分析器上下文
 */
typedef struct {
    Token *tokens;           // 标记数组
    size_t token_count;      // 标记数量
    size_t current;          // 当前标记位置
    KunyuError error;        // 错误信息
} ParserContext;

// 全局语法分析器上下文
static ParserContext parser;

/**
 * 获取当前标记
 */
static Token* current_token() {
    if (parser.current >= parser.token_count) {
        return NULL;
    }
    return &parser.tokens[parser.current];
}

/**
 * 获取下一个标记但不前进
 */
static Token* peek_token() {
    if (parser.current + 1 >= parser.token_count) {
        return NULL;
    }
    return &parser.tokens[parser.current + 1];
}

/**
 * 前进到下一个标记
 */
static Token* advance() {
    Token* token = current_token();
    if (token != NULL) {
        parser.current++;
    }
    return token;
}

/**
 * 检查当前标记是否是指定类型
 */
static bool check(KunyuTokenType type) {
    Token* token = current_token();
    if (token == NULL) {
        return false;
    }
    return token->type == type;
}

/**
 * 检查当前标记是否是指定的关键字
 */
static bool check_keyword(const char* keyword) {
    Token* token = current_token();
    if (token == NULL || token->type != KUNYU_TOKEN_KEYWORD) {
        return false;
    }
    return strcmp(token->value, keyword) == 0;
}

/**
 * 如果当前标记是指定类型，则前进到下一个标记并返回true
 */
static bool match(KunyuTokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

/**
 * 如果当前标记是指定的关键字，则前进到下一个标记并返回true
 */
static bool match_keyword(const char* keyword) {
    if (check_keyword(keyword)) {
        advance();
        return true;
    }
    return false;
}

/**
 * 预期当前标记是指定类型，如果是则前进到下一个标记，否则报错
 */
static Token* expect(KunyuTokenType type, const char* message) {
    if (check(type)) {
        return advance();
    }
    
    Token* token = current_token();
    parser.error.code = KUNYU_ERROR_PARSER;
    parser.error.line = token ? token->line : 0;
    parser.error.column = token ? token->column : 0;
    snprintf(parser.error.message, sizeof(parser.error.message), "%s", message);
    
    return NULL;
}

/**
 * 预期当前标记是指定的关键字，如果是则前进到下一个标记，否则报错
 */
static Token* expect_keyword(const char* keyword, const char* message) {
    if (check_keyword(keyword)) {
        return advance();
    }
    
    Token* token = current_token();
    parser.error.code = KUNYU_ERROR_PARSER;
    parser.error.line = token ? token->line : 0;
    parser.error.column = token ? token->column : 0;
    snprintf(parser.error.message, sizeof(parser.error.message), "%s", message);
    
    return NULL;
}

// 前置声明解析函数
static AstNode* parse_expression();
static AstNode* parse_statement();
static AstNode* parse_print_stmt();
static AstNode* parse_var_decl();
static AstNode* parse_if_stmt();
static AstNode* parse_loop_stmt();
static AstNode* parse_function_decl();
static AstNode* parse_return_stmt();
static AstNode* parse_block();
static AstNode* parse_primary();
static AstNode* parse_binary_expr(AstNode* left);
static AstNode* parse_function_call(const char* name);

/**
 * 解析一个程序（顶层语句序列）
 */
static AstNode* parse_program() {
    AstNode* program = create_program();
    if (program == NULL) {
        parser.error.code = KUNYU_ERROR_MEMORY;
        snprintf(parser.error.message, sizeof(parser.error.message), 
                 "内存分配失败，无法创建程序节点");
        return NULL;
    }
    
    // 跳过前导换行
    while (match(KUNYU_TOKEN_NEWLINE)) {}
    
    // 解析所有顶层语句
    while (parser.current < parser.token_count && !check(KUNYU_TOKEN_EOF)) {
        AstNode* stmt = parse_statement();
        if (stmt != NULL) {
            program_add_statement(program, stmt);
        } else if (parser.error.code != KUNYU_OK) {
            // 发生错误
            ast_free(program);
            return NULL;
        }
        
        // 跳过语句后的换行
        while (match(KUNYU_TOKEN_NEWLINE)) {}
    }
    
    return program;
}

/**
 * 解析一个语句
 */
static AstNode* parse_statement() {
    // 尝试解析输出语句
    if (check_keyword("输出")) {
        return parse_print_stmt();
    }
    
    // 尝试解析变量声明
    if (check_keyword("变量") || check_keyword("常量")) {
        return parse_var_decl();
    }
    
    // 尝试解析条件语句
    if (check_keyword("如果")) {
        return parse_if_stmt();
    }
    
    // 尝试解析循环语句
    if (check_keyword("循环")) {
        return parse_loop_stmt();
    }
    
    // 尝试解析函数声明
    if (check_keyword("函数")) {
        return parse_function_decl();
    }
    
    // 尝试解析返回语句
    if (check_keyword("返回")) {
        return parse_return_stmt();
    }
    
    // 默认尝试解析表达式语句
    AstNode* expr = parse_expression();
    if (expr == NULL) {
        return NULL;
    }
    
    // 表达式语句需要以分号结尾
    if (!match(KUNYU_TOKEN_DELIMITER) || strcmp(current_token()->value, ";") != 0) {
        ast_free(expr);
        parser.error.code = KUNYU_ERROR_PARSER;
        Token* token = current_token();
        parser.error.line = token ? token->line : 0;
        parser.error.column = token ? token->column : 0;
        snprintf(parser.error.message, sizeof(parser.error.message), 
                 "预期';'作为表达式语句的结束");
        return NULL;
    }
    
    advance(); // 跳过分号
    
    return create_expression_stmt(expr);
}

/**
 * 解析输出语句
 */
static AstNode* parse_print_stmt() {
    // 匹配 "输出" 关键字
    Token* keyword = expect_keyword("输出", "预期'输出'关键字");
    if (keyword == NULL) {
        return NULL;
    }
    
    // 解析输出表达式
    AstNode* expr = parse_expression();
    if (expr == NULL) {
        return NULL;
    }
    
    // 输出语句需要以分号结尾
    Token* semicolon = expect(KUNYU_TOKEN_DELIMITER, "预期';'作为输出语句的结束");
    if (semicolon == NULL || strcmp(semicolon->value, ";") != 0) {
        ast_free(expr);
        return NULL;
    }
    
    return create_print(expr);
}

/**
 * 解析变量声明
 */
static AstNode* parse_var_decl() {
    // 匹配 "变量" 或 "常量" 关键字
    bool is_constant = check_keyword("常量");
    advance(); // 跳过变量/常量关键字
    
    // 获取变量名
    Token* name = expect(KUNYU_TOKEN_IDENTIFIER, "预期变量名标识符");
    if (name == NULL) {
        return NULL;
    }
    
    // 匹配赋值运算符 "="
    Token* equals = expect(KUNYU_TOKEN_OPERATOR, "预期'='赋值运算符");
    if (equals == NULL || strcmp(equals->value, "=") != 0) {
        return NULL;
    }
    
    // 解析初始值表达式
    AstNode* initializer = parse_expression();
    if (initializer == NULL) {
        return NULL;
    }
    
    // 变量声明需要以分号结尾
    Token* semicolon = expect(KUNYU_TOKEN_DELIMITER, "预期';'作为变量声明的结束");
    if (semicolon == NULL || strcmp(semicolon->value, ";") != 0) {
        ast_free(initializer);
        return NULL;
    }
    
    return create_var_decl(name->value, initializer, is_constant);
}

/**
 * 解析条件语句
 */
static AstNode* parse_if_stmt() {
    // 匹配 "如果" 关键字
    Token* if_keyword = expect_keyword("如果", "预期'如果'关键字");
    if (if_keyword == NULL) {
        return NULL;
    }
    
    // 匹配左括号 "("
    Token* lparen = expect(KUNYU_TOKEN_DELIMITER, "预期'('开始条件表达式");
    if (lparen == NULL || strcmp(lparen->value, "(") != 0) {
        return NULL;
    }
    
    // 解析条件表达式
    AstNode* condition = parse_expression();
    if (condition == NULL) {
        return NULL;
    }
    
    // 匹配右括号 ")"
    Token* rparen = expect(KUNYU_TOKEN_DELIMITER, "预期')'结束条件表达式");
    if (rparen == NULL || strcmp(rparen->value, ")") != 0) {
        ast_free(condition);
        return NULL;
    }
    
    // 匹配左大括号 "{"
    Token* lbrace = expect(KUNYU_TOKEN_DELIMITER, "预期'{'开始条件分支代码块");
    if (lbrace == NULL || strcmp(lbrace->value, "{") != 0) {
        ast_free(condition);
        return NULL;
    }
    
    // 解析条件为真时的代码块
    AstNode* then_branch = parse_block();
    if (then_branch == NULL) {
        ast_free(condition);
        return NULL;
    }
    
    // 检查是否有 "否则" 分支
    AstNode* else_branch = NULL;
    if (match_keyword("否则")) {
        // 如果有 "否则" 关键字，则解析 "否则" 分支
        
        // 检查是否是 "否则如果" 结构
        if (check_keyword("如果")) {
            // "否则如果"结构将递归解析为一个新的if语句
            else_branch = parse_if_stmt();
            if (else_branch == NULL) {
                ast_free(condition);
                ast_free(then_branch);
                return NULL;
            }
        } else {
            // 普通的 "否则" 分支，匹配左大括号 "{"
            Token* else_lbrace = expect(KUNYU_TOKEN_DELIMITER, "预期'{'开始否则分支代码块");
            if (else_lbrace == NULL || strcmp(else_lbrace->value, "{") != 0) {
                ast_free(condition);
                ast_free(then_branch);
                return NULL;
            }
            
            // 解析 "否则" 分支的代码块
            else_branch = parse_block();
            if (else_branch == NULL) {
                ast_free(condition);
                ast_free(then_branch);
                return NULL;
            }
        }
    }
    
    return create_if(condition, then_branch, else_branch);
}

/**
 * 解析循环语句
 */
static AstNode* parse_loop_stmt() {
    // 匹配 "循环" 关键字
    Token* loop_keyword = expect_keyword("循环", "预期'循环'关键字");
    if (loop_keyword == NULL) {
        return NULL;
    }
    
    // 匹配左括号 "("
    Token* lparen = expect(KUNYU_TOKEN_DELIMITER, "预期'('开始循环条件");
    if (lparen == NULL || strcmp(lparen->value, "(") != 0) {
        return NULL;
    }
    
    // 解析循环条件表达式
    AstNode* condition = parse_expression();
    if (condition == NULL) {
        return NULL;
    }
    
    // 匹配右括号 ")"
    Token* rparen = expect(KUNYU_TOKEN_DELIMITER, "预期')'结束循环条件");
    if (rparen == NULL || strcmp(rparen->value, ")") != 0) {
        ast_free(condition);
        return NULL;
    }
    
    // 匹配左大括号 "{"
    Token* lbrace = expect(KUNYU_TOKEN_DELIMITER, "预期'{'开始循环体");
    if (lbrace == NULL || strcmp(lbrace->value, "{") != 0) {
        ast_free(condition);
        return NULL;
    }
    
    // 解析循环体
    AstNode* body = parse_block();
    if (body == NULL) {
        ast_free(condition);
        return NULL;
    }
    
    return create_loop(condition, body);
}

/**
 * 解析代码块
 */
static AstNode* parse_block() {
    // 跳过换行
    while (match(KUNYU_TOKEN_NEWLINE)) {}
    
    // 创建语句数组
    AstNode** statements = NULL;
    int stmt_count = 0;
    
    // 解析语句，直到遇到右大括号 "}"
    while (!check(KUNYU_TOKEN_DELIMITER) || strcmp(current_token()->value, "}") != 0) {
        AstNode* stmt = parse_statement();
        if (stmt == NULL) {
            // 发生错误，释放已解析的语句
            for (int i = 0; i < stmt_count; i++) {
                ast_free(statements[i]);
            }
            free(statements);
            return NULL;
        }
        
        // 添加语句到数组
        AstNode** new_statements = (AstNode**)realloc(statements, sizeof(AstNode*) * (stmt_count + 1));
        if (new_statements == NULL) {
            // 内存分配失败，释放已解析的语句
            for (int i = 0; i < stmt_count; i++) {
                ast_free(statements[i]);
            }
            free(statements);
            ast_free(stmt);
            
            parser.error.code = KUNYU_ERROR_MEMORY;
            snprintf(parser.error.message, sizeof(parser.error.message), 
                     "内存分配失败，无法扩展语句数组");
            return NULL;
        }
        
        statements = new_statements;
        statements[stmt_count] = stmt;
        stmt_count++;
        
        // 跳过换行
        while (match(KUNYU_TOKEN_NEWLINE)) {}
        
        // 检查是否到达文件结尾
        if (check(KUNYU_TOKEN_EOF)) {
            // 缺少右大括号 "}"
            for (int i = 0; i < stmt_count; i++) {
                ast_free(statements[i]);
            }
            free(statements);
            
            parser.error.code = KUNYU_ERROR_PARSER;
            snprintf(parser.error.message, sizeof(parser.error.message), 
                     "代码块未闭合，预期'}'");
            return NULL;
        }
    }
    
    // 匹配右大括号 "}"
    Token* rbrace = expect(KUNYU_TOKEN_DELIMITER, "预期'}'结束代码块");
    if (rbrace == NULL || strcmp(rbrace->value, "}") != 0) {
        for (int i = 0; i < stmt_count; i++) {
            ast_free(statements[i]);
        }
        free(statements);
        return NULL;
    }
    
    return create_block(statements, stmt_count);
}

/**
 * 解析函数声明
 */
static AstNode* parse_function_decl() {
    // 匹配 "函数" 关键字
    Token* func_keyword = expect_keyword("函数", "预期'函数'关键字");
    if (func_keyword == NULL) {
        return NULL;
    }
    
    // 获取函数名
    Token* name = expect(KUNYU_TOKEN_IDENTIFIER, "预期函数名标识符");
    if (name == NULL) {
        return NULL;
    }
    
    // 匹配左括号 "("
    Token* lparen = expect(KUNYU_TOKEN_DELIMITER, "预期'('开始参数列表");
    if (lparen == NULL || strcmp(lparen->value, "(") != 0) {
        return NULL;
    }
    
    // 解析参数列表
    char** params = NULL;
    int param_count = 0;
    
    // 如果没有立即遇到右括号，则开始解析参数
    if (!check(KUNYU_TOKEN_DELIMITER) || strcmp(current_token()->value, ")") != 0) {
        while (true) {
            // 获取参数名
            Token* param = expect(KUNYU_TOKEN_IDENTIFIER, "预期参数名标识符");
            if (param == NULL) {
                // 释放已解析的参数
                for (int i = 0; i < param_count; i++) {
                    free(params[i]);
                }
                free(params);
                return NULL;
            }
            
            // 添加参数到数组
            char** new_params = (char**)realloc(params, sizeof(char*) * (param_count + 1));
            if (new_params == NULL) {
                // 内存分配失败，释放已解析的参数
                for (int i = 0; i < param_count; i++) {
                    free(params[i]);
                }
                free(params);
                
                parser.error.code = KUNYU_ERROR_MEMORY;
                snprintf(parser.error.message, sizeof(parser.error.message), 
                         "内存分配失败，无法扩展参数数组");
                return NULL;
            }
            
            params = new_params;
            params[param_count] = strdup(param->value);
            param_count++;
            
            // 如果遇到右括号，则参数列表结束
            if (check(KUNYU_TOKEN_DELIMITER) && strcmp(current_token()->value, ")") == 0) {
                break;
            }
            
            // 否则，匹配逗号 ","
            Token* comma = expect(KUNYU_TOKEN_DELIMITER, "预期','分隔参数");
            if (comma == NULL || strcmp(comma->value, ",") != 0) {
                // 释放已解析的参数
                for (int i = 0; i < param_count; i++) {
                    free(params[i]);
                }
                free(params);
                return NULL;
            }
        }
    }
    
    // 匹配右括号 ")"
    Token* rparen = expect(KUNYU_TOKEN_DELIMITER, "预期')'结束参数列表");
    if (rparen == NULL || strcmp(rparen->value, ")") != 0) {
        // 释放已解析的参数
        for (int i = 0; i < param_count; i++) {
            free(params[i]);
        }
        free(params);
        return NULL;
    }
    
    // 匹配左大括号 "{"
    Token* lbrace = expect(KUNYU_TOKEN_DELIMITER, "预期'{'开始函数体");
    if (lbrace == NULL || strcmp(lbrace->value, "{") != 0) {
        // 释放已解析的参数
        for (int i = 0; i < param_count; i++) {
            free(params[i]);
        }
        free(params);
        return NULL;
    }
    
    // 解析函数体
    AstNode* body = parse_block();
    if (body == NULL) {
        // 释放已解析的参数
        for (int i = 0; i < param_count; i++) {
            free(params[i]);
        }
        free(params);
        return NULL;
    }
    
    return create_function(name->value, params, param_count, body);
}

/**
 * 解析返回语句
 */
static AstNode* parse_return_stmt() {
    // 匹配 "返回" 关键字
    Token* return_keyword = expect_keyword("返回", "预期'返回'关键字");
    if (return_keyword == NULL) {
        return NULL;
    }
    
    // 解析返回值表达式
    AstNode* value = parse_expression();
    if (value == NULL) {
        return NULL;
    }
    
    // 返回语句需要以分号结尾
    Token* semicolon = expect(KUNYU_TOKEN_DELIMITER, "预期';'作为返回语句的结束");
    if (semicolon == NULL || strcmp(semicolon->value, ";") != 0) {
        ast_free(value);
        return NULL;
    }
    
    return create_return(value);
}

/**
 * 解析表达式
 */
static AstNode* parse_expression() {
    // 检查是否是赋值表达式
    if (check(KUNYU_TOKEN_IDENTIFIER)) {
        Token* name = current_token();
        Token* next = peek_token();
        
        // 如果下一个标记是赋值运算符，则解析赋值表达式
        if (next != NULL && next->type == KUNYU_TOKEN_OPERATOR && strcmp(next->value, "=") == 0) {
            advance(); // 消耗标识符
            advance(); // 消耗赋值运算符
            
            // 解析右侧表达式
            AstNode* value = parse_expression();
            if (value == NULL) {
                return NULL;
            }
            
            // 注意：这里不需要检查分号，因为赋值表达式作为表达式语句的一部分，
            // 在parse_statement中已经处理了分号检查
            return create_assign(name->value, value);
        }
    }
    
    AstNode* expr = parse_primary();
    if (expr == NULL) {
        return NULL;
    }
    
    // 检查是否是二元表达式
    Token* next = current_token();
    if (next != NULL && next->type == KUNYU_TOKEN_OPERATOR) {
        return parse_binary_expr(expr);
    }
    
    return expr;
}

/**
 * 解析二元表达式
 */
static AstNode* parse_binary_expr(AstNode* left) {
    Token* op = advance(); // 获取运算符
    
    // 确定运算符类型
    BinaryOpType op_type;
    if (strcmp(op->value, "+") == 0) {
        op_type = OP_ADD;
    } else if (strcmp(op->value, "-") == 0) {
        op_type = OP_SUB;
    } else if (strcmp(op->value, "*") == 0) {
        op_type = OP_MUL;
    } else if (strcmp(op->value, "/") == 0) {
        op_type = OP_DIV;
    } else if (strcmp(op->value, "%") == 0) {
        op_type = OP_MOD;
    } else if (strcmp(op->value, "==") == 0) {
        op_type = OP_EQ;
    } else if (strcmp(op->value, "!=") == 0) {
        op_type = OP_NE;
    } else if (strcmp(op->value, "<") == 0) {
        op_type = OP_LT;
    } else if (strcmp(op->value, "<=") == 0) {
        op_type = OP_LE;
    } else if (strcmp(op->value, ">") == 0) {
        op_type = OP_GT;
    } else if (strcmp(op->value, ">=") == 0) {
        op_type = OP_GE;
    } else if (strcmp(op->value, "&&") == 0) {
        op_type = OP_AND;
    } else if (strcmp(op->value, "||") == 0) {
        op_type = OP_OR;
    } else {
        ast_free(left);
        parser.error.code = KUNYU_ERROR_PARSER;
        parser.error.line = op->line;
        parser.error.column = op->column;
        snprintf(parser.error.message, sizeof(parser.error.message), 
                 "不支持的运算符: %s", op->value);
        return NULL;
    }
    
    // 解析右操作数
    AstNode* right = parse_primary();
    if (right == NULL) {
        ast_free(left);
        return NULL;
    }
    
    // 创建二元表达式节点
    AstNode* binary = create_binary(left, op_type, right);
    if (binary == NULL) {
        ast_free(left);
        ast_free(right);
        parser.error.code = KUNYU_ERROR_MEMORY;
        snprintf(parser.error.message, sizeof(parser.error.message), 
                 "内存分配失败，无法创建二元表达式节点");
        return NULL;
    }
    
    // 检查是否有更多的运算符
    Token* next = current_token();
    if (next != NULL && next->type == KUNYU_TOKEN_OPERATOR) {
        return parse_binary_expr(binary);
    }
    
    return binary;
}

/**
 * 解析函数调用
 */
static AstNode* parse_function_call(const char* name) {
    // 匹配左括号 "("
    Token* lparen = expect(KUNYU_TOKEN_DELIMITER, "预期'('开始函数调用");
    if (lparen == NULL || strcmp(lparen->value, "(") != 0) {
        return NULL;
    }
    
    // 解析参数列表
    AstNode** args = NULL;
    int arg_count = 0;
    
    // 如果没有立即遇到右括号，则开始解析参数
    if (!check(KUNYU_TOKEN_DELIMITER) || strcmp(current_token()->value, ")") != 0) {
        while (true) {
            // 解析参数表达式
            AstNode* arg = parse_expression();
            if (arg == NULL) {
                // 释放已解析的参数
                for (int i = 0; i < arg_count; i++) {
                    ast_free(args[i]);
                }
                free(args);
                return NULL;
            }
            
            // 添加参数到数组
            AstNode** new_args = (AstNode**)realloc(args, sizeof(AstNode*) * (arg_count + 1));
            if (new_args == NULL) {
                // 内存分配失败，释放已解析的参数
                for (int i = 0; i < arg_count; i++) {
                    ast_free(args[i]);
                }
                free(args);
                ast_free(arg);
                
                parser.error.code = KUNYU_ERROR_MEMORY;
                snprintf(parser.error.message, sizeof(parser.error.message), 
                         "内存分配失败，无法扩展参数数组");
                return NULL;
            }
            
            args = new_args;
            args[arg_count] = arg;
            arg_count++;
            
            // 如果遇到右括号，则参数列表结束
            if (check(KUNYU_TOKEN_DELIMITER) && strcmp(current_token()->value, ")") == 0) {
                break;
            }
            
            // 否则，匹配逗号 ","
            Token* comma = expect(KUNYU_TOKEN_DELIMITER, "预期','分隔参数");
            if (comma == NULL || strcmp(comma->value, ",") != 0) {
                // 释放已解析的参数
                for (int i = 0; i < arg_count; i++) {
                    ast_free(args[i]);
                }
                free(args);
                return NULL;
            }
        }
    }
    
    // 匹配右括号 ")"
    Token* rparen = expect(KUNYU_TOKEN_DELIMITER, "预期')'结束参数列表");
    if (rparen == NULL || strcmp(rparen->value, ")") != 0) {
        // 释放已解析的参数
        for (int i = 0; i < arg_count; i++) {
            ast_free(args[i]);
        }
        free(args);
        return NULL;
    }
    
    return create_call(name, args, arg_count);
}

/**
 * 解析基本表达式（字面量、变量、分组表达式等）
 */
static AstNode* parse_primary() {
    Token* token = current_token();
    if (token == NULL) {
        parser.error.code = KUNYU_ERROR_PARSER;
        snprintf(parser.error.message, sizeof(parser.error.message), 
                 "预期表达式但遇到了文件结束");
        return NULL;
    }
    
    // 处理字面量
    if (token->type == KUNYU_TOKEN_NUMBER || token->type == KUNYU_TOKEN_STRING) {
        advance();
        return create_literal(token->type, token->value);
    }
    
    // 处理变量引用或函数调用
    if (token->type == KUNYU_TOKEN_IDENTIFIER) {
        const char* name = token->value;
        advance();
        
        // 检查是否是函数调用
        if (check(KUNYU_TOKEN_DELIMITER) && strcmp(current_token()->value, "(") == 0) {
            return parse_function_call(name);
        }
        
        // 否则是变量引用
        return create_variable(name);
    }
    
    // 处理分组表达式
    if (token->type == KUNYU_TOKEN_DELIMITER && strcmp(token->value, "(") == 0) {
        advance(); // 跳过左括号
        
        AstNode* expr = parse_expression();
        if (expr == NULL) {
            return NULL;
        }
        
        // 确保有匹配的右括号
        token = expect(KUNYU_TOKEN_DELIMITER, "预期')'来闭合分组表达式");
        if (token == NULL || strcmp(token->value, ")") != 0) {
            ast_free(expr);
            return NULL;
        }
        
        return create_grouping(expr);
    }
    
    parser.error.code = KUNYU_ERROR_PARSER;
    parser.error.line = token->line;
    parser.error.column = token->column;
    snprintf(parser.error.message, sizeof(parser.error.message), 
             "预期表达式但遇到了: %s", token->value);
    return NULL;
}

/**
 * 初始化语法分析器
 */
static void parser_init(Token *tokens, size_t token_count) {
    parser.tokens = tokens;
    parser.token_count = token_count;
    parser.current = 0;
    parser.error.code = KUNYU_OK;
    parser.error.message[0] = '\0';
    parser.error.line = 0;
    parser.error.column = 0;
}

/**
 * 解析标记流生成AST
 * @param tokens 标记数组
 * @param token_count 标记数量
 * @return AST根节点，失败返回NULL
 */
struct AstNode* parser_parse(Token *tokens, size_t token_count) {
    if (tokens == NULL || token_count == 0) {
        return NULL;
    }
    
    // 初始化语法分析器
    parser_init(tokens, token_count);
    
    // 解析程序
    return parse_program();
}

/**
 * 获取语法分析器错误信息
 */
KunyuError* parser_get_error() {
    return &parser.error;
} 