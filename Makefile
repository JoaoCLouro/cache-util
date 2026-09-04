# ============================================================================
# Makefile for Cache Library, Test Suite & Benchmarks (NASM & GCC/LD)
# ============================================================================

CC        = gcc
NASM      = nasm
CFLAGS    = -Wall -Wextra -std=c99 -g -Iinclude
NASMFLAGS = -f elf64 -g
LDFLAGS   = -lpthread -no-pie

# Directories
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
BIN_DIR   = bin
TEST_DIR  = tests

# Targets
TEST_CACHE        = $(BIN_DIR)/test_cache
TEST_DIRECT_CACHE = $(BIN_DIR)/test_direct_cache
BENCHMARK_TARGET  = $(BIN_DIR)/benchmark_cache

# Source & Object Files (.asm for assembly, .c for C sources)
C_SRCS    = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS  = $(wildcard $(SRC_DIR)/*.asm)

C_OBJS    = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS  = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%_asm.o, $(ASM_SRCS))
LIB_OBJS  = $(C_OBJS) $(ASM_OBJS)

# Default rule
all: dirs
	@echo "Detected C Sources: $(C_SRCS)"
	@echo "Detected ASM Sources: $(ASM_SRCS)"
	@$(MAKE) --no-print-directory $(TEST_CACHE) $(TEST_DIRECT_CACHE) $(BENCHMARK_TARGET)

# Create necessary directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# Compile C source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble NASM source files (.asm) to unique object files to prevent name clashes
$(BUILD_DIR)/%_asm.o: $(SRC_DIR)/%.asm | dirs
	$(NASM) $(NASMFLAGS) $< -o $@

# Build high-level test executable
$(TEST_CACHE): $(BUILD_DIR)/test_cache.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build direct interface test executable
$(TEST_DIRECT_CACHE): $(BUILD_DIR)/test_direct_cache.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build benchmark executable
$(BENCHMARK_TARGET): $(BUILD_DIR)/benchmark_cache.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile test and benchmark files separately to avoid multiple definitions of main
$(BUILD_DIR)/test_cache.o: $(TEST_DIR)/test_cache.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_direct_cache.o: $(TEST_DIR)/test_direct_cache.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/benchmark_cache.o: $(TEST_DIR)/benchmark_cache.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Phony targets
.PHONY: all clean test benchmark valgrind

# Run both test suites
test: all
	@echo "=== Running High-Level Test Suite ==="
	./$(TEST_CACHE)
	@echo "=== Running Direct Cache Test Suite ==="
	./$(TEST_DIRECT_CACHE)

# Run the benchmark performance analyzer
benchmark: all
	@echo "=== Running Cache Benchmark Analyzer ==="
	./$(BENCHMARK_TARGET)

# Clean up build artifacts and binaries
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Memory check execution
valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST_CACHE)