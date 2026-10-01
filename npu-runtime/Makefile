# npu-runtime — reusable NPU execution layer (builds a static+shared library and
# the benchmark/example/test tools). Nothing here writes outside this tree.
#
# Overridable:
#   NPUNLOCK_ROOT   unpacked npunlock source tree (headers, ld script, carrier)
#   NPUNLOCK_BUILD  build tree holding libnpunlock.so (patchblob implementation)
#   BUILD           output directory (default build)

NPUNLOCK_ROOT  ?= /home/asdf/projects/npunlock-work/npunlock
NPUNLOCK_BUILD ?= $(NPUNLOCK_ROOT)/build/fresh
BUILD          ?= build
CC             ?= cc
CXX            ?= c++

CFLAGS ?= -O2
CFLAGS += -std=gnu99 -Wall -Wextra -fPIC -D_GNU_SOURCE \
          -DNPURT_DEFAULT_NPUNLOCK_ROOT='"$(NPUNLOCK_ROOT)"'
INCLUDES = -Iinclude -I$(NPUNLOCK_ROOT)/include -I$(NPUNLOCK_ROOT)/src/workers
LDLIBS   = -L$(NPUNLOCK_BUILD) -lnpunlock -lpthread -ldl -lm -Wl,-rpath,$(NPUNLOCK_BUILD)

SOURCES = src/npurt_util.c src/npurt_device.c src/npurt_toolchain.c src/npurt_api.c
OBJECTS = $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES))

LIB_STATIC = $(BUILD)/libnpu_runtime.a
LIB_SHARED = $(BUILD)/libnpu_runtime.so
TOOLS = $(BUILD)/npu-runtime-bench $(BUILD)/npu-runtime-example $(BUILD)/npu-runtime-test \
        $(BUILD)/npu-runtime-dump $(BUILD)/npu-runtime-ceiling $(BUILD)/npu-runtime-session \
        $(BUILD)/npu-runtime-batch $(BUILD)/npu-runtime-two-input

.PHONY: all clean tools check

all: $(LIB_STATIC) $(LIB_SHARED) tools

tools: $(TOOLS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_STATIC): $(OBJECTS)
	ar rcs $@ $(OBJECTS)

$(LIB_SHARED): $(OBJECTS)
	$(CC) -shared -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD)/npu-runtime-bench: bench/bench_runtime.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-example: examples/backend_example.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-dump: tools/dump_output.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-ceiling: tools/device_ceiling.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-two-input: tests/two_input_test.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-batch: tools/batch_probe.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-session: tools/session_probe.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< $(LIB_STATIC) $(LDLIBS) -o $@

$(BUILD)/npu-runtime-test: tests/runtime_test.c $(LIB_STATIC)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_STATIC) $(LDLIBS) -o $@

# Runs the end-to-end checks (semantic add-one oracle + shape/state errors).
check: $(TOOLS)
	./tools/run_checks.sh

clean:
	rm -rf $(BUILD)
