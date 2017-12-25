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

#ifndef EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMSCRIPTRUNTIME_H_
#define EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMSCRIPTRUNTIME_H_

#include "SPWasmRuntime.h"

NS_SP_EXT_BEGIN(wasm)

class ScriptEnvironment : public Environment {
public:
	virtual ~ScriptEnvironment() { }
	virtual bool init();

protected:
	void initHostMemFunc();
};

class ScriptRuntime : public Runtime {
public:
	struct Allocator;

	struct MemoryContext {
		memory::pool_t *pool;
		uint32_t size;
		uint8_t *originalData;
		Allocator *allocator = nullptr;
	};

	virtual ~ScriptRuntime();

	std::mutex &getMutex() const;
	MemoryContext *getMemoryContext(const RuntimeMemory &) const;

protected:
	virtual bool onImportMemory(RuntimeMemory &target, const Module::Import &import) override;
	//virtual bool onImportTable(RuntimeTable &target, const Module::Import &import);

	virtual bool onInitMemory(const StringView &module, const StringView &env, RuntimeMemory &target) override;

	virtual bool onMemoryAction(const RuntimeMemory &, uint32_t, RuntimeMemory::Action) override;

	mutable std::mutex _mutex;
};

class ScriptThread : public Thread {
public:
	static constexpr uint32_t DefaultUserStackSize = 8_KiB;

	struct Config {
		uint32_t valueStack = DefaultValueStackSize;
		uint32_t callStack = DefaultCallStackSize;
		uint32_t userStack = DefaultUserStackSize;

		uint32_t tag = 0;

		Config() = default;
	};

	virtual ~ScriptThread();
	virtual bool init(const Arc<ScriptRuntime> &, const Config &);

	uint32_t pushString(const RuntimeMemory &, const StringView &);
	uint32_t pushMemory(const RuntimeMemory &, uint8_t *, size_t s);

protected:
};

NS_SP_EXT_END(wasm)

#endif /* EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMSCRIPTRUNTIME_H_ */
