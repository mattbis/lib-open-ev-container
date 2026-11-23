CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -Isrc.dev/core/include
AR      := ar

BUILD_DIR := var/build
LIB       := $(BUILD_DIR)/liboevc.a

LIB_SRCS := \
	src.dev/core/src/oevc_crc32.c \
	src.dev/core/src/oevc_container.c \
	src.dev/core/src/oevc_sym_il.c \
	src.dev/core/src/oevc_export_json.c

LIB_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))

ifeq ($(OS),Windows_NT)
	EXE := .exe
else
	EXE :=
endif

LOEC_BIN    := $(BUILD_DIR)/loec$(EXE)
EXAMPLE_BIN := $(BUILD_DIR)/basic_daw_example$(EXE)
TEST_BIN    := $(BUILD_DIR)/test_oevc$(EXE)

.PHONY: all test clean

all: $(LIB) $(LOEC_BIN) $(EXAMPLE_BIN)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LOEC_BIN): src.dev/tool/manager/main/loec-main.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

$(EXAMPLE_BIN): doc/examples/basic_daw/basic_daw_example.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

$(TEST_BIN): test/core/test_oevc.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

test: all $(TEST_BIN)
	./$(TEST_BIN)
	./$(EXAMPLE_BIN) $(BUILD_DIR)/basic_daw.oevc
	./$(LOEC_BIN) rtest $(BUILD_DIR)/basic_daw.oevc
	./$(LOEC_BIN) export $(BUILD_DIR)/basic_daw.oevc $(BUILD_DIR)/basic_daw.json

clean:
	rm -rf $(BUILD_DIR)
