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
	src/language_service/documentation.cpp \
	src/language_service/edits.cpp \
	src/language_service/formatter.cpp \
	src/language_service/language_service.cpp \
	src/language_service/queries.cpp \
	src/language_service/queries_structure.cpp \
	src/language_service/refactor.cpp \
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
	src/semantic/workspace_index.cpp \
	src/semantic/type_checker.cpp \
	src/semantic/units.cpp \
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
WORKSPACE_TEST := bin/workspace-test
SEMANTIC_INDEX_TEST := bin/semantic-index-test
LANGUAGE_QUERIES_TEST := bin/language-queries-test
LANGUAGE_EDITS_TEST := bin/language-edits-test
CONSTANTS_TEST := bin/constants-test
WINDOWS_LAUNCHER := bin/sagan-launch.exe
WINDOWS_LAUNCHER_RESOURCE := obj/launcher/sagan-resource.o
BUILD_TMP := build/tmp
HOST_UNAME := $(shell uname -s 2>/dev/null)
WINDOWS_HOST := $(if $(filter Windows_NT,$(OS)),1,$(if $(findstring MINGW,$(HOST_UNAME)),1,$(if $(findstring MSYS,$(HOST_UNAME)),1,)))
ifneq ($(WINDOWS_HOST),)
BUILD_TMP_NATIVE := $(shell cygpath -w "$(CURDIR)/$(BUILD_TMP)")
WINDOWS_RUNTIME_LDFLAGS := -static -static-libgcc -static-libstdc++
else
BUILD_TMP_NATIVE := $(CURDIR)/$(BUILD_TMP)
WINDOWS_RUNTIME_LDFLAGS :=
endif
TEMP_ENV := TMPDIR="$(BUILD_TMP_NATIVE)" TMP="$(BUILD_TMP_NATIVE)" TEMP="$(BUILD_TMP_NATIVE)"

.PHONY: all libraries windows-launcher clean test integration-test check-windows-runtime coverage tokenizer-inspect package-demo run-demo geometry-demo units-demo editor-tooling-demo formatter-demo ast-demo get-version FORCE

all: $(TARGET)

libraries: $(COMPILER_LIBRARY)

ifneq ($(WINDOWS_HOST),)
windows-launcher: $(WINDOWS_LAUNCHER)

$(WINDOWS_LAUNCHER_RESOURCE): packaging/windows/sagan.rc packaging/windows/sagan.ico
	@mkdir -p $(dir $@)
	windres -I packaging/windows $< -O coff -o $@

$(WINDOWS_LAUNCHER): src/launcher/windows_launcher.cpp $(WINDOWS_LAUNCHER_RESOURCE)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(WINDOWS_LAUNCHER_RESOURCE) -o $@ \
		$(WINDOWS_RUNTIME_LDFLAGS) $(LDFLAGS) -mwindows -municode -lshell32
else
windows-launcher:
	@echo "The Explorer launcher is built only on Windows."
endif

$(COMPILER_LIBRARY): $(LIBRARY_OBJECTS)
	@mkdir -p $(dir $@)
	ar rcs $@ $(LIBRARY_OBJECTS)

$(TARGET): $(CLI_OBJECTS) $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CLI_OBJECTS) $(COMPILER_LIBRARY) -o $@ $(WINDOWS_RUNTIME_LDFLAGS) $(LDFLAGS)

$(SOURCE_DIAGNOSTICS_TEST): tests/source_diagnostics_test.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(WORKSPACE_TEST): tests/workspace_test.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(SEMANTIC_INDEX_TEST): tests/semantic_index_test.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(LANGUAGE_QUERIES_TEST): tests/language_queries_test.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(LANGUAGE_EDITS_TEST): tests/language_edits_test.cpp $(COMPILER_LIBRARY)
	@mkdir -p $(dir $@) $(BUILD_TMP)
	$(TEMP_ENV) $(CXX) $(CXXFLAGS) $< $(COMPILER_LIBRARY) -o $@ $(LDFLAGS)

$(CONSTANTS_TEST): tests/constants_test.cpp $(COMPILER_LIBRARY)
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

test: $(TARGET) $(SOURCE_DIAGNOSTICS_TEST) $(WORKSPACE_TEST) $(SEMANTIC_INDEX_TEST) $(LANGUAGE_QUERIES_TEST) $(LANGUAGE_EDITS_TEST) $(CONSTANTS_TEST)
	bash scripts/windows/check_runtime_imports.sh $(TARGET)
	bash scripts/windows/installer_policy_test.sh
	bash scripts/release_policy_test.sh
	bash scripts/release_mirror_test.sh
	$(SOURCE_DIAGNOSTICS_TEST)
	$(WORKSPACE_TEST)
	$(SEMANTIC_INDEX_TEST)
	$(LANGUAGE_QUERIES_TEST)
	$(LANGUAGE_EDITS_TEST)
	$(CONSTANTS_TEST)
	$(TARGET) --self-test
	bash scripts/cli_test.sh
	bash tests/integration/run.sh

integration-test: $(TARGET)
	bash tests/integration/run.sh

check-windows-runtime: $(TARGET)
	bash scripts/windows/check_runtime_imports.sh $(TARGET)

coverage:
	bash scripts/coverage.sh

tokenizer-inspect: $(TARGET)
	$(TARGET) --tokens tests/fixtures/syntax/tokenizer.sagan

package-demo: $(TARGET)
	bash scripts/package_demo.sh

run-demo: $(TARGET)
	bash scripts/run_demo.sh

geometry-demo: $(TARGET)
	bash scripts/geometry_demo.sh

units-demo: $(TARGET)
	bash scripts/units_demo.sh

editor-tooling-demo: $(TARGET)
	bash scripts/editor_tooling_demo.sh

formatter-demo: $(LANGUAGE_EDITS_TEST)
	$(LANGUAGE_EDITS_TEST)

ast-demo: $(TARGET)
	@mkdir -p build
	$(TARGET) --ast-html examples/ast.sagan build/ast-demo.html
	@echo "Visual AST demo: build/ast-demo.html"

get-version:
	@echo $(SAGAN_VERSION)

FORCE:

clean:
	rm -rf obj bin build/lib

-include $(DEPENDENCIES)
