.PHONY: all build dev test run compiledb compiledb-commands format clean

CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Iinclude
SOURCES := src/shitnet.cpp tests/ethernet.cpp

all: dev

build:
	zig build -Doptimize=ReleaseFast

dev:
	zig build -Doptimize=Debug

test:
	zig build test

run:
	zig build run

compiledb:
	bear --output compile_commands.json -- make compiledb-commands

compiledb-commands:
	$(foreach source,$(SOURCES),clang++ $(CXXFLAGS) -fsyntax-only $(source);)

format:
	find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i

clean:
	rm -rf zig-out .zig-cache compile_commands.json
