BUILD_DIR ?= build
BINARY := $(BUILD_DIR)/code

.PHONY: all build run clean

all: build

build:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target code

# Run compiler: read source from stdin, IR to stdout, builtin.c to stderr
run:
	$(BINARY)

clean:
	rm -rf $(BUILD_DIR)
