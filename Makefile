#
#  Copyright 2017 Roman Katuntsev <sbkarr@stappler.org>
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

#
# input variables
#

## output dirs and objects
OUTPUT_DIR := build
OUTPUT_TEST := build

BINARYEN_BIN ?= binaryen/bin
WABT_BIN ?= wabt/bin

WASM_AS ?= $(BINARYEN_BIN)/wasm-as
WAT2WASM ?= $(WABT_BIN)/wat2wasm
WASM2WAT ?= $(WABT_BIN)/wasm2wat

ifndef RELEASE
OUTPUT_DIR := $(OUTPUT_DIR)/debug
GLOBAL_CFLAGS ?= -O0 -g
GLOBAL_CXXFLAGS ?= -O0 -g
else
OUTPUT_DIR := $(OUTPUT_DIR)/release
GLOBAL_CFLAGS ?= -Os
GLOBAL_CXXFLAGS ?= -Os
endif

OUTPUT_LIB = $(OUTPUT_DIR)/libwasm-interp.a
OUTPUT_EXEC = $(OUTPUT_DIR)/wasm-interp

## project root
GLOBAL_ROOT := .

## executables
GLOBAL_MKDIR ?= mkdir -p
GLOBAL_RM ?= rm -f

GLOBAL_CC ?= gcc
GLOBAL_CPP ?= g++
GLOBAL_AR ?= ar rcs

#
# build variables
#

# lib sources
# recursive source files
LIB_SRCS_DIRS += \
	wasm

# individual source files
LIB_SRCS_OBJS += \

# recursive includes
LIB_INCLUDES_DIRS += \
	wasm

# individual includes
LIB_INCLUDES_OBJS += \
	.


# exec sources
# recursive source files
EXEC_SRCS_DIRS += \
	exec

# individual source files
EXEC_SRCS_OBJS += \

#
# make lists
#

# search for sources
LIB_SRCS := \
	$(foreach dir,$(LIB_SRCS_DIRS),$(shell find $(GLOBAL_ROOT)/$(dir) -name '*.cpp')) \
	$(addprefix $(GLOBAL_ROOT)/,$(LIB_SRCS_OBJS))

# search for sources
EXEC_SRCS := \
	$(foreach dir,$(EXEC_SRCS_DIRS),$(shell find $(GLOBAL_ROOT)/$(dir) -name '*.cpp')) \
	$(addprefix $(GLOBAL_ROOT)/,$(EXEC_SRCS_OBJS))

# search for includes
LIB_INCLUDES := \
	$(foreach dir,$(LIB_INCLUDES_DIRS),$(shell find $(GLOBAL_ROOT)/$(dir) -type d)) \
	$(addprefix $(GLOBAL_ROOT)/,$(LIB_INCLUDES_OBJS))

# build object list to compile
LIB_OBJS := $(patsubst %.cpp,%.o,$(patsubst $(GLOBAL_ROOT)/%,$(OUTPUT_DIR)/%,$(LIB_SRCS)))

# build object list to compile
EXEC_OBJS := $(patsubst %.cpp,%.o,$(patsubst $(GLOBAL_ROOT)/%,$(OUTPUT_DIR)/%,$(EXEC_SRCS)))

# build directory list to create
LIB_DIRS := $(sort $(dir $(LIB_OBJS)))

# build directory list to create
EXEC_DIRS := $(sort $(dir $(EXEC_OBJS)))

# build compiler include flag list
LIB_INPUT_CFLAGS := $(addprefix -I,$(LIB_INCLUDES))

BUILD_CXXFLAGS := $(GLOBAL_CXXFLAGS) $(LIB_INPUT_CFLAGS) $(CFLAGS) $(CPPFLAGS)
BUILD_CFLAGS := $(GLOBAL_CFLAGS) $(LIB_INPUT_CFLAGS) $(CFLAGS)

TEST_WAST := $(shell find $(GLOBAL_ROOT)/test -name '*.wast')
TEST_WASM := $(patsubst $(GLOBAL_ROOT)/%.wast,$(OUTPUT_TEST)/%.wasm,$(TEST_WAST))

TEST_WABT_WAST := $(shell find $(GLOBAL_ROOT)/test -name '*.wat')
TEST_WABT_WASM := $(patsubst $(GLOBAL_ROOT)/%.wat,$(OUTPUT_TEST)/%.wasm,$(TEST_WABT_WAST))

TEST_ASSERT := $(patsubst $(GLOBAL_ROOT)/%,$(OUTPUT_TEST)/%,$(shell find $(GLOBAL_ROOT)/test -name '*.assert'))

TEST_WAT := $(patsubst $(OUTPUT_TEST)/%.wasm,$(OUTPUT_TEST)/%.wat,$(TEST_WABT_WASM) $(TEST_WASM))

.DEFAULT_GOAL := all

# include sources dependencies
-include $(patsubst %.o,%.d,$(LIB_OBJS))
-include $(patsubst %.o,%.d,$(EXEC_OBJS))

# build cpp sources
$(OUTPUT_DIR)/%.o: $(GLOBAL_ROOT)/%.cpp
	$(GLOBAL_CPP) -MMD -MP -MF $(OUTPUT_DIR)/$*.d $(BUILD_CXXFLAGS) $< -c -o $@

# build cpp sources
$(OUTPUT_DIR)/%.o: $(GLOBAL_ROOT)/%.cc
	$(GLOBAL_CPP) -MMD -MP -MF $(OUTPUT_DIR)/$*.d $(BUILD_CXXFLAGS) $< -c -o $@

# build c sources
$(OUTPUT_DIR)/%.o: $(GLOBAL_ROOT)/%.c
	$(GLOBAL_CC) -MMD -MP -MF $(OUTPUT_DIR)/$*.d $(BUILD_CFLAGS) $< -c -o $@

$(OUTPUT_LIB): $(LIB_OBJS)
	$(GLOBAL_AR) $(OUTPUT_LIB) $(LIB_OBJS)

$(OUTPUT_EXEC): $(LIB_OBJS) $(EXEC_OBJS)
	$(GLOBAL_CPP) $(LIB_OBJS) $(EXEC_OBJS) -o $(OUTPUT_EXEC)

$(OUTPUT_TEST)/%.wasm: %.wast
	$(WASM_AS) -o $@ $<

$(OUTPUT_TEST)/%.wasm: %.wat
	$(WAT2WASM) -o $@ $<

$(OUTPUT_TEST)/%.wat: $(OUTPUT_TEST)/%.wasm
	$(WASM2WAT) -o $@ $<

$(OUTPUT_TEST)/%.assert: %.assert
	cp -f $< $@ 

test: $(TEST_WASM) $(TEST_WABT_WASM) $(TEST_WAT) $(TEST_ASSERT)
test-exec: .prebuild $(OUTPUT_EXEC) test
lib: .prebuild $(OUTPUT_LIB)

all: .prebuild $(OUTPUT_EXEC) test lib

.prebuild:
	@$(GLOBAL_MKDIR) $(LIB_DIRS) $(EXEC_DIRS) $(OUTPUT_TEST)/test

clean:
	$(GLOBAL_RM) -r $(OUTPUT_DIR) $(OUTPUT_TEST)/test

.PHONY: all prebuild clean test lib test-exec
