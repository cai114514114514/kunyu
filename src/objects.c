/**
 * 坤舆编程语言 - 对象系统
 * 实现基本的对象类型和内存管理
 */

#include "../includes/kunyu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 销毁数字对象
 */
static void number_destructor(PyObject *obj) {
    // 数字对象没有需要释放的其他资源
}

/**
 * 销毁字符串对象
 */
static void string_destructor(PyObject *obj) {
    PyStringObject *str = (PyStringObject *)obj;
    free(str->value);
}

/**
 * 销毁列表对象
 */
static void list_destructor(PyObject *obj) {
    PyListObject *list = (PyListObject *)obj;
    // 减少所有项的引用计数
    for (size_t i = 0; i < list->length; i++) {
        py_decref(list->items[i]);
    }
    free(list->items);
}

/**
 * 销毁字典对象
 */
static void dict_destructor(PyObject *obj) {
    PyDictObject *dict = (PyDictObject *)obj;
    // 减少所有键和值的引用计数
    for (size_t i = 0; i < dict->size; i++) {
        py_decref(dict->items[i].key);
        py_decref(dict->items[i].value);
    }
    free(dict->items);
}

/**
 * 增加对象引用计数
 */
void py_incref(PyObject *obj) {
    if (obj != NULL) {
        obj->ref_count++;
    }
}

/**
 * 减少对象引用计数，当计数为0时销毁对象
 */
void py_decref(PyObject *obj) {
    if (obj != NULL) {
        obj->ref_count--;
        if (obj->ref_count <= 0) {
            // 调用对象的析构函数
            if (obj->destructor != NULL) {
                obj->destructor(obj);
            }
            // 释放对象内存
            free(obj);
        }
    }
}

/**
 * 创建一个新的数字对象
 */
PyObject* py_number_new(double value) {
    PyNumberObject *obj = (PyNumberObject *)malloc(sizeof(PyNumberObject));
    if (obj == NULL) {
        return NULL;
    }
    
    obj->base.type = TYPE_NUMBER;
    obj->base.ref_count = 1;
    obj->base.destructor = number_destructor;
    obj->value = value;
    
    return (PyObject *)obj;
}

/**
 * 创建一个新的字符串对象
 */
PyObject* py_string_new(const char *value) {
    if (value == NULL) {
        return NULL;
    }
    
    PyStringObject *obj = (PyStringObject *)malloc(sizeof(PyStringObject));
    if (obj == NULL) {
        return NULL;
    }
    
    obj->base.type = TYPE_STRING;
    obj->base.ref_count = 1;
    obj->base.destructor = string_destructor;
    
    obj->value = strdup(value);
    if (obj->value == NULL) {
        free(obj);
        return NULL;
    }
    
    obj->length = strlen(value);
    
    return (PyObject *)obj;
}

/**
 * 创建一个新的列表对象
 */
PyObject* py_list_new() {
    PyListObject *obj = (PyListObject *)malloc(sizeof(PyListObject));
    if (obj == NULL) {
        return NULL;
    }
    
    obj->base.type = TYPE_LIST;
    obj->base.ref_count = 1;
    obj->base.destructor = list_destructor;
    
    const size_t initial_capacity = 8;
    obj->items = (PyObject **)malloc(sizeof(PyObject *) * initial_capacity);
    if (obj->items == NULL) {
        free(obj);
        return NULL;
    }
    
    obj->length = 0;
    obj->capacity = initial_capacity;
    
    return (PyObject *)obj;
}

/**
 * 向列表添加项
 */
bool py_list_append(PyObject *list, PyObject *item) {
    if (list == NULL || list->type != TYPE_LIST || item == NULL) {
        return false;
    }
    
    PyListObject *list_obj = (PyListObject *)list;
    
    // 检查是否需要扩容
    if (list_obj->length >= list_obj->capacity) {
        size_t new_capacity = list_obj->capacity * 2;
        PyObject **new_items = (PyObject **)realloc(list_obj->items, sizeof(PyObject *) * new_capacity);
        if (new_items == NULL) {
            return false;
        }
        
        list_obj->items = new_items;
        list_obj->capacity = new_capacity;
    }
    
    // 添加项并增加引用计数
    list_obj->items[list_obj->length++] = item;
    py_incref(item);
    
    return true;
}

/**
 * 获取列表的长度
 */
size_t py_list_length(PyObject *list) {
    if (list == NULL || list->type != TYPE_LIST) {
        return 0;
    }
    
    PyListObject *list_obj = (PyListObject *)list;
    return list_obj->length;
}

/**
 * 获取列表中指定索引的项
 */
PyObject* py_list_get(PyObject *list, size_t index) {
    if (list == NULL || list->type != TYPE_LIST) {
        return NULL;
    }
    
    PyListObject *list_obj = (PyListObject *)list;
    if (index >= list_obj->length) {
        return NULL;
    }
    
    PyObject *item = list_obj->items[index];
    py_incref(item);
    return item;
}

/**
 * 设置列表中指定索引的项
 */
bool py_list_set(PyObject *list, size_t index, PyObject *item) {
    if (list == NULL || list->type != TYPE_LIST || item == NULL) {
        return false;
    }
    
    PyListObject *list_obj = (PyListObject *)list;
    if (index >= list_obj->length) {
        return false;
    }
    
    // 减少旧项的引用计数
    py_decref(list_obj->items[index]);
    
    // 设置新项并增加引用计数
    list_obj->items[index] = item;
    py_incref(item);
    
    return true;
}

/**
 * 创建一个新的字典对象
 */
PyObject* py_dict_new() {
    PyDictObject *obj = (PyDictObject *)malloc(sizeof(PyDictObject));
    if (obj == NULL) {
        return NULL;
    }
    
    obj->base.type = TYPE_DICT;
    obj->base.ref_count = 1;
    obj->base.destructor = dict_destructor;
    
    const size_t initial_capacity = 8;
    obj->items = (DictItem *)malloc(sizeof(DictItem) * initial_capacity);
    if (obj->items == NULL) {
        free(obj);
        return NULL;
    }
    
    obj->size = 0;
    obj->capacity = initial_capacity;
    
    return (PyObject *)obj;
}

/**
 * 查找字典中的键位置
 */
static int py_dict_find_index(PyDictObject *dict, PyObject *key) {
    for (size_t i = 0; i < dict->size; i++) {
        // 目前仅支持字符串键的比较
        if (key->type == TYPE_STRING && dict->items[i].key->type == TYPE_STRING) {
            PyStringObject *str_key = (PyStringObject *)key;
            PyStringObject *dict_key = (PyStringObject *)dict->items[i].key;
            
            if (strcmp(str_key->value, dict_key->value) == 0) {
                return i;
            }
        }
    }
    return -1;
}

/**
 * 设置字典中键对应的值
 */
bool py_dict_set(PyObject *dict, PyObject *key, PyObject *value) {
    if (dict == NULL || dict->type != TYPE_DICT || key == NULL || value == NULL) {
        return false;
    }
    
    PyDictObject *dict_obj = (PyDictObject *)dict;
    int index = py_dict_find_index(dict_obj, key);
    
    if (index >= 0) {
        // 替换现有键值对
        py_decref(dict_obj->items[index].value);
        dict_obj->items[index].value = value;
        py_incref(value);
    } else {
        // 检查是否需要扩容
        if (dict_obj->size >= dict_obj->capacity) {
            size_t new_capacity = dict_obj->capacity * 2;
            DictItem *new_items = (DictItem *)realloc(dict_obj->items, sizeof(DictItem) * new_capacity);
            if (new_items == NULL) {
                return false;
            }
            
            dict_obj->items = new_items;
            dict_obj->capacity = new_capacity;
        }
        
        // 添加新键值对
        dict_obj->items[dict_obj->size].key = key;
        dict_obj->items[dict_obj->size].value = value;
        py_incref(key);
        py_incref(value);
        dict_obj->size++;
    }
    
    return true;
}

/**
 * 获取字典中键对应的值
 */
PyObject* py_dict_get(PyObject *dict, PyObject *key) {
    if (dict == NULL || dict->type != TYPE_DICT || key == NULL) {
        return NULL;
    }
    
    PyDictObject *dict_obj = (PyDictObject *)dict;
    int index = py_dict_find_index(dict_obj, key);
    
    if (index >= 0) {
        PyObject *value = dict_obj->items[index].value;
        py_incref(value);
        return value;
    }
    
    return NULL;
}

/**
 * 获取字典的大小
 */
size_t py_dict_size(PyObject *dict) {
    if (dict == NULL || dict->type != TYPE_DICT) {
        return 0;
    }
    
    PyDictObject *dict_obj = (PyDictObject *)dict;
    return dict_obj->size;
} 