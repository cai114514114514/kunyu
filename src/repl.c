/**
 * 坤舆编程语言 - 交互式解释器 (REPL)
 * 提供读取-求值-打印-循环的交互式环境
 */

#include "../includes/kunyu.h"
#include "../includes/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define REPL_BUFFER_SIZE 4096
#define REPL_PROMPT "坤舆> "
#define REPL_WELCOME "欢迎使用坤舆编程语言交互式环境！\n" \
                    "输入表达式或语句进行求值，输入'退出'或按Ctrl+D退出。\n"

// 存储上下文的全局状态
static bool repl_initialized = false;

/**
 * 设置控制台支持UTF-8输出
 */
static void setup_console_utf8() {
#ifdef _WIN32
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

/**
 * 打印一个对象的字符串表示
 */
static void print_object(PyObject *obj) {
    if (obj == NULL) {
        printf("null\n");
        return;
    }
    
    switch (obj->type) {
        case TYPE_NUMBER: {
            PyNumberObject *num = (PyNumberObject *)obj;
            // 检查是否为整数
            double intpart;
            if (modf(num->value, &intpart) == 0.0) {
                printf("%.0f\n", num->value);
            } else {
                printf("%g\n", num->value);
            }
            break;
        }
        case TYPE_STRING: {
            PyStringObject *str = (PyStringObject *)obj;
            printf("\"%s\"\n", str->value);
            break;
        }
        case TYPE_LIST: {
            PyListObject *list = (PyListObject *)list;
            printf("[列表，长度: %zu]\n", py_list_length(obj));
            break;
        }
        case TYPE_DICT: {
            PyDictObject *dict = (PyDictObject *)dict;
            printf("[字典，大小: %zu]\n", py_dict_size(obj));
            break;
        }
        default:
            printf("[对象]\n");
            break;
    }
}

/**
 * 执行表达式并打印结果
 */
static bool execute_and_print(const char *source) {
    // 初始化词法分析器
    Token *tokens = lexer_init(source);
    if (tokens == NULL) {
        fprintf(stderr, "错误: 初始化词法分析器失败\n");
        return false;
    }
    
    // 执行词法分析
    int token_count = lexer_tokenize();
    if (token_count < 0) {
        KunyuError *error = lexer_get_error();
        fprintf(stderr, "词法分析错误: %s (行 %d, 列 %d)\n", 
                error->message, error->line, error->column);
        lexer_free();
        return false;
    }
    
    // 判断输入是否为表达式或语句
    bool is_expression = false;
    for (int i = 0; i < token_count; i++) {
        Token *token = &tokens[i];
        
        // 如果有赋值运算符（=）但没有变量/常量关键字，认为是表达式
        if (token->type == KUNYU_TOKEN_OPERATOR && strcmp(token->value, "=") == 0) {
            bool has_var_keyword = false;
            for (int j = 0; j < i; j++) {
                if (tokens[j].type == KUNYU_TOKEN_KEYWORD && 
                    (strcmp(tokens[j].value, "变量") == 0 || strcmp(tokens[j].value, "常量") == 0)) {
                    has_var_keyword = true;
                    break;
                }
            }
            
            if (!has_var_keyword) {
                is_expression = true;
                break;
            }
        }
        
        // 如果没有分号结尾，认为是表达式（简化处理）
        if (i == token_count - 1 && 
            !(token->type == KUNYU_TOKEN_DELIMITER && strcmp(token->value, ";") == 0)) {
            is_expression = true;
        }
    }
    
    // 如果是表达式，添加输出语句
    if (is_expression) {
        // 创建新的源代码，添加输出语句
        char *new_source = (char *)malloc(strlen(source) + 10);
        if (new_source == NULL) {
            fprintf(stderr, "错误: 内存分配失败\n");
            lexer_free();
            return false;
        }
        
        sprintf(new_source, "输出 %s;", source);
        
        // 使用新的源代码重新进行词法分析
        lexer_free();
        tokens = lexer_init(new_source);
        if (tokens == NULL) {
            fprintf(stderr, "错误: 初始化词法分析器失败\n");
            free(new_source);
            return false;
        }
        
        token_count = lexer_tokenize();
        if (token_count < 0) {
            KunyuError *error = lexer_get_error();
            fprintf(stderr, "词法分析错误: %s (行 %d, 列 %d)\n", 
                    error->message, error->line, error->column);
            lexer_free();
            free(new_source);
            return false;
        }
        
        free(new_source);
    }
    
    // 语法分析
    AstNode *ast = parser_parse(tokens, token_count);
    if (ast == NULL) {
        KunyuError *error = parser_get_error();
        if (error->code != KUNYU_OK) {
            fprintf(stderr, "语法分析错误: %s (行 %d, 列 %d)\n", 
                    error->message, error->line, error->column);
        } else {
            fprintf(stderr, "错误: 语法分析失败，无法生成AST\n");
        }
        lexer_free();
        return false;
    }
    
    // 执行程序
    if (!interpreter_execute(ast)) {
        KunyuError *error = interpreter_get_error();
        fprintf(stderr, "运行时错误: %s (行 %d, 列 %d)\n", 
                error->message, error->line, error->column);
        ast_free(ast);
        lexer_free();
        return false;
    }
    
    // 释放资源
    ast_free(ast);
    lexer_free();
    
    return true;
}

/**
 * 初始化REPL环境
 */
static bool repl_init() {
    // 设置控制台以支持UTF-8输出
    setup_console_utf8();
    
    // 初始化解释器（解释器内部会初始化全局作用域）
    // 为了保持全局作用域，我们只在程序开始时执行一次interpreter_init()
    // 注意：全局变量会在不同表达式之间保持状态
    
    repl_initialized = true;
    return true;
}

/**
 * 读取用户输入
 * 返回动态分配的字符串，使用后需要释放
 */
static char* read_input() {
    char *buffer = (char *)malloc(REPL_BUFFER_SIZE);
    if (buffer == NULL) {
        fprintf(stderr, "错误: 内存分配失败\n");
        return NULL;
    }
    
    printf("%s", REPL_PROMPT);
    
    if (fgets(buffer, REPL_BUFFER_SIZE, stdin) == NULL) {
        free(buffer);
        return NULL;  // EOF 或读取错误
    }
    
    // 移除末尾的换行符
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return buffer;
}

/**
 * 启动REPL循环
 */
void repl_start() {
    if (!repl_initialized) {
        if (!repl_init()) {
            fprintf(stderr, "错误: 初始化REPL环境失败\n");
            return;
        }
    }
    
    // 打印欢迎信息
    printf("%s", REPL_WELCOME);
    
    while (true) {
        // 读取输入
        char *input = read_input();
        if (input == NULL) {
            printf("\n再见！\n");
            break;  // EOF，退出循环
        }
        
        // 检查是否要退出
        if (strcmp(input, "退出") == 0 || strcmp(input, "exit") == 0) {
            free(input);
            printf("再见！\n");
            break;
        }
        
        // 跳过空输入
        if (input[0] == '\0') {
            free(input);
            continue;
        }
        
        // 执行并打印结果
        execute_and_print(input);
        
        free(input);
    }
} 