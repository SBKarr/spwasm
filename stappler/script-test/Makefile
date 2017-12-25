# Copyright (c) 2018 Roman Katuntsev <sbkarr@stappler.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# verbose := yes

STAPPLER_ROOT = ../../../..

include ../../stappler.mk

LOCAL_OUTDIR := bin
LOCAL_OUTPUT_EXECUTABLE := $(LOCAL_OUTDIR)/script-test

LOCAL_TOOLKIT := common

LOCAL_ROOT = .

LOCAL_SRCS_DIRS :=  $(SPWASM_SOURCE_DIR) src
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS := $(SPWASM_INCLUDE)

LOCAL_MAIN := main.cpp

LOCAL_LIBS =


#
# input variables
#


LOCAL_OUTPUT_WASM = $(LOCAL_OUTDIR)/test.wasm
LOCAL_OUTPUT_WAST = $(LOCAL_OUTDIR)/test.wast

## default cflags
WASM_CFLAGS ?= -Os

## executables
WASM_MKDIR ?= mkdir -p

LLVM ?= ../../../llvm-build/bin
WABT ?= ../../wabt/bin


#
# build variables
#

WASM_SRCS_DIRS := wasm
WASM_SRCS_OBJS :=

WASM_INCLUDES := wasm ../script

# search for sources
WASM_SRCS := \
	$(foreach dir,$(WASM_SRCS_DIRS),$(shell find $(LOCAL_ROOT)/$(dir) -name '*.c*')) \
	$(addprefix $(LOCAL_ROOT)/,$(WASM_SRCS_OBJS))

# search for includes
WASM_INCLUDES := $(addprefix $(LOCAL_ROOT)/,$(WASM_INCLUDES))
WASM_OBJS := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst $(LOCAL_ROOT)/%,$(LOCAL_OUTDIR)/%,$(WASM_SRCS))))

WASM_CC := $(LLVM)/clang
WASM_CPP := $(LLVM)/clang++
WASM_LLD := $(LLVM)/wasm-ld

# build directory list to create
WASM_DIRS := $(sort $(dir $(WASM_OBJS)))

WASMC_CXXFLAGS := $(WASM_CFLAGS) $(addprefix -I,$(WASM_INCLUDES)) -std=c++14 -fno-rtti -fno-exceptions
WASMC_CFLAGS := $(WASM_CFLAGS) $(addprefix -I,$(WASM_INCLUDES))

# include sources dependencies
-include $(patsubst %.o,%.d,$(WASM_OBJS))

# build cpp sources
$(LOCAL_OUTDIR)/wasm/%.o: $(LOCAL_ROOT)/wasm/%.cpp
	$(WASM_CPP) -MMD -MP -MF $(LOCAL_OUTDIR)/wasm/$*.d --target=wasm32-unknown-unknown-wasm $(WASMC_CXXFLAGS) -c $< -o $@

# build c sources
$(LOCAL_OUTDIR)/wasm/%.o: $(LOCAL_ROOT)/wasm/%.c
	$(WASM_CC) -MMD -MP -MF $(LOCAL_OUTDIR)/wasm/$*.d --target=wasm32-unknown-unknown-wasm $(WASMC_CFLAGS) -c $< -o $@

# build c sources
$(LOCAL_OUTPUT_WASM): $(WASM_OBJS)
	$(WASM_LLD) --allow-undefined --no-entry --import-memory --global-base=0 -z stack-size=1024 -o $@ $(WASM_OBJS)

$(LOCAL_OUTPUT_WAST): $(LOCAL_OUTPUT_WASM)
	$(WABT)/wasm2wat $< -o $@

local: wasm

wasm: .wasm_prebuild $(LOCAL_OUTPUT_WAST)

.wasm_prebuild:
	@$(WASM_MKDIR) $(WASM_DIRS)

.PHONY: .wasm_prebuild wasm

include $(STAPPLER_ROOT)/make/local.mk
