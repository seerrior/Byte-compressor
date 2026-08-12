CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -pedantic
CPPFLAGS = -Iinclude -Isrc -D_FILE_OFFSET_BITS=64
BUILD    = build

LIB_SRC = src/bytec_archive.c \
          src/bytec_crc.c \
          src/bytec_format.c \
          src/bytec_io.c \
          src/bytec_lz.c \
          src/bytec_pack.c \
          src/bytec_read.c \
          src/bytec_source.c

LIB_OBJ = $(LIB_SRC:%.c=$(BUILD)/%.o)
LIB     = $(BUILD)/libbytec.a
CLI     = $(BUILD)/bytec
EXAMPLE = $(BUILD)/bytec_example
TEST    = $(BUILD)/bytec_test

all: $(LIB) $(CLI) $(EXAMPLE)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(CLI): tools/bytec_cli.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) -o $@

$(EXAMPLE): examples/runtime_usage.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) -o $@

$(TEST): tests/test_roundtrip.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) -o $@

test: $(TEST)
	./$(TEST)

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
