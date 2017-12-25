/*
 * Copyright 2017 Roman Katuntsev <sbkarr@stappler.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SPWasmScriptRuntime.h"
#include "SPWasmScriptAllocator.cc"
#include "SPWasmScriptMemory.cc"

NS_SP_EXT_BEGIN(wasm)

static bool tryDeduceStackOffsetWithMagic(const ::wasm::RuntimeModule &module, uint32_t &value) {
	auto expIt = module.exports.find("__wasm_get_stack_pointer");
	if (expIt != module.exports.end() && expIt->second.second == ::wasm::ExternalKind::Func) {
		if (auto func = module.func[expIt->second.first].first) {
			for (auto &opcode : func->opcodes) {
				switch (opcode.opcode) {
				case ::wasm::Opcode::GetGlobal:
					if (const auto g = module.globals[opcode.value32.v1]) {
						if (g->mut && g->value.type == ::wasm::Type::I32) {
							value = g->value.value.i32;
							return true;
						}
					}
					break;
				default:
					break;
				}
			}
		}
	}
	return false;
}

static bool tryDeduceStackOffsetFromGlobal(const ::wasm::RuntimeModule &module, uint32_t &value) {
	for (auto &it : module.module->getGlobalIndexVec()) {
		if (!it.import && !it.exported) {
			if (auto g = module.globals[it.index]) {
				if (g->mut && g->value.type == ::wasm::Type::I32) {
					value = g->value.value.i32;
					return true;
				}
			}
		}
	}
	return false;
}

static uint32_t deduceStackOffset(const RuntimeMemory &mem, const Map<String, RuntimeModule> &mod) {
	uint32_t ret = 0;
	auto memPtr = &mem;
	for (auto &it : mod) {
		if (it.second.module) {
			auto size = it.second.module->getLinkingOffset();
			if (size != ::wasm::kInvalidOffset) {
				if (ret < size) {
					ret = size;
				}
			} else {
				for (auto &memIt : it.second.memory) {
					if (memPtr == memIt) {
						uint32_t value = 0;
						if (tryDeduceStackOffsetWithMagic(it.second, value)) {
							if (ret < value) {
								ret = value;
							}
						} else if (tryDeduceStackOffsetFromGlobal(it.second, value)) {
							if (ret < value) {
								ret = value;
							}
						}
					}
				}
			}
		}
	}
	return ret;
}

static ScriptRuntime::MemoryContext * makeContext(memory::pool_t *root, const RuntimeMemory &mem, uint32_t reqSize, const Map<String, RuntimeModule> &modules) {
	auto p = memory::pool::create(root);
	auto ret = (ScriptRuntime::MemoryContext *)memory::pool::palloc(p, sizeof(ScriptRuntime::MemoryContext));
	ret->pool = p;
	ret->size = reqSize;
	ret->originalData = (uint8_t *)memory::pool::palloc(p, reqSize);
	memset(ret->originalData, 0, reqSize);

	uint32_t stackOffset = ALIGN_DEFAULT(deduceStackOffset(mem, modules));
	const auto offset = ret->originalData + stackOffset;
	ret->allocator = new (offset) Allocator(ALIGN_DEFAULT(stackOffset));
	return ret;
}

static ScriptRuntime::MemoryContext * reallocContext(memory::pool_t *root, const RuntimeMemory &mem, uint32_t reqSize) {
	auto origCtx = (ScriptRuntime::MemoryContext *)mem.ctx;

	auto p = memory::pool::create(root);
	auto ret = (ScriptRuntime::MemoryContext *)memory::pool::palloc(p, sizeof(ScriptRuntime::MemoryContext));
	ret->pool = p;
	ret->size = reqSize;
	ret->originalData = (uint8_t *)memory::pool::palloc(p, reqSize);
	auto offset = (uint8_t *)origCtx->allocator - origCtx->originalData;

	memcpy(ret->originalData, origCtx->originalData, origCtx->size);
	memset(ret->originalData + origCtx->size, 0, ret->size - origCtx->size);
	memory::pool::destroy((memory::pool_t *)origCtx->pool);

	ret->allocator = (Allocator *)(ret->originalData + offset);

	return ret;
}

static void freeContext(const RuntimeMemory &mem) {
	auto origCtx = (ScriptRuntime::MemoryContext *)mem.ctx;
	memory::pool::destroy(origCtx->pool);
}


bool ScriptEnvironment::init() {
	if (!Environment::init()) {
		return false;
	}
	memory::pool::push(_pool);
	initHostMemFunc();
	memory::pool::pop();
	return true;
}

ScriptRuntime::~ScriptRuntime() { }

std::mutex &ScriptRuntime::getMutex() const {
	return _mutex;
}
ScriptRuntime::MemoryContext *ScriptRuntime::getMemoryContext(const RuntimeMemory &mem) const {
	return (ScriptRuntime::MemoryContext *)mem.ctx;
}

bool ScriptRuntime::onImportMemory(RuntimeMemory &target, const Module::Import &import) {
	return true;
}

bool ScriptRuntime::onInitMemory(const StringView &module, const StringView &env, RuntimeMemory &target) {
	return true;
}

bool ScriptRuntime::onMemoryAction(const RuntimeMemory &mem, uint32_t size, RuntimeMemory::Action a) {
	switch (a) {
	case RuntimeMemory::Action::Alloc: {
		auto ctx = makeContext(_pool, mem, size, _runtime->getModules());
		mem.data = ctx->originalData;
		mem.size = ctx->size;
		mem.ctx = ctx;
		break;
	}
	case RuntimeMemory::Action::Realloc: {
		auto ctx = reallocContext(_pool, mem, size);
		mem.data = ctx->originalData;
		mem.size = ctx->size;
		mem.ctx = ctx;
		break;
	}
	case RuntimeMemory::Action::Free:
		freeContext(mem);
		mem.data = nullptr;
		mem.size = 0;
		mem.ctx = nullptr;
		break;
	}
	return true;
}

ScriptThread::~ScriptThread() { }

bool ScriptThread::init(const Arc<ScriptRuntime> &runtime, const Config & config) {
	if (!Thread::init(runtime, config.tag, config.valueStack, config.callStack)) {
		return false;
	}

	_thread->setThreadContext(this);

	auto &memVec = runtime->getRuntime()->getRuntimeMemory();
	for (const RuntimeMemory &memIt : memVec) {
		std::unique_lock<std::mutex> lock(runtime->getMutex());

		MemCtx memCtx(_thread, &memIt);

		auto p = Pool::create(memCtx, config.userStack + SIZEOF_MEMNODE_T + SIZEOF_POOL_T + 1024);
		auto stackp = p->palloc(memCtx, config.userStack);
		MemPtr<ThreadContext> ctxPtr = p->palloc(memCtx, sizeof(ThreadContext)).reinterpret<ThreadContext>();
		new (ctxPtr.get(memCtx)) ThreadContext(memCtx, ADDRESS(memCtx, p));

		_thread->setUserStackPointer(stackp.addr() + config.userStack, stackp.addr());
		_thread->setUserContext(ctxPtr.addr());
	}

	return true;
}

uint32_t ScriptThread::pushString(const RuntimeMemory &mem, const StringView &str) {
	MemCtx memCtx(_thread, &mem, static_cast<ScriptRuntime *>(_runtime.get())->getMutex());

	MemPtr<ThreadContext> ctxPtr(_thread->getUserStackPointer());
	auto threadCtx = ctxPtr.get(memCtx);
	auto pool = threadCtx->top(memCtx);

	auto memPtr = pool.get(memCtx)->palloc(memCtx, str.size() + 1);
	memcpy(memPtr.get(memCtx), str.data(), str.size());
	*(memPtr.get(memCtx) + str.size()) = 0;
	return memPtr.addr();
}

uint32_t ScriptThread::pushMemory(const RuntimeMemory &mem, uint8_t *data, size_t size) {
	MemCtx memCtx(_thread, &mem, static_cast<ScriptRuntime *>(_runtime.get())->getMutex());

	MemPtr<ThreadContext> ctxPtr(_thread->getUserStackPointer());
	auto pool = ctxPtr.get(memCtx)->top(memCtx);

	auto memPtr = pool.get(memCtx)->palloc(memCtx, size);
	memcpy(memPtr.get(memCtx), data, size);
	return memPtr.addr();
}

NS_SP_EXT_END(wasm)
