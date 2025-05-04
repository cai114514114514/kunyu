/**
 * 坤舆编程语言 - 内置函数实现
 * 提供基本的内置函数和数据结构操作
 */

#include "../includes/kunyu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 内置函数表
typedef struct BuiltinFunc {
    const char *name;              // 函数名
    PyObject* (*func)(PyObject **args, int arg_count);  // 函数指针
    int arg_count;                 // 参数数量
    struct BuiltinFunc *next;      // 下一个函数
} BuiltinFunc;

// 全局内置函数表
static BuiltinFunc *builtin_funcs = NULL;

/**
 * 初始化内置函数表
 */
static void register_builtin(const char *name, PyObject* (*func)(PyObject **args, int arg_count), int arg_count) {
    BuiltinFunc *new_func = malloc(sizeof(BuiltinFunc));
    if (new_func == NULL) {
        return;
    }
    
    new_func->name = strdup(name);
    new_func->func = func;
    new_func->arg_count = arg_count;
    new_func->next = builtin_funcs;
    
    builtin_funcs = new_func;
}

/**
 * 查找内置函数
 */
static BuiltinFunc* find_builtin(const char *name) {
    BuiltinFunc *func = builtin_funcs;
    while (func != NULL) {
        if (strcmp(func->name, name) == 0) {
            return func;
        }
        func = func->next;
    }
    return NULL;
}

/**
 * 释放内置函数表
 */
void builtins_cleanup() {
    BuiltinFunc *func = builtin_funcs;
    while (func != NULL) {
        BuiltinFunc *next = func->next;
        free((void*)func->name);
        free(func);
        func = next;
    }
    builtin_funcs = NULL;
}

/**
 * 内置函数：创建列表
 */
static PyObject* builtin_create_list(PyObject **args, int arg_count) {
    return py_list_new();
}

/**
 * 内置函数：列表添加
 */
static PyObject* builtin_list_append(PyObject **args, int arg_count) {
    if (arg_count != 2 || args[0] == NULL || args[1] == NULL) {
        return NULL;
    }
    
    PyObject *list = args[0];
    PyObject *item = args[1];
    
    if (list->type != TYPE_LIST) {
        return NULL;
    }
    
    bool result = py_list_append(list, item);
    return py_number_new(result ? 1 : 0);
}

/**
 * 内置函数：列表长度
 */
static PyObject* builtin_list_length(PyObject **args, int arg_count) {
    if (arg_count != 1 || args[0] == NULL) {
        return NULL;
    }
    
    PyObject *list = args[0];
    
    if (list->type != TYPE_LIST) {
        return NULL;
    }
    
    size_t length = py_list_length(list);
    return py_number_new((double)length);
}

/**
 * 内置函数：列表获取
 */
static PyObject* builtin_list_get(PyObject **args, int arg_count) {
    if (arg_count != 2 || args[0] == NULL || args[1] == NULL) {
        return NULL;
    }
    
    PyObject *list = args[0];
    PyObject *index_obj = args[1];
    
    if (list->type != TYPE_LIST || index_obj->type != TYPE_NUMBER) {
        return NULL;
    }
    
    PyNumberObject *index_num = (PyNumberObject *)index_obj;
    size_t index = (size_t)index_num->value;
    
    return py_list_get(list, index);
}

/**
 * 内置函数：列表设置
 */
static PyObject* builtin_list_set(PyObject **args, int arg_count) {
    if (arg_count != 3 || args[0] == NULL || args[1] == NULL || args[2] == NULL) {
        return NULL;
    }
    
    PyObject *list = args[0];
    PyObject *index_obj = args[1];
    PyObject *value = args[2];
    
    if (list->type != TYPE_LIST || index_obj->type != TYPE_NUMBER) {
        return NULL;
    }
    
    PyNumberObject *index_num = (PyNumberObject *)index_obj;
    size_t index = (size_t)index_num->value;
    
    bool result = py_list_set(list, index, value);
    return py_number_new(result ? 1 : 0);
}

/**
 * 内置函数：创建字典
 */
static PyObject* builtin_create_dict(PyObject **args, int arg_count) {
    return py_dict_new();
}

/**
 * 内置函数：字典设置
 */
static PyObject* builtin_dict_set(PyObject **args, int arg_count) {
    if (arg_count != 3 || args[0] == NULL || args[1] == NULL || args[2] == NULL) {
        return NULL;
    }
    
    PyObject *dict = args[0];
    PyObject *key = args[1];
    PyObject *value = args[2];
    
    if (dict->type != TYPE_DICT) {
        return NULL;
    }
    
    bool result = py_dict_set(dict, key, value);
    return py_number_new(result ? 1 : 0);
}

/**
 * 内置函数：字典获取
 */
static PyObject* builtin_dict_get(PyObject **args, int arg_count) {
    if (arg_count != 2 || args[0] == NULL || args[1] == NULL) {
        return NULL;
    }
    
    PyObject *dict = args[0];
    PyObject *key = args[1];
    
    if (dict->type != TYPE_DICT) {
        return NULL;
    }
    
    PyObject *value = py_dict_get(dict, key);
    if (value == NULL) {
        // 如果键不存在，返回null对象
        return NULL;
    }
    
    return value;
}

/**
 * 内置函数：字典大小
 */
static PyObject* builtin_dict_size(PyObject **args, int arg_count) {
    if (arg_count != 1 || args[0] == NULL) {
        return NULL;
    }
    
    PyObject *dict = args[0];
    
    if (dict->type != TYPE_DICT) {
        return NULL;
    }
    
    size_t size = py_dict_size(dict);
    return py_number_new((double)size);
}

/**
 * 调用内置函数
 */
PyObject* builtins_call(const char *name, PyObject **args, int arg_count) {
    BuiltinFunc *func = find_builtin(name);
    if (func == NULL) {
        return NULL;
    }
    
    if (func->arg_count >= 0 && func->arg_count != arg_count) {
        return NULL;
    }
    
    return func->func(args, arg_count);
}

/**
 * 初始化内置函数
 */
void builtins_init() {
    // 列表操作
    register_builtin("创建列表", builtin_create_list, 0);
    register_builtin("列表添加", builtin_list_append, 2);
    register_builtin("列表长度", builtin_list_length, 1);
    register_builtin("列表获取", builtin_list_get, 2);
    register_builtin("列表设置", builtin_list_set, 3);
    
    // 字典操作
    register_builtin("创建字典", builtin_create_dict, 0);
    register_builtin("字典设置", builtin_dict_set, 3);
    register_builtin("字典获取", builtin_dict_get, 2);
    register_builtin("字典大小", builtin_dict_size, 1);
    
    // 数学函数
    // 可在此添加更多内置函数
}

/**
 * 检查是否是内置函数
 */
bool builtins_is_builtin(const char *name) {
    return find_builtin(name) != NULL;
} 