# Name of final executable file
TARGET_EXEC := deep-messenger
# Name of directory where build files should be created
BUILD_DIR := build
# Directories containing source files
SRC_DIRS := src
# Directories containing header files
INC_DIRS := include
# Directory containing test files
TEST_DIR := tests

# Compiler
CC := gcc

# Directory remove command
RM := rm -rf
# Directory create command
MKDIR := mkdir -p

# Find the file with main function in source
MAIN_SRC := $(shell grep -l "int\s*main\s*(.*)" $(SRC_DIRS:%=%/*))
MAIN_OBJ := $(MAIN_SRC:%=$(BUILD_DIR)/%.o)
MAIN_DEP := $(MAIN_OBJ:.o=.d)

# Find all source files
SRCS := $(shell find $(SRC_DIRS) -name '*.c' ! -wholename $(MAIN_SRC))
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Find all test files
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.c')
TEST_OBJS := $(TEST_SRCS:%=$(BUILD_DIR)/%.o)
TEST_DEPS := $(TEST_OBJS:.o=.d)
TEST_BINS := $(TEST_SRCS:%=$(BUILD_DIR)/%.bin)

INC_FLAGS := $(addprefix -I, $(INC_DIRS))

# Preprocessor flags
CPPFLAGS := $(INC_FLAGS) -MMD -MP -D NCURSES_WIDECHAR
# Compiler flags
CFLAGS :=
# Linker flags
LDFLAGS := -lncursesw -lsqlite3 -lcrypto -levent

.PHONY: clean test.ls test.run.ls
.SECONDARY: $(TEST_BINS) $(TEST_OBJS)

# Link all object files into executable
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS) $(MAIN_OBJ)
	$(CC) $(OBJS) $(MAIN_OBJ) -o $@ $(LDFLAGS)

# Compiling all .c source files
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Run test with given name
test.run.%: $(BUILD_DIR)/$(TEST_DIR)/%.c.bin
	@echo Compilation done running test: $(@:test.run.%=%)
	@echo
	@./$(BUILD_DIR)/$(TEST_DIR)/$(@:test.run.%=%).c.bin

# Compiling test binaries
$(BUILD_DIR)/%.c.bin: $(OBJS) $(BUILD_DIR)/%.c.o
	$(MKDIR) $(dir $@)
	$(CC) $(OBJS) $(@:.bin=.o) -o $@ $(LDFLAGS)

# Show list of all available tests
test.ls:
	@echo List of all available tests:
	@echo $(TEST_SRCS:$(TEST_DIR)/%.c=%)

# Delete all build files
clean:
	$(RM) $(BUILD_DIR)

# Include generated dependency files
-include $(DEPS)
-include $(MAIN_DEP)
-include $(TEST_DEPS)