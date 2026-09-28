APPNAME := sagan
CXX ?= g++
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wpedantic -Werror -DUNI_ALGO_STATIC_DATA -Ithird_party/uni-algo/include
LDFLAGS ?=

# Development versions follow Zach Westerman's commit-derived Schematic model.
# Changing a major or minor version also establishes a new base commit, which
# resets the automatically calculated patch number to zero.
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_BASE_COMMIT := cb27232c
VERSION_PATCH := $(shell git rev-list --count $(VERSION_BASE_COMMIT)..HEAD 2>/dev/null || echo 0)
VERSION_REVISION := $(shell git rev-parse --short=8 HEAD 2>/dev/null || echo unknown)
VERSION_DIRTY := $(if $(shell git status --porcelain --untracked-files=normal 2>/dev/null),.dirty,)
SAGAN_VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)+g$(VERSION_REVISION)$(VERSION_DIRTY)

SOURCES := \
	src/main.cpp \
	src/parser/ast_render.cpp \
	src/parser/ast_node.cpp \
	src/parser/lex.cpp \
	src/parser/parse_error.cpp \
	src/parser/parser.cpp \
	src/parser/tokenizer.cpp \
	src/parser/tokens.cpp \
	src/parser/unicode.cpp

OBJECTS := $(patsubst src/%.cpp,obj/%.o,$(SOURCES))
OBJECTS += obj/version.o
TARGET := bin/$(APPNAME)
BUILD_TMP := build/tmp
BUILD_TMP_NATIVE := $(shell cygpath -w "$(CURDIR)/$(BUILD_TMP)")
TEMP_ENV := TMPDIR="$(BUILD_TMP_NATIVE)" TMP="$(BUILD_TMP_NATIVE)" TEMP="$(BUILD_TMP_NATIVE)"

.PHONY: all clean test demo parser-demo ast-demo get-version FORCE

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

demo: $(TARGET)
	$(TARGET) examples/tokenizer_demo.sagan

parser-demo: $(TARGET)
	$(TARGET) --ast examples/parser_demo.sagan

ast-demo: $(TARGET)
	@mkdir -p build
	$(TARGET) --ast-html examples/parser_demo.sagan build/ast-demo.html
	@echo "Visual AST demo: build/ast-demo.html"

get-version:
	@echo $(SAGAN_VERSION)

FORCE:

clean:
	rm -rf obj bin
