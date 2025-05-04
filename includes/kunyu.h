/**
 * 坤舆编程语言 - 主头文件
 * 定义基本数据结构和接口
 */

#ifndef KUNYU_H
#define KUNYU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * 版本信息
 */
#define KUNYU_VERSION "0.1.0"
#define KUNYU_NAME "坤舆编程语言"

/**
 * 错误码定义
 */
typedef enum {
    KUNYU_OK = 0,
    KUNYU_ERROR_LEXER,       // 词法分析错误
    KUNYU_ERROR_PARSER,      // 语法分析错误
    KUNYU_ERROR_COMPILER,    // 编译错误
    KUNYU_ERROR_RUNTIME,     // 运行时错误
    KUNYU_ERROR_IO,          // IO错误
    KUNYU_ERROR_MEMORY,      // 内存错误
} KunyuErrorCode;

/**
 * 标记类型定义
 */
typedef enum {
    KUNYU_TOKEN_EOF = 0,     // 文件结束
    KUNYU_TOKEN_IDENTIFIER,  // 标识符
    KUNYU_TOKEN_KEYWORD,     // 关键字
    KUNYU_TOKEN_STRING,      // 字符串
    KUNYU_TOKEN_NUMBER,      // 数字
    KUNYU_TOKEN_OPERATOR,    // 运算符
    KUNYU_TOKEN_DELIMITER,   // 分隔符
    KUNYU_TOKEN_NEWLINE,     // 换行
} KunyuTokenType;

/**
 * 标记结构
 */
typedef struct {
    KunyuTokenType type;     // 标记类型
    char *value;             // 标记值
    int line;                // 行号
    int column;              // 列号
} Token;

/**
 * 关键字枚举
 */
typedef enum {
    KEYWORD_VARIABLE = 0,    // 变量
    KEYWORD_CONSTANT,        // 常量
    KEYWORD_IF,              // 如果
    KEYWORD_ELSE,            // 否则
    KEYWORD_LOOP,            // 循环
    KEYWORD_FUNCTION,        // 函数
    KEYWORD_RETURN,          // 返回
    KEYWORD_PRINT,           // 输出
    KEYWORD_COUNT            // 关键字总数
} KeywordType;

/**
 * AST节点类型
 */
typedef enum {
    NODE_PROGRAM = 0,        // 程序
    NODE_BLOCK,              // 代码块
    NODE_VARDECL,            // 变量声明
    NODE_FUNCDECL,           // 函数声明
    NODE_IF,                 // 条件语句
    NODE_LOOP,               // 循环语句
    NODE_CALL,               // 函数调用
    NODE_RETURN,             // 返回语句
    NODE_PRINT,              // 输出语句
    NODE_BINARY,             // 二元表达式
    NODE_UNARY,              // 一元表达式
    NODE_LITERAL,            // 字面量
    NODE_IDENTIFIER,         // 标识符
    NODE_GROUPING,           // 分组表达式
    NODE_ASSIGN              // 赋值表达式
} NodeType;

/**
 * 对象类型定义
 */
typedef enum {
    TYPE_NULL = 0,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_DICT,
    TYPE_FUNCTION
} ObjectType;

/**
 * 基本对象结构
 */
typedef struct PyObject {
    ObjectType type;         // 对象类型
    int ref_count;           // 引用计数
    void (*destructor)(struct PyObject*);  // 析构函数
} PyObject;

/**
 * 数字对象
 */
typedef struct {
    PyObject base;
    double value;
} PyNumberObject;

/**
 * 字符串对象
 */
typedef struct {
    PyObject base;
    char *value;
    size_t length;
} PyStringObject;

/**
 * 列表对象
 */
typedef struct {
    PyObject base;
    PyObject **items;
    size_t length;
    size_t capacity;
} PyListObject;

/**
 * 字典项
 */
typedef struct {
    PyObject *key;
    PyObject *value;
} DictItem;

/**
 * 字典对象
 */
typedef struct {
    PyObject base;
    DictItem *items;
    size_t size;
    size_t capacity;
} PyDictObject;

/**
 * 错误处理结构
 */
typedef struct {
    KunyuErrorCode code;
    char message[256];
    int line;
    int column;
} KunyuError;

/**
 * 前置声明
 */
struct AstNode;
struct CodeObject;
struct VmState;

/**
 * 词法分析器接口
 */
Token* lexer_init(const char *source);
Token* lexer_next_token();
void lexer_free();
int lexer_tokenize();
KunyuError* lexer_get_error();
Token* lexer_get_tokens();
size_t lexer_get_token_count();

/**
 * 语法分析器接口
 */
struct AstNode* parser_parse(Token *tokens, size_t token_count);
void parser_free(struct AstNode *node);

/**
 * 编译器接口
 */
struct CodeObject* compiler_compile(struct AstNode *root);
void compiler_free(struct CodeObject *code);

/**
 * 虚拟机接口
 */
PyObject* vm_execute(struct CodeObject *code);
void vm_free();

/**
 * 内存管理接口
 */
void* kunyu_malloc(size_t size);
void kunyu_free(void *ptr);
void kunyu_collect_garbage();

/**
 * 对象系统接口
 */
void py_incref(PyObject *obj);
void py_decref(PyObject *obj);
PyObject* py_number_new(double value);
PyObject* py_string_new(const char *value);

// 列表对象接口
PyObject* py_list_new();
bool py_list_append(PyObject *list, PyObject *item);
size_t py_list_length(PyObject *list);
PyObject* py_list_get(PyObject *list, size_t index);
bool py_list_set(PyObject *list, size_t index, PyObject *item);

// 字典对象接口
PyObject* py_dict_new();
bool py_dict_set(PyObject *dict, PyObject *key, PyObject *value);
PyObject* py_dict_get(PyObject *dict, PyObject *key);
size_t py_dict_size(PyObject *dict);

/**
 * 解释器接口
 */
bool interpreter_execute(struct AstNode *root);
KunyuError* interpreter_get_error();
void interpreter_cleanup();

/**
 * 内置函数接口
 */
void builtins_init();
void builtins_cleanup();
PyObject* builtins_call(const char *name, PyObject **args, int arg_count);
bool builtins_is_builtin(const char *name);

/**
 * REPL接口
 */
void repl_start();

#endif /* KUNYU_H */ 