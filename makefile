APPNAME := sagan
CXX ?= g++
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wpedantic -Werror -MMD -MP -DUNI_ALGO_STATIC_DATA -Ithird_party/uni-algo/include
LDFLAGS ?=

# Development versions retain Schematic's Git-derived identity while using
# Conventional Commit markers to declare semantic-version impact.
SAGAN_VERSION := $(shell bash scripts/version.sh current 2>/dev/null || echo 0.0.0+gunknown)

LIBRARY_SOURCES := \
	src/codegen/cpp_generator.cpp \
	src/diagnostics/diagnostic.cpp \
	src/driver/native_runner.cpp \
	src/language_service/language_service.cpp \
	src/language_service/workspace.cpp \
	src/modules/resolver.cpp \
	src/parser/ast_render.cpp \
	src/parser/ast_node.cpp \
	src/parser/lex.cpp \
	src/parser/parse_error.cpp \
	src/parser/parser.cpp \
	src/parser/tokenizer.cpp \
	src/parser/tokens.cpp \
	src/parser/unicode.cpp \
	src/semantic/analyzer.cpp \
	src/semantic/index.cpp \
	src/semantic/type_checker.cpp \
	src/source/provider.cpp \
	src/source/source.cpp \
	src/syntax/syntax.cpp

LIBRARY_OBJECTS := $(patsubst src/%.cpp,obj/%.o,$(LIBRARY_SOURCES))
CLI_OBJECTS := obj/main.o obj/version.o
OBJECTS := $(LIBRARY_OBJECTS) $(CLI_OBJECTS)
DEPENDENCIES := $(OBJECTS:.o=.d)
TARGET := bin/$(APPNAME)
COMPILER_LIBRARY := build/lib/libsagan-compiler.a
SOURCE_DIAGNOSTICS_TEST := bin/source-diagnostics-test
WORKSPACE_DEMO := bin/workspace-demo
SEMANTIC_INDEX_DEMO := bin/semantic-index-demo
WINDOWS_LAUNCHER := bin/sagan-launch.exe
WINDOWS_LAUNCHER_RESOURCE := obj/launcher/sagan-resource.o
BUILD_TMP := build/tmp
ifeq ($(OS),Windows_NT)
BUILD_TMP_NATIVE := $(shell cygpath -w "$(CURDIR)/$(BUILD_TMP)")
else
BUILD_TMP_NATIVE := $(CURDIR)/$(BUILD_TMP)
endif
TEMP_ENV := TMPDIR="$(BUILD_TMP_NATIVE)" TMP="$(BUILD_TMP_NATIVE)" TEMP="$(BUILD_TMP_NATIVE)"

.PHONY: all libraries windows-launcher clean test coverage demo parser-demo semantic-demo type-demo entry-demo module-demo package-demo run-demo geometry-demo editor-tooling-demo workspace-demo semantic-index-demo execution-demo runtime-error-demo optional-demo weak-demo ownership-demo payload-enum-demo generic-sum-demo generic-class-demo ast-demo get-version FORCE

all: $(TARGET)

libraries: $(COMPILER_LIBRARY)

ifeq ($(OS),Windows_NT)
windows-launcher: $(WINDOWS_LAUNCHER)

$(WINDOWS_LAUNCHER_RESOURCE): packaging/windows/sagan.rc packaging/windows/sagan.ico
	@mkdir -p $(dir $@)
	windres -I packaging/windows $< -O coff -o $@

$(WINDOWS_LAUNCHER): src/launcher/windows_launcher.cpp $(WINDOWS_LAUNCHER_RESOURCE)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(WINDOWS_LAUNCHER_RESOURCE) -o $@ -mwindows -municode -lshell32
else
windows-launcher:
	@echo "The Explorer launcher is built only on Windows."
endif

$(COMPILER_LIBRARY): $(LIBRARY_OBJECTS)
	@mkdir -p $(dir $@)
	ar rcs $@ $(LIBRARY_OBJECTS)

$(TARGET): $(CLI_OBJECTS) $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CLI_OBJECTS) $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(SOURCE_DIAGNOSTICS_TEST): tests/source_diagnostics_test.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(WORKSPACE_DEMO): tests/workspace_demo.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(SEMANTIC_INDEX_DEMO): tests/semantic_index_demo.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) -c $< -o $@

obj/version.cpp: FORCE
	@mkdir -p $(dir $@)
	@printf 'extern const char *const SAGAN_VERSION = "%s";\n' '$(SAGAN_VERSION)' > $@

obj/version.o: obj/version.cpp src/version.hpp
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET) $(SOURCE_DIAGNOSTICS_TEST) $(WORKSPACE_DEMO) $(SEMANTIC_INDEX_DEMO)
	$(SOURCE_DIAGNOSTICS_TEST)
	$(WORKSPACE_DEMO)
	$(SEMANTIC_INDEX_DEMO)
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

module-demo: $(TARGET)
	bash scripts/module_demo.sh

package-demo: $(TARGET)
	bash scripts/package_demo.sh

run-demo: $(TARGET)
	bash scripts/run_demo.sh

geometry-demo: $(TARGET)
	bash scripts/geometry_demo.sh

editor-tooling-demo: $(TARGET)
	bash scripts/editor_tooling_demo.sh

workspace-demo: $(WORKSPACE_DEMO)
	$(WORKSPACE_DEMO)

semantic-index-demo: $(SEMANTIC_INDEX_DEMO)
	$(SEMANTIC_INDEX_DEMO)

execution-demo: $(TARGET)
	bash scripts/execution_demo.sh

runtime-error-demo: $(TARGET)
	bash scripts/runtime_error_demo.sh

optional-demo: $(TARGET)
	bash scripts/optional_demo.sh

weak-demo: $(TARGET)
	bash scripts/weak_demo.sh

ownership-demo: $(TARGET)
	bash scripts/ownership_demo.sh

payload-enum-demo: $(TARGET)
	bash scripts/payload_enum_demo.sh

generic-sum-demo: $(TARGET)
	bash scripts/generic_sum_demo.sh

generic-class-demo: $(TARGET)
	bash scripts/generic_class_demo.sh

ast-demo: $(TARGET)
	@mkdir -p build
	$(TARGET) --ast-html examples/parser_demo.sagan build/ast-demo.html
	@echo "Visual AST demo: build/ast-demo.html"

get-version:
	@echo $(SAGAN_VERSION)

FORCE:

clean:
	rm -rf obj bin build/lib

-include $(DEPENDENCIES)
