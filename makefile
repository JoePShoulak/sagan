APPNAME := sagan
CXX ?= g++
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wpedantic -Werror -DUNI_ALGO_STATIC_DATA -Ithird_party/uni-algo/include
LDFLAGS ?=

# Development versions retain Schematic's Git-derived identity while using
# Conventional Commit markers to declare semantic-version impact.
SAGAN_VERSION := $(shell bash scripts/version.sh current 2>/dev/null || echo 0.0.0+gunknown)

SOURCES := \
	src/main.cpp \
	src/codegen/cpp_generator.cpp \
	src/parser/ast_render.cpp \
	src/parser/ast_node.cpp \
	src/parser/lex.cpp \
	src/parser/parse_error.cpp \
	src/parser/parser.cpp \
	src/parser/tokenizer.cpp \
	src/parser/tokens.cpp \
	src/parser/unicode.cpp \
	src/semantic/analyzer.cpp \
	src/semantic/type_checker.cpp

OBJECTS := $(patsubst src/%.cpp,obj/%.o,$(SOURCES))
OBJECTS += obj/version.o
TARGET := bin/$(APPNAME)
BUILD_TMP := build/tmp
ifeq ($(OS),Windows_NT)
BUILD_TMP_NATIVE := $(shell cygpath -w "$(CURDIR)/$(BUILD_TMP)")
else
BUILD_TMP_NATIVE := $(CURDIR)/$(BUILD_TMP)
endif
TEMP_ENV := TMPDIR="$(BUILD_TMP_NATIVE)" TMP="$(BUILD_TMP_NATIVE)" TEMP="$(BUILD_TMP_NATIVE)"

.PHONY: all clean test coverage demo parser-demo semantic-demo type-demo entry-demo execution-demo ast-demo get-version FORCE

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) -c $< -o $@

obj/version.cpp: FORCE
	@mkdir -p $(dir $@)
	@printf 'extern const char *const SAGAN_VERSION = "%s";\n' '$(SAGAN_VERSION)' > $@

obj/version.o: obj/version.cpp src/version.hpp
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	$(TARGET) --self-test
	bash scripts/cli_test.sh

coverage:
	bash scripts/coverage.sh

demo: $(TARGET)
	$(TARGET) examples/tokenizer_demo.sagan

parser-demo: $(TARGET)
	$(TARGET) --ast examples/parser_demo.sagan

semantic-demo: $(TARGET)
	$(TARGET) --semantic examples/semantic_demo.sagan

type-demo: $(TARGET)
	$(TARGET) --types examples/type_demo.sagan

entry-demo: $(TARGET)
	$(TARGET) --entry examples/entry_demo.sagan

execution-demo: $(TARGET)
	bash scripts/execution_demo.sh

ast-demo: $(TARGET)
	@mkdir -p build
	$(TARGET) --ast-html examples/parser_demo.sagan build/ast-demo.html
	@echo "Visual AST demo: build/ast-demo.html"

get-version:
	@echo $(SAGAN_VERSION)

FORCE:

clean:
	rm -rf obj bin
