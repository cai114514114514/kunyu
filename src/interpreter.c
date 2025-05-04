/**
 * 坤舆编程语言 - 解释器
 * 直接执行抽象语法树
 */

#include "../includes/kunyu.h"
#include "../includes/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/**
 * 解释器上下文
 */
typedef struct {
    KunyuError error;        // 错误信息
    bool has_return;         // 是否有返回值
    PyObject *return_value;  // 返回值
} InterpreterContext;

// 函数表条目
typedef struct FunctionEntry {
    char *name;              // 函数名
    char **params;           // 参数名列表
    int param_count;         // 参数数量
    AstNode *body;           // 函数体
    struct FunctionEntry *next;
} FunctionEntry;

// 变量表条目
typedef struct VariableEntry {
    char *name;
    bool is_constant;
    PyObject *value;
    struct VariableEntry *next;
} VariableEntry;

// 作用域
typedef struct Scope {
    VariableEntry *variables; // 本作用域的变量
    struct Scope *parent;     // 父作用域
} Scope;

// 符号表（变量表和函数表）
static Scope *current_scope = NULL;
static FunctionEntry *function_table = NULL;

// 全局解释器上下文
static InterpreterContext interpreter;

/**
 * 初始化解释器
 */
static void interpreter_init() {
    interpreter.error.code = KUNYU_OK;
    interpreter.error.message[0] = '\0';
    interpreter.error.line = 0;
    interpreter.error.column = 0;
    interpreter.has_return = false;
    interpreter.return_value = NULL;
    
    // 清空所有作用域
    while (current_scope != NULL) {
        Scope *parent = current_scope->parent;
        
        // 清空作用域中的变量
        VariableEntry *var = current_scope->variables;
        while (var != NULL) {
            VariableEntry *next = var->next;
            if (var->value != NULL) {
                py_decref(var->value);
            }
            free(var->name);
            free(var);
            var = next;
        }
        
        free(current_scope);
        current_scope = parent;
    }
    
    // 创建全局作用域
    current_scope = (Scope*)malloc(sizeof(Scope));
    if (current_scope != NULL) {
        current_scope->variables = NULL;
        current_scope->parent = NULL;
    }
    
    // 清空函数表
    FunctionEntry *func = function_table;
    while (func != NULL) {
        FunctionEntry *next = func->next;
        free(func->name);
        
        // 释放参数名
        for (int i = 0; i < func->param_count; i++) {
            free(func->params[i]);
        }
        free(func->params);
        
        free(func);
        func = next;
    }
    function_table = NULL;
    
    // 初始化内置函数
    builtins_init();
}

/**
 * 创建新的作用域
 */
static bool push_scope() {
    Scope *new_scope = (Scope*)malloc(sizeof(Scope));
    if (new_scope == NULL) {
        interpreter.error.code = KUNYU_ERROR_MEMORY;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "内存分配失败，无法创建新作用域");
        return false;
    }
    
    new_scope->variables = NULL;
    new_scope->parent = current_scope;
    current_scope = new_scope;
    
    return true;
}

/**
 * 退出当前作用域
 */
static void pop_scope() {
    if (current_scope == NULL) {
        return;
    }
    
    Scope *parent = current_scope->parent;
    
    // 清空作用域中的变量
    VariableEntry *var = current_scope->variables;
    while (var != NULL) {
        VariableEntry *next = var->next;
        if (var->value != NULL) {
            py_decref(var->value);
        }
        free(var->name);
        free(var);
        var = next;
    }
    
    free(current_scope);
    current_scope = parent;
}

/**
 * 创建数字对象
 */
static PyObject* create_number_object(double value) {
    return py_number_new(value);
}

/**
 * 创建字符串对象
 */
static PyObject* create_string_object(const char *value) {
    return py_string_new(value);
}

/**
 * 在当前作用域查找变量
 */
static VariableEntry* find_variable_in_scope(const char *name, Scope *scope) {
    VariableEntry *entry = scope->variables;
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * 查找变量（包括所有父作用域）
 */
static VariableEntry* find_variable(const char *name) {
    Scope *scope = current_scope;
    while (scope != NULL) {
        VariableEntry *entry = find_variable_in_scope(name, scope);
        if (entry != NULL) {
            return entry;
        }
        scope = scope->parent;
    }
    return NULL;
}

/**
 * 查找函数
 */
static FunctionEntry* find_function(const char *name) {
    FunctionEntry *entry = function_table;
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * 定义变量
 */
static bool define_variable(const char *name, PyObject *value, bool is_constant) {
    // 检查当前作用域中变量是否已存在
    VariableEntry *existing = find_variable_in_scope(name, current_scope);
    if (existing != NULL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "变量'%s'已经在当前作用域中定义", name);
        return false;
    }
    
    // 创建新的变量条目
    VariableEntry *entry = (VariableEntry *)malloc(sizeof(VariableEntry));
    if (entry == NULL) {
        interpreter.error.code = KUNYU_ERROR_MEMORY;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "内存分配失败，无法创建变量条目");
        return false;
    }
    
    entry->name = strdup(name);
    if (entry->name == NULL) {
        free(entry);
        interpreter.error.code = KUNYU_ERROR_MEMORY;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "内存分配失败，无法复制变量名");
        return false;
    }
    
    entry->value = value;
    entry->is_constant = is_constant;
    
    // 增加引用计数
    if (value != NULL) {
        py_incref(value);
    }
    
    // 添加到当前作用域的变量表
    entry->next = current_scope->variables;
    current_scope->variables = entry;
    
    return true;
}

/**
 * 定义函数
 */
static bool define_function(const char *name, char **params, int param_count, AstNode *body) {
    // 检查函数是否已存在
    FunctionEntry *existing = find_function(name);
    if (existing != NULL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "函数'%s'已经定义", name);
        return false;
    }
    
    // 创建新的函数条目
    FunctionEntry *entry = (FunctionEntry *)malloc(sizeof(FunctionEntry));
    if (entry == NULL) {
        interpreter.error.code = KUNYU_ERROR_MEMORY;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "内存分配失败，无法创建函数条目");
        return false;
    }
    
    entry->name = strdup(name);
    if (entry->name == NULL) {
        free(entry);
        interpreter.error.code = KUNYU_ERROR_MEMORY;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "内存分配失败，无法复制函数名");
        return false;
    }
    
    // 复制参数列表
    if (param_count > 0) {
        entry->params = (char**)malloc(sizeof(char*) * param_count);
        if (entry->params == NULL) {
            free(entry->name);
            free(entry);
            interpreter.error.code = KUNYU_ERROR_MEMORY;
            snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                     "内存分配失败，无法创建参数数组");
            return false;
        }
        
        for (int i = 0; i < param_count; i++) {
            entry->params[i] = strdup(params[i]);
            if (entry->params[i] == NULL) {
                // 清理已分配的内存
                for (int j = 0; j < i; j++) {
                    free(entry->params[j]);
                }
                free(entry->params);
                free(entry->name);
                free(entry);
                
                interpreter.error.code = KUNYU_ERROR_MEMORY;
                snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                         "内存分配失败，无法复制参数名");
                return false;
            }
        }
    } else {
        entry->params = NULL;
    }
    
    entry->param_count = param_count;
    entry->body = body;
    
    // 添加到函数表
    entry->next = function_table;
    function_table = entry;
    
    return true;
}

/**
 * 获取变量值
 */
static PyObject* get_variable(const char *name) {
    VariableEntry *entry = find_variable(name);
    if (entry == NULL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "未定义的变量: %s", name);
        return NULL;
    }
    
    return entry->value;
}

/**
 * 设置变量值
 */
static bool set_variable(const char *name, PyObject *value) {
    VariableEntry *entry = find_variable(name);
    if (entry == NULL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "未定义的变量: %s", name);
        return false;
    }
    
    if (entry->is_constant) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "不能修改常量: %s", name);
        return false;
    }
    
    // 替换值
    if (entry->value != NULL) {
        py_decref(entry->value);
    }
    
    entry->value = value;
    if (value != NULL) {
        py_incref(value);
    }
    
    return true;
}

/**
 * 判断对象是否为真
 */
static bool is_truthy(PyObject *obj) {
    if (obj == NULL) {
        return false;
    }
    
    switch (obj->type) {
        case TYPE_NUMBER: {
            PyNumberObject *num = (PyNumberObject *)obj;
            return num->value != 0;
        }
        case TYPE_STRING: {
            PyStringObject *str = (PyStringObject *)obj;
            return str->length > 0;
        }
        default:
            return true;
    }
}

/**
 * 字符串表示对象值
 */
static char* object_to_string(PyObject *obj) {
    if (obj == NULL) {
        return strdup("null");
    }
    
    char buffer[256]; // 用于转换数字的缓冲区
    
    switch (obj->type) {
        case TYPE_NUMBER: {
            PyNumberObject *num = (PyNumberObject *)obj;
            // 检查数字是否为整数
            double intpart;
            if (modf(num->value, &intpart) == 0.0) {
                // 是整数，使用%d格式
                snprintf(buffer, sizeof(buffer), "%.0f", num->value);
            } else {
                // 是浮点数，使用%g格式
                snprintf(buffer, sizeof(buffer), "%g", num->value);
            }
            return strdup(buffer);
        }
        case TYPE_STRING: {
            PyStringObject *str = (PyStringObject *)obj;
            return strdup(str->value);
        }
        default:
            return strdup("[对象]");
    }
}

/**
 * 前置声明执行函数
 */
static bool execute_statement(AstNode *node);
static bool execute_block(BlockStmt *block);
static PyObject* eval_expression(AstNode *node);

/**
 * 执行输出语句
 */
static bool exec_print_stmt(AstNode *node) {
    if (node->type != NODE_PRINT) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期输出语句节点");
        return false;
    }
    
    PrintStmt *stmt = (PrintStmt *)node;
    PyObject *value = eval_expression(stmt->value);
    if (value == NULL) {
        return false;
    }
    
    char *str = object_to_string(value);
    printf("%s\n", str);
    free(str);
    
    py_decref(value);
    return true;
}

/**
 * 执行变量声明
 */
static bool exec_var_decl(AstNode *node) {
    if (node->type != NODE_VARDECL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期变量声明节点");
        return false;
    }
    
    VarDeclStmt *stmt = (VarDeclStmt *)node;
    
    // 评估初始值
    PyObject *value = eval_expression(stmt->initializer);
    if (value == NULL) {
        return false;
    }
    
    // 定义变量
    bool result = define_variable(stmt->name, value, stmt->is_constant);
    py_decref(value);
    
    return result;
}

/**
 * 执行条件语句
 */
static bool exec_if_stmt(AstNode *node) {
    if (node->type != NODE_IF) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期条件语句节点");
        return false;
    }
    
    IfStmt *stmt = (IfStmt *)node;
    
    // 评估条件
    PyObject *condition = eval_expression(stmt->condition);
    if (condition == NULL) {
        return false;
    }
    
    bool condition_result = is_truthy(condition);
    py_decref(condition);
    
    // 根据条件执行相应的分支
    if (condition_result) {
        return execute_statement(stmt->then_branch);
    } else if (stmt->else_branch != NULL) {
        return execute_statement(stmt->else_branch);
    }
    
    return true;
}

/**
 * 执行循环语句
 */
static bool exec_loop_stmt(AstNode *node) {
    if (node->type != NODE_LOOP) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期循环语句节点");
        return false;
    }
    
    LoopStmt *stmt = (LoopStmt *)node;
    
    while (true) {
        // 评估条件
        PyObject *condition = eval_expression(stmt->condition);
        if (condition == NULL) {
            return false;
        }
        
        bool condition_result = is_truthy(condition);
        py_decref(condition);
        
        // 条件为假时退出循环
        if (!condition_result) {
            break;
        }
        
        // 执行循环体
        bool result = execute_statement(stmt->body);
        if (!result) {
            return false;
        }
        
        // 如果有返回值，提前退出循环
        if (interpreter.has_return) {
            return true;
        }
    }
    
    return true;
}

/**
 * 执行函数声明
 */
static bool exec_function_decl(AstNode *node) {
    if (node->type != NODE_FUNCDECL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期函数声明节点");
        return false;
    }
    
    FunctionStmt *stmt = (FunctionStmt *)node;
    
    // 定义函数
    return define_function(stmt->name, stmt->params, stmt->param_count, stmt->body);
}

/**
 * 执行代码块
 */
static bool execute_block(BlockStmt *block) {
    if (block->base.base.type != NODE_BLOCK) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期代码块节点");
        return false;
    }
    
    // 创建新的作用域
    if (!push_scope()) {
        return false;
    }
    
    // 执行块中的每条语句
    for (int i = 0; i < block->stmt_count; i++) {
        if (!execute_statement(block->statements[i])) {
            pop_scope();
            return false;
        }
        
        // 如果遇到返回语句，停止执行
        if (interpreter.has_return) {
            break;
        }
    }
    
    // 退出作用域
    pop_scope();
    
    return true;
}

/**
 * 执行返回语句
 */
static bool exec_return_stmt(AstNode *node) {
    if (node->type != NODE_RETURN) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期返回语句节点");
        return false;
    }
    
    ReturnStmt *stmt = (ReturnStmt *)node;
    
    // 释放之前的返回值
    if (interpreter.return_value != NULL) {
        py_decref(interpreter.return_value);
        interpreter.return_value = NULL;
    }
    
    // 评估返回值表达式
    if (stmt->value != NULL) {
        PyObject *value = eval_expression(stmt->value);
        if (value == NULL) {
            return false;
        }
        interpreter.return_value = value;
    }
    
    // 设置返回标志
    interpreter.has_return = true;
    
    return true;
}

/**
 * 执行表达式语句
 */
static bool exec_expression_stmt(AstNode *node) {
    if (node->type != NODE_PROGRAM || ((StmtNode*)node)->stmt_type != STMT_EXPRESSION) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期表达式语句节点");
        return false;
    }
    
    ExpressionStmt *stmt = (ExpressionStmt *)node;
    
    // 评估表达式
    PyObject *result = eval_expression(stmt->expr);
    if (result == NULL) {
        return false;
    }
    
    // 释放结果，表达式语句不保留值
    py_decref(result);
    
    return true;
}

/**
 * 处理二元表达式
 */
static PyObject* eval_binary_expr(BinaryExpr *expr) {
    PyObject *left = eval_expression(expr->left);
    if (left == NULL) {
        return NULL;
    }
    
    PyObject *right = eval_expression(expr->right);
    if (right == NULL) {
        py_decref(left);
        return NULL;
    }
    
    PyObject *result = NULL;
    
    // 处理字符串连接
    if (expr->op == OP_ADD && 
        (left->type == TYPE_STRING || right->type == TYPE_STRING)) {
        
        char *left_str = object_to_string(left);
        char *right_str = object_to_string(right);
        
        size_t result_len = strlen(left_str) + strlen(right_str) + 1;
        char *result_str = (char *)malloc(result_len);
        
        if (result_str != NULL) {
            strcpy(result_str, left_str);
            strcat(result_str, right_str);
            result = create_string_object(result_str);
            free(result_str);
        }
        
        free(left_str);
        free(right_str);
    }
    // 处理数值运算
    else if (left->type == TYPE_NUMBER && right->type == TYPE_NUMBER) {
        PyNumberObject *num_left = (PyNumberObject *)left;
        PyNumberObject *num_right = (PyNumberObject *)right;
        double value;
        
        switch (expr->op) {
            case OP_ADD:
                value = num_left->value + num_right->value;
                break;
            case OP_SUB:
                value = num_left->value - num_right->value;
                break;
            case OP_MUL:
                value = num_left->value * num_right->value;
                break;
            case OP_DIV:
                if (num_right->value == 0) {
                    interpreter.error.code = KUNYU_ERROR_RUNTIME;
                    snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                             "除数不能为零");
                    py_decref(left);
                    py_decref(right);
                    return NULL;
                }
                value = num_left->value / num_right->value;
                break;
            case OP_MOD:
                if (num_right->value == 0) {
                    interpreter.error.code = KUNYU_ERROR_RUNTIME;
                    snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                             "模运算的除数不能为零");
                    py_decref(left);
                    py_decref(right);
                    return NULL;
                }
                value = (int)num_left->value % (int)num_right->value;
                break;
            case OP_EQ:
                value = (num_left->value == num_right->value) ? 1 : 0;
                break;
            case OP_NE:
                value = (num_left->value != num_right->value) ? 1 : 0;
                break;
            case OP_LT:
                value = (num_left->value < num_right->value) ? 1 : 0;
                break;
            case OP_LE:
                value = (num_left->value <= num_right->value) ? 1 : 0;
                break;
            case OP_GT:
                value = (num_left->value > num_right->value) ? 1 : 0;
                break;
            case OP_GE:
                value = (num_left->value >= num_right->value) ? 1 : 0;
                break;
            default:
                interpreter.error.code = KUNYU_ERROR_RUNTIME;
                snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                         "不支持的运算符");
                py_decref(left);
                py_decref(right);
                return NULL;
        }
        
        result = create_number_object(value);
    }
    else {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "类型不匹配的运算");
    }
    
    py_decref(left);
    py_decref(right);
    
    return result;
}

/**
 * 评估函数调用表达式
 */
static PyObject* eval_call_expr(CallExpr *expr) {
    // 首先检查是否是内置函数
    if (builtins_is_builtin(expr->name)) {
        // 评估所有参数
        PyObject **args = NULL;
        if (expr->arg_count > 0) {
            args = (PyObject **)malloc(sizeof(PyObject *) * expr->arg_count);
            if (args == NULL) {
                interpreter.error.code = KUNYU_ERROR_MEMORY;
                snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                         "内存分配失败，无法创建参数数组");
                return NULL;
            }
            
            for (int i = 0; i < expr->arg_count; i++) {
                args[i] = eval_expression(expr->args[i]);
                if (args[i] == NULL) {
                    // 清理已评估的参数
                    for (int j = 0; j < i; j++) {
                        py_decref(args[j]);
                    }
                    free(args);
                    return NULL;
                }
            }
        }
        
        // 调用内置函数
        PyObject *result = builtins_call(expr->name, args, expr->arg_count);
        
        // 清理参数
        if (args != NULL) {
            for (int i = 0; i < expr->arg_count; i++) {
                py_decref(args[i]);
            }
            free(args);
        }
        
        if (result == NULL) {
            interpreter.error.code = KUNYU_ERROR_RUNTIME;
            snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                     "调用内置函数'%s'失败", expr->name);
            return NULL;
        }
        
        return result;
    }
    
    // 如果不是内置函数，则查找用户定义的函数
    FunctionEntry *func = find_function(expr->name);
    if (func == NULL) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "未定义的函数: %s", expr->name);
        return NULL;
    }
    
    // 检查参数数量
    if (expr->arg_count != func->param_count) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "函数'%s'需要%d个参数，但接收到%d个", 
                 expr->name, func->param_count, expr->arg_count);
        return NULL;
    }
    
    // 创建新的作用域
    if (!push_scope()) {
        return NULL;
    }
    
    // 绑定参数
    for (int i = 0; i < expr->arg_count; i++) {
        PyObject *arg_value = eval_expression(expr->args[i]);
        if (arg_value == NULL) {
            pop_scope();
            return NULL;
        }
        
        bool result = define_variable(func->params[i], arg_value, false);
        py_decref(arg_value);
        
        if (!result) {
            pop_scope();
            return NULL;
        }
    }
    
    // 重置返回标志
    interpreter.has_return = false;
    if (interpreter.return_value != NULL) {
        py_decref(interpreter.return_value);
        interpreter.return_value = NULL;
    }
    
    // 执行函数体
    bool success = execute_statement(func->body);
    
    // 保存返回值（如果有）
    PyObject *return_value = NULL;
    if (success && interpreter.has_return && interpreter.return_value != NULL) {
        return_value = interpreter.return_value;
        py_incref(return_value);
    } else if (success && !interpreter.has_return) {
        // 如果没有返回值，默认返回nil
        return_value = NULL;
    }
    
    // 退出作用域
    pop_scope();
    
    // 清除返回值状态
    interpreter.has_return = false;
    if (interpreter.return_value != NULL) {
        py_decref(interpreter.return_value);
        interpreter.return_value = NULL;
    }
    
    if (!success) {
        return NULL;
    }
    
    return return_value;
}

/**
 * 评估分组表达式
 */
static PyObject* eval_grouping_expr(GroupingExpr *expr) {
    return eval_expression(expr->expr);
}

/**
 * 评估变量引用表达式
 */
static PyObject* eval_variable_expr(VariableExpr *expr) {
    PyObject *value = get_variable(expr->name);
    if (value != NULL) {
        py_incref(value);
    }
    return value;
}

/**
 * 评估字面量表达式
 */
static PyObject* eval_literal_expr(LiteralExpr *expr) {
    switch (expr->token_type) {
        case KUNYU_TOKEN_NUMBER: {
            double value = atof(expr->value);
            return create_number_object(value);
        }
        case KUNYU_TOKEN_STRING:
            return create_string_object(expr->value);
        default:
            interpreter.error.code = KUNYU_ERROR_RUNTIME;
            snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                     "不支持的字面量类型");
            return NULL;
    }
}

/**
 * 评估赋值表达式
 */
static PyObject* eval_assign_expr(AssignExpr *expr) {
    // 评估右侧表达式
    PyObject *value = eval_expression(expr->value);
    if (value == NULL) {
        return NULL;
    }
    
    // 设置变量值
    if (!set_variable(expr->name, value)) {
        py_decref(value);
        return NULL;
    }
    
    // 赋值表达式的值就是右侧表达式的值
    return value; // 不需要增加引用计数，因为set_variable已经增加了
}

/**
 * 执行表达式
 */
static PyObject* eval_expression(AstNode *node) {
    if (node == NULL) {
        return NULL;
    }
    
    switch (node->type) {
        case NODE_LITERAL: {
            LiteralExpr *expr = (LiteralExpr *)node;
            return eval_literal_expr(expr);
        }
        case NODE_IDENTIFIER: {
            VariableExpr *expr = (VariableExpr *)node;
            return eval_variable_expr(expr);
        }
        case NODE_BINARY: {
            BinaryExpr *expr = (BinaryExpr *)node;
            return eval_binary_expr(expr);
        }
        case NODE_GROUPING: {
            GroupingExpr *expr = (GroupingExpr *)node;
            return eval_grouping_expr(expr);
        }
        case NODE_CALL: {
            CallExpr *expr = (CallExpr *)node;
            return eval_call_expr(expr);
        }
        case NODE_ASSIGN: {
            AssignExpr *expr = (AssignExpr *)node;
            return eval_assign_expr(expr);
        }
        default: {
            interpreter.error.code = KUNYU_ERROR_RUNTIME;
            snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                     "不支持的表达式类型");
            return NULL;
        }
    }
}

/**
 * 执行语句
 */
static bool execute_statement(AstNode *node) {
    if (node == NULL) {
        return false;
    }
    
    switch (node->type) {
        case NODE_PRINT:
            return exec_print_stmt(node);
        case NODE_VARDECL:
            return exec_var_decl(node);
        case NODE_IF:
            return exec_if_stmt(node);
        case NODE_LOOP:
            return exec_loop_stmt(node);
        case NODE_FUNCDECL:
            return exec_function_decl(node);
        case NODE_BLOCK: {
            BlockStmt *block = (BlockStmt *)node;
            return execute_block(block);
        }
        case NODE_RETURN:
            return exec_return_stmt(node);
        case NODE_PROGRAM:
            if (((StmtNode*)node)->stmt_type == STMT_EXPRESSION) {
                return exec_expression_stmt(node);
            }
            // Fall through for other program node types
        default: {
            interpreter.error.code = KUNYU_ERROR_RUNTIME;
            snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                     "不支持的语句类型");
            return false;
        }
    }
}

/**
 * 执行程序
 */
static bool execute_program(AstNode *program) {
    if (program == NULL || program->type != NODE_PROGRAM) {
        interpreter.error.code = KUNYU_ERROR_RUNTIME;
        snprintf(interpreter.error.message, sizeof(interpreter.error.message), 
                 "预期程序节点");
        return false;
    }
    
    Program *prog = (Program *)program;
    
    // 执行每条语句
    for (int i = 0; i < prog->stmt_count; i++) {
        if (!execute_statement(prog->statements[i])) {
            return false;
        }
        
        // 如果遇到返回语句，停止执行（理论上顶层不应该有返回语句）
        if (interpreter.has_return) {
            break;
        }
    }
    
    return true;
}

/**
 * 执行AST
 * @param root AST根节点
 * @return 成功返回true，失败返回false
 */
bool interpreter_execute(AstNode *root) {
    if (root == NULL) {
        return false;
    }
    
    interpreter_init();
    
    bool result = execute_program(root);
    
    // 不要在这里清理内置函数，因为可能会再次执行
    // builtins_cleanup(); 
    
    return result;
}

/**
 * 清理解释器资源
 */
void interpreter_cleanup() {
    // 清理内置函数
    builtins_cleanup();
    
    // 清理其他资源
    // 这里可以添加其他需要清理的资源
}

/**
 * 获取解释器错误信息
 */
KunyuError* interpreter_get_error() {
    return &interpreter.error;
} 