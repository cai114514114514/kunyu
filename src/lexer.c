/**
 * 坤舆编程语言 - 词法分析器
 * 将源代码转换为标记流
 */

#include "../includes/kunyu.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * 关键字表
 */
static const char *keywords[] = {
    "变量", "常量", "如果", "否则", "循环", "函数", "返回", "输出"
};

/**
 * 词法分析器上下文
 */
typedef struct {
    const char *source;      // 源代码
    size_t source_len;       // 源代码长度
    size_t pos;              // 当前位置
    int line;                // 当前行号
    int column;              // 当前列号
    Token *tokens;           // 标记数组
    size_t token_count;      // 标记数量
    size_t token_capacity;   // 标记容量
    KunyuError error;        // 错误信息
} LexerContext;

// 全局词法分析器上下文
static LexerContext lexer;

/**
 * 初始化词法分析器
 * @param source 源代码字符串
 * @return 标记数组指针，失败返回NULL
 */
Token* lexer_init(const char *source) {
    if (source == NULL) {
        return NULL;
    }

    // 初始化词法分析器上下文
    lexer.source = source;
    lexer.source_len = strlen(source);
    lexer.pos = 0;
    lexer.line = 1;
    lexer.column = 1;
    lexer.token_count = 0;
    lexer.token_capacity = 1024;  // 初始容量
    
    // 分配标记数组
    lexer.tokens = (Token*)malloc(sizeof(Token) * lexer.token_capacity);
    if (lexer.tokens == NULL) {
        return NULL;
    }
    
    // 清空错误信息
    lexer.error.code = KUNYU_OK;
    lexer.error.message[0] = '\0';
    lexer.error.line = 0;
    lexer.error.column = 0;
    
    return lexer.tokens;
}

/**
 * 释放词法分析器资源
 */
void lexer_free() {
    if (lexer.tokens != NULL) {
        // 释放每个标记的值
        for (size_t i = 0; i < lexer.token_count; i++) {
            if (lexer.tokens[i].value != NULL) {
                free(lexer.tokens[i].value);
            }
        }
        
        // 释放标记数组
        free(lexer.tokens);
        lexer.tokens = NULL;
    }
    
    lexer.source = NULL;
    lexer.token_count = 0;
    lexer.token_capacity = 0;
}

/**
 * 检查当前字符是否到达源码结尾
 */
static bool is_eof() {
    return lexer.pos >= lexer.source_len;
}

/**
 * 获取当前字符
 */
static char current_char() {
    if (is_eof()) {
        return '\0';
    }
    return lexer.source[lexer.pos];
}

/**
 * 获取下一个字符（不改变位置）
 */
static char peek_char() {
    if (lexer.pos + 1 >= lexer.source_len) {
        return '\0';
    }
    return lexer.source[lexer.pos + 1];
}

/**
 * 前进一个字符
 */
static void advance() {
    if (is_eof()) {
        return;
    }
    
    char c = lexer.source[lexer.pos];
    lexer.pos++;
    
    if (c == '\n') {
        lexer.line++;
        lexer.column = 1;
    } else {
        lexer.column++;
    }
}

/**
 * 添加一个标记
 */
static bool add_token(KunyuTokenType type, const char *value, int line, int column) {
    // 如果标记数组满了，则扩容
    if (lexer.token_count >= lexer.token_capacity) {
        size_t new_capacity = lexer.token_capacity * 2;
        Token *new_tokens = (Token*)realloc(lexer.tokens, sizeof(Token) * new_capacity);
        if (new_tokens == NULL) {
            lexer.error.code = KUNYU_ERROR_MEMORY;
            snprintf(lexer.error.message, sizeof(lexer.error.message), 
                     "内存分配失败，无法扩容标记数组");
            return false;
        }
        
        lexer.tokens = new_tokens;
        lexer.token_capacity = new_capacity;
    }
    
    // 分配标记值的内存并复制
    char *token_value = NULL;
    if (value != NULL) {
        token_value = strdup(value);
        if (token_value == NULL) {
            lexer.error.code = KUNYU_ERROR_MEMORY;
            snprintf(lexer.error.message, sizeof(lexer.error.message), 
                     "内存分配失败，无法复制标记值");
            return false;
        }
    }
    
    // 添加标记
    lexer.tokens[lexer.token_count].type = type;
    lexer.tokens[lexer.token_count].value = token_value;
    lexer.tokens[lexer.token_count].line = line;
    lexer.tokens[lexer.token_count].column = column;
    
    lexer.token_count++;
    return true;
}

/**
 * 跳过空白字符
 */
static void skip_whitespace() {
    while (!is_eof()) {
        char c = current_char();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

/**
 * 跳过注释
 */
static void skip_comment() {
    // 跳过 #
    advance();
    
    // 跳到行尾或文件结束
    while (!is_eof()) {
        if (current_char() == '\n') {
            break;
        }
        advance();
    }
}

/**
 * 判断是否是字母或下划线
 */
static bool is_alpha(char c) {
    return isalpha(c) || c == '_';
}

/**
 * 判断是否是数字
 */
static bool is_digit(char c) {
    return isdigit(c);
}

/**
 * 判断是否是字母、数字或下划线
 */
static bool is_alnum(char c) {
    return isalnum(c) || c == '_';
}

/**
 * 判断是否是UTF-8多字节序列的开始
 */
static bool is_utf8_start(char c) {
    return (c & 0xC0) == 0xC0;
}

/**
 * 判断是否是UTF-8多字节序列的后续字节
 */
static bool is_utf8_continuation(char c) {
    return (c & 0xC0) == 0x80;
}

/**
 * 获取UTF-8字符的字节数
 */
static int utf8_char_length(char c) {
    if ((c & 0x80) == 0) return 1;      // 0xxxxxxx
    if ((c & 0xE0) == 0xC0) return 2;   // 110xxxxx
    if ((c & 0xF0) == 0xE0) return 3;   // 1110xxxx
    if ((c & 0xF8) == 0xF0) return 4;   // 11110xxx
    return 1; // 非法UTF-8字符，当作单字节处理
}

/**
 * 读取一个完整的UTF-8字符
 */
static char* read_utf8_char() {
    if (is_eof()) {
        return NULL;
    }
    
    char c = current_char();
    int len = utf8_char_length(c);
    
    // 检查是否有足够的字符
    if (lexer.pos + len > lexer.source_len) {
        return NULL;
    }
    
    // 分配内存
    char *utf8_char = (char*)malloc(len + 1);
    if (utf8_char == NULL) {
        return NULL;
    }
    
    // 复制字符
    for (int i = 0; i < len; i++) {
        utf8_char[i] = lexer.source[lexer.pos + i];
    }
    utf8_char[len] = '\0';
    
    // 前进
    for (int i = 0; i < len; i++) {
        advance();
    }
    
    return utf8_char;
}

/**
 * 判断一个UTF-8字符串是否是关键字
 */
static int is_keyword(const char *str) {
    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * 读取一个标识符
 */
static char* read_identifier() {
    size_t start_pos = lexer.pos;
    size_t length = 0;
    
    // 读取第一个字符
    char *first_char = read_utf8_char();
    if (first_char == NULL) {
        return NULL;
    }
    free(first_char);
    
    // 读取后续字符
    while (!is_eof()) {
        char c = current_char();
        
        // ASCII字符
        if (!is_utf8_start(c)) {
            if (!is_alnum(c)) {
                break;
            }
            advance();
        } else {
            // UTF-8字符
            char *utf8_char = read_utf8_char();
            if (utf8_char == NULL) {
                break;
            }
            free(utf8_char);
        }
    }
    
    // 计算标识符长度
    length = lexer.pos - start_pos;
    
    // 复制标识符
    char *identifier = (char*)malloc(length + 1);
    if (identifier == NULL) {
        return NULL;
    }
    
    strncpy(identifier, lexer.source + start_pos, length);
    identifier[length] = '\0';
    
    return identifier;
}

/**
 * 读取一个数字
 */
static char* read_number() {
    size_t start_pos = lexer.pos;
    bool has_dot = false;
    
    // 读取数字部分
    while (!is_eof()) {
        char c = current_char();
        
        if (is_digit(c)) {
            advance();
        } else if (c == '.' && !has_dot) {
            has_dot = true;
            advance();
        } else {
            break;
        }
    }
    
    // 计算数字长度
    size_t length = lexer.pos - start_pos;
    
    // 复制数字
    char *number = (char*)malloc(length + 1);
    if (number == NULL) {
        return NULL;
    }
    
    strncpy(number, lexer.source + start_pos, length);
    number[length] = '\0';
    
    return number;
}

/**
 * 读取一个字符串
 */
static char* read_string() {
    // 跳过开始的引号
    advance();
    
    size_t start_pos = lexer.pos;
    size_t length = 0;
    
    // 读取字符串内容
    while (!is_eof() && current_char() != '"') {
        // 处理转义序列
        if (current_char() == '\\') {
            advance();
            if (is_eof()) {
                break;
            }
        }
        advance();
    }
    
    // 计算字符串长度
    length = lexer.pos - start_pos;
    
    // 复制字符串内容
    char *string = (char*)malloc(length + 1);
    if (string == NULL) {
        return NULL;
    }
    
    strncpy(string, lexer.source + start_pos, length);
    string[length] = '\0';
    
    // 跳过结束的引号
    if (!is_eof() && current_char() == '"') {
        advance();
    }
    
    return string;
}

/**
 * 读取下一个标记
 */
static bool read_next_token() {
    skip_whitespace();
    
    if (is_eof()) {
        return add_token(KUNYU_TOKEN_EOF, "", lexer.line, lexer.column);
    }
    
    char c = current_char();
    int line = lexer.line;
    int column = lexer.column;
    
    // 处理注释
    if (c == '#') {
        skip_comment();
        return true;  // 继续读取下一个标记
    }
    
    // 处理换行
    if (c == '\n') {
        advance();
        return add_token(KUNYU_TOKEN_NEWLINE, "\n", line, column);
    }
    
    // 处理标识符和关键字
    if (is_alpha(c) || is_utf8_start(c)) {
        char *identifier = read_identifier();
        if (identifier == NULL) {
            lexer.error.code = KUNYU_ERROR_LEXER;
            snprintf(lexer.error.message, sizeof(lexer.error.message), 
                     "读取标识符时发生错误");
            lexer.error.line = line;
            lexer.error.column = column;
            return false;
        }
        
        // 检查是否是关键字
        int keyword_index = is_keyword(identifier);
        if (keyword_index >= 0) {
            bool result = add_token(KUNYU_TOKEN_KEYWORD, identifier, line, column);
            free(identifier);
            return result;
        } else {
            bool result = add_token(KUNYU_TOKEN_IDENTIFIER, identifier, line, column);
            free(identifier);
            return result;
        }
    }
    
    // 处理数字
    if (is_digit(c)) {
        char *number = read_number();
        if (number == NULL) {
            lexer.error.code = KUNYU_ERROR_LEXER;
            snprintf(lexer.error.message, sizeof(lexer.error.message), 
                     "读取数字时发生错误");
            lexer.error.line = line;
            lexer.error.column = column;
            return false;
        }
        
        bool result = add_token(KUNYU_TOKEN_NUMBER, number, line, column);
        free(number);
        return result;
    }
    
    // 处理字符串
    if (c == '"') {
        char *string = read_string();
        if (string == NULL) {
            lexer.error.code = KUNYU_ERROR_LEXER;
            snprintf(lexer.error.message, sizeof(lexer.error.message), 
                     "读取字符串时发生错误");
            lexer.error.line = line;
            lexer.error.column = column;
            return false;
        }
        
        bool result = add_token(KUNYU_TOKEN_STRING, string, line, column);
        free(string);
        return result;
    }
    
    // 处理操作符和分隔符
    switch (c) {
        case '=':
            advance();
            if (current_char() == '=') {
                advance();
                return add_token(KUNYU_TOKEN_OPERATOR, "==", line, column);
            }
            return add_token(KUNYU_TOKEN_OPERATOR, "=", line, column);
            
        case '+':
            advance();
            return add_token(KUNYU_TOKEN_OPERATOR, "+", line, column);
            
        case '-':
            advance();
            return add_token(KUNYU_TOKEN_OPERATOR, "-", line, column);
            
        case '*':
            advance();
            return add_token(KUNYU_TOKEN_OPERATOR, "*", line, column);
            
        case '/':
            advance();
            return add_token(KUNYU_TOKEN_OPERATOR, "/", line, column);
            
        case '%':
            advance();
            return add_token(KUNYU_TOKEN_OPERATOR, "%", line, column);
            
        case '<':
            advance();
            if (current_char() == '=') {
                advance();
                return add_token(KUNYU_TOKEN_OPERATOR, "<=", line, column);
            }
            return add_token(KUNYU_TOKEN_OPERATOR, "<", line, column);
            
        case '>':
            advance();
            if (current_char() == '=') {
                advance();
                return add_token(KUNYU_TOKEN_OPERATOR, ">=", line, column);
            }
            return add_token(KUNYU_TOKEN_OPERATOR, ">", line, column);
            
        case '!':
            advance();
            if (current_char() == '=') {
                advance();
                return add_token(KUNYU_TOKEN_OPERATOR, "!=", line, column);
            }
            return add_token(KUNYU_TOKEN_OPERATOR, "!", line, column);
            
        case '&':
            advance();
            if (current_char() == '&') {
                advance();
                return add_token(KUNYU_TOKEN_OPERATOR, "&&", line, column);
            }
            return add_token(KUNYU_TOKEN_OPERATOR, "&", line, column);
            
        case '|':
            advance();
            if (current_char() == '|') {
                advance();
                return add_token(KUNYU_TOKEN_OPERATOR, "||", line, column);
            }
            return add_token(KUNYU_TOKEN_OPERATOR, "|", line, column);
            
        case '(':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, "(", line, column);
            
        case ')':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, ")", line, column);
            
        case '{':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, "{", line, column);
            
        case '}':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, "}", line, column);
            
        case '[':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, "[", line, column);
            
        case ']':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, "]", line, column);
            
        case ',':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, ",", line, column);
            
        case '.':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, ".", line, column);
            
        case ';':
            advance();
            return add_token(KUNYU_TOKEN_DELIMITER, ";", line, column);
            
        default:
            // 未知字符
            lexer.error.code = KUNYU_ERROR_LEXER;
            snprintf(lexer.error.message, sizeof(lexer.error.message), 
                     "未知字符: %c", c);
            lexer.error.line = line;
            lexer.error.column = column;
            advance();  // 跳过未知字符
            return false;
    }
}

/**
 * 执行词法分析
 * @return 成功返回标记数量，失败返回-1
 */
int lexer_tokenize() {
    // 重置标记计数
    lexer.token_count = 0;
    
    // 读取所有标记
    while (true) {
        if (!read_next_token()) {
            return -1;
        }
        
        // 如果读到了EOF标记，结束
        if (lexer.token_count > 0 && 
            lexer.tokens[lexer.token_count - 1].type == KUNYU_TOKEN_EOF) {
            break;
        }
    }
    
    return lexer.token_count;
}

/**
 * 获取下一个标记
 * @return 下一个标记指针，失败返回NULL
 */
Token* lexer_next_token() {
    static size_t current_token = 0;
    
    if (lexer.tokens == NULL || current_token >= lexer.token_count) {
        return NULL;
    }
    
    return &lexer.tokens[current_token++];
}

/**
 * 获取词法分析器错误信息
 * @return 错误信息结构体指针
 */
KunyuError* lexer_get_error() {
    return &lexer.error;
}

/**
 * 获取标记数组
 * @return 标记数组指针
 */
Token* lexer_get_tokens() {
    return lexer.tokens;
}

/**
 * 获取标记数量
 * @return 标记数量
 */
size_t lexer_get_token_count() {
    return lexer.token_count;
} 