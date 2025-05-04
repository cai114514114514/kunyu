# 坤舆编程语言构建脚本

CC = gcc
CFLAGS = -Wall -g -std=c99 -I./includes
LDFLAGS = -lm

# 源文件和目标文件
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
BIN = $(BIN_DIR)/kunyu

# 确保目录存在
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# 默认目标
all: $(BIN)

# 链接目标可执行文件
$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# 编译源文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# 运行测试示例
test: $(BIN)
	$(BIN) hello.kunyu

# 调试运行模式
debug: $(BIN)
	$(BIN) -d hello.kunyu

# 运行交互式REPL
repl: $(BIN)
	$(BIN) -i

# 帮助信息
help:
	@echo "坤舆编程语言构建系统"
	@echo "使用方法:"
	@echo "  make       - 编译项目"
	@echo "  make clean - 清理生成的文件"
	@echo "  make test  - 运行测试示例"
	@echo "  make debug - 以调试模式运行测试示例"
	@echo "  make repl  - 启动交互式解释器"
	@echo "  make help  - 显示此帮助信息"

.PHONY: all clean test debug repl help 