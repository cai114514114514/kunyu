/**
 * 坤舆编程语言 - 主程序
 * 命令行入口点和参数处理
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

// 函数声明
struct AstNode* parser_parse(Token *tokens, size_t token_count);
KunyuError* parser_get_error();
bool interpreter_execute(AstNode *root);
KunyuError* interpreter_get_error();

/**
 * 命令行选项
 */
typedef struct {
    bool help;               // 显示帮助信息
    bool version;            // 显示版本信息
    bool compile_only;       // 只编译不运行
    bool debug;              // 调试模式
    bool interactive;        // 交互式模式
    const char *input_file;  // 输入文件名
    const char *output_file; // 输出文件名
} CommandOptions;

/**
 * 显示版本信息
 */
static void show_version() {
    printf("%s v%s\n", KUNYU_NAME, KUNYU_VERSION);
}

/**
 * 显示帮助信息
 */
static void show_help(const char *program_name) {
    printf("用法: %s [选项] 文件名\n\n", program_name);
    printf("选项:\n");
    printf("  -h, --help         显示帮助信息\n");
    printf("  -v, --version      显示版本信息\n");
    printf("  -c, --compile      只编译不运行\n");
    printf("  -o, --output 文件名 指定输出文件名\n");
    printf("  -d, --debug        调试模式\n");
    printf("  -i, --interactive  启动交互式REPL环境\n");
    printf("\n");
}

/**
 * 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数数组
 * @param options 选项结构体指针
 * @return 成功返回true，失败返回false
 */
static bool parse_args(int argc, char *argv[], CommandOptions *options) {
    // 初始化选项
    options->help = false;
    options->version = false;
    options->compile_only = false;
    options->debug = false;
    options->interactive = false;
    options->input_file = NULL;
    options->output_file = NULL;
    
    // 至少需要一个参数（程序名）
    if (argc < 1) {
        return false;
    }
    
    // 只有程序名，显示帮助
    if (argc == 1) {
        options->help = true;
        return true;
    }
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            options->help = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            options->version = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) {
            options->compile_only = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            options->debug = true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            options->interactive = true;
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            options->output_file = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "错误: 未知选项 '%s'\n", argv[i]);
            return false;
        } else {
            // 输入文件
            if (options->input_file == NULL) {
                options->input_file = argv[i];
            } else {
                fprintf(stderr, "错误: 只能指定一个输入文件\n");
                return false;
            }
        }
    }
    
    return true;
}

/**
 * 读取文件内容
 * @param filename 文件名
 * @return 文件内容字符串，使用后需要释放
 */
static char* read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "错误: 无法打开文件 '%s'\n", filename);
        return NULL;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 分配内存
    char *buffer = (char *)malloc(size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "错误: 内存分配失败\n");
        fclose(file);
        return NULL;
    }
    
    // 读取文件内容
    size_t read_size = fread(buffer, 1, size, file);
    if (read_size != size) {
        fprintf(stderr, "警告: 读取文件大小与预期不符\n");
    }
    
    // 添加字符串结束符
    buffer[read_size] = '\0';
    
    fclose(file);
    return buffer;
}

/**
 * 处理词法分析错误
 * @param error 错误信息
 */
static void handle_lexer_error(KunyuError *error) {
    fprintf(stderr, "词法分析错误: %s (行 %d, 列 %d)\n", 
            error->message, error->line, error->column);
}

/**
 * 处理语法分析错误
 * @param error 错误信息
 */
static void handle_parser_error(KunyuError *error) {
    fprintf(stderr, "语法分析错误: %s (行 %d, 列 %d)\n", 
            error->message, error->line, error->column);
}

/**
 * 处理解释器错误
 * @param error 错误信息
 */
static void handle_interpreter_error(KunyuError *error) {
    fprintf(stderr, "运行时错误: %s (行 %d, 列 %d)\n", 
            error->message, error->line, error->column);
}

/**
 * 打印标记类型
 */
static const char* token_type_str(KunyuTokenType type) {
    switch (type) {
        case KUNYU_TOKEN_EOF:        return "EOF";
        case KUNYU_TOKEN_IDENTIFIER: return "标识符";
        case KUNYU_TOKEN_KEYWORD:    return "关键字";
        case KUNYU_TOKEN_STRING:     return "字符串";
        case KUNYU_TOKEN_NUMBER:     return "数字";
        case KUNYU_TOKEN_OPERATOR:   return "运算符";
        case KUNYU_TOKEN_DELIMITER:  return "分隔符";
        case KUNYU_TOKEN_NEWLINE:    return "换行";
        default:                     return "未知";
    }
}

/**
 * 打印标记
 * @param token 标记指针
 */
static void print_token(const Token *token) {
    printf("%-10s | %-10s | 行 %-4d | 列 %-4d\n", 
           token_type_str(token->type), 
           token->value, 
           token->line, 
           token->column);
}

/**
 * 调试模式下打印所有标记
 * @param tokens 标记数组
 * @param count 标记数量
 */
static void print_tokens(const Token *tokens, size_t count) {
    printf("\n=== 标记列表 ===\n");
    printf("%-10s | %-10s | %-7s | %-7s\n", "类型", "值", "行", "列");
    printf("-------------------------------------\n");
    
    for (size_t i = 0; i < count; i++) {
        print_token(&tokens[i]);
    }
    
    printf("=== 共 %zu 个标记 ===\n\n", count);
}

/**
 * 设置控制台支持UTF-8输出
 */
static void setup_console_utf8() {
#ifdef _WIN32
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(65001);
#endif
}

/**
 * 主程序入口
 */
int main(int argc, char *argv[]) {
    // 设置控制台以支持UTF-8输出
    setup_console_utf8();
    
    CommandOptions options;
    
    // 解析命令行参数
    if (!parse_args(argc, argv, &options)) {
        show_help(argv[0]);
        return 1;
    }
    
    // 显示帮助或版本信息
    if (options.help) {
        show_help(argv[0]);
        return 0;
    }
    
    if (options.version) {
        show_version();
        return 0;
    }
    
    // 交互模式
    if (options.interactive) {
        repl_start();
        // 清理资源
        interpreter_cleanup();
        return 0;
    }
    
    // 检查是否提供了输入文件
    if (options.input_file == NULL) {
        fprintf(stderr, "错误: 未指定输入文件\n");
        show_help(argv[0]);
        return 1;
    }
    
    // 读取输入文件
    char *source = read_file(options.input_file);
    if (source == NULL) {
        return 1;
    }
    
    // 初始化词法分析器
    Token *tokens = lexer_init(source);
    if (tokens == NULL) {
        fprintf(stderr, "错误: 初始化词法分析器失败\n");
        free(source);
        return 1;
    }
    
    // 执行词法分析
    int token_count = lexer_tokenize();
    if (token_count < 0) {
        KunyuError *error = lexer_get_error();
        handle_lexer_error(error);
        lexer_free();
        free(source);
        return 1;
    }
    
    // 调试模式打印标记
    if (options.debug) {
        print_tokens(tokens, token_count);
        printf("\n=== 开始执行程序 ===\n\n");
    }
    
    // 语法分析
    AstNode *ast = parser_parse(tokens, token_count);
    if (ast == NULL) {
        KunyuError *error = parser_get_error();
        if (error->code != KUNYU_OK) {
            handle_parser_error(error);
        } else {
            fprintf(stderr, "错误: 语法分析失败，无法生成AST\n");
        }
        lexer_free();
        free(source);
        return 1;
    }
    
    // 执行程序（除非是仅编译模式）
    if (!options.compile_only) {
        if (!interpreter_execute(ast)) {
            KunyuError *error = interpreter_get_error();
            handle_interpreter_error(error);
            ast_free(ast);
            lexer_free();
            free(source);
            interpreter_cleanup(); // 清理解释器资源
            return 1;
        }
        
        if (options.debug) {
            printf("\n=== 程序执行完成 ===\n");
        }
    }
    
    // 释放资源
    ast_free(ast);
    lexer_free();
    free(source);
    interpreter_cleanup(); // 清理解释器资源
    
    return 0;
} 