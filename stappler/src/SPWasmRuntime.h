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

#ifndef EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMRUNTIME_H_
#define EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMRUNTIME_H_

#include "SPRef.h"
#include "wasm/Environment.h"
#include "wasm/Thread.h"

NS_SP_EXT_BEGIN(wasm)

template <typename T>
using Vector = ::wasm::Vector<T>;

template <typename T>
using Function = ::wasm::Function<T>;

template <typename K, typename V>
using Map = ::wasm::Map<K, V>;

using String = ::wasm::String;
using StringStream = ::wasm::StringStream;

using Module = ::wasm::Module;
using HostModule = ::wasm::HostModule;
using Value = ::wasm::Value;
using TypedValue = ::wasm::TypedValue;
using ReadOptions = ::wasm::ReadOptions;
using RuntimeModule = ::wasm::RuntimeModule;
using RuntimeGlobal = ::wasm::RuntimeGlobal;
using RuntimeMemory = ::wasm::RuntimeMemory;
using RuntimeTable = ::wasm::RuntimeTable;
using Func = ::wasm::Func;
using LinkingPolicy = ::wasm::LinkingPolicy;
using HostFunc = ::wasm::HostFunc;

using RuntimeModule = ::wasm::RuntimeModule;
using ValueVec = ::wasm::Vector<Value>;

class Environment : public Ref {
public:
	using PrintCallback = Function<void(const StringView &)>;

	~Environment();

	virtual bool init();

	void setPrintCallback(const PrintCallback &);
	const PrintCallback &getPrintCallback() const;

	Module * loadModule(const StringView &, const uint8_t *, size_t, const ReadOptions & = ReadOptions());
	HostModule * makeHostModule(const StringView &);
	HostModule * getEnvModule() const;

	const Map<String, Module> &getExternalModules() const;
	const Map<String, HostModule> &getHostModules() const;

	const memory::MemPool &getPool() const;

protected:
	void initHostMathFunc();
	void initHostStringFunc();

	void onError(const StringView &, const StringStream &) const;

	friend class Runtime;

	memory::MemPool _pool = memory::MemPool::ManagedRoot;
	memory::MemPool _loaderPool = memory::MemPool::None;
	::wasm::Environment *_env = nullptr;
	PrintCallback _printCallback;
};

class Runtime : public Ref {
public:
	virtual ~Runtime();

	virtual bool init(const Arc<Environment> &);

	const RuntimeModule *getModule(const StringView &) const;
	const RuntimeModule *getModule(const Module *) const;

	const Func *getExportFunc(const StringView &module, const StringView &name) const;
	const Func *getExportFunc(const RuntimeModule &module, const StringView &name) const;

	const RuntimeGlobal *getGlobal(const StringView &module, const StringView &name) const;
	const RuntimeGlobal *getGlobal(const RuntimeModule &module, const StringView &name) const;

	bool setGlobal(const StringView &module, const StringView &name, const Value &);
	bool setGlobal(const RuntimeModule &module, const StringView &name, const Value &);

	const memory::MemPool &getPool() const;
	const ::wasm::Runtime *getRuntime() const;

protected:
	virtual bool onMemoryAction(const RuntimeMemory &, uint32_t, RuntimeMemory::Action);

	virtual bool onImportFunc(HostFunc &target, const Module::Import &import);
	virtual bool onImportGlobal(RuntimeGlobal &target, const Module::Import &import);
	virtual bool onImportMemory(RuntimeMemory &target, const Module::Import &import);
	virtual bool onImportTable(RuntimeTable &target, const Module::Import &import);

	virtual bool onInitMemory(const StringView &module, const StringView &env, RuntimeMemory &target);
	virtual bool onInitTable(const StringView &module, const StringView &env, RuntimeTable &target);

	friend class Thread;
	friend class ImportInternal;

	Arc<Environment> _env;
	memory::MemPool _pool = memory::MemPool::Managed;
	::wasm::Runtime *_runtime = nullptr;
};

class Thread : public Ref {
public:
	using Result = ::wasm::Thread::Result;

	static constexpr uint32_t DefaultValueStackSize = 1024;
	static constexpr uint32_t DefaultCallStackSize = 256;

	virtual ~Thread();

	virtual bool init(const Arc<Runtime> &, uint32_t tag = 0, uint32_t = DefaultValueStackSize, uint32_t = DefaultCallStackSize);

	bool run(const RuntimeModule &module, const Func &func, Value *buffer = nullptr);

	template <typename Callback>
	Thread::Result prepare(const StringView &m, const StringView &f, const Callback &cb);

	bool call(const RuntimeModule &module, const Func &, Vector<Value> &paramsInOut);
	bool call(const RuntimeModule &module, const Func &, Value *paramsInOut);

	bool call(const Func &, Vector<Value> &paramsInOut);
	bool call(const Func &, Value *paramsInOut);

	Thread::Result callSafe(const RuntimeModule &module, const Func &, Vector<Value> &paramsInOut);
	Thread::Result callSafe(const RuntimeModule &module, const Func &, Value *paramsInOut);

	Thread::Result callSafe(const Func &, Vector<Value> &paramsInOut);
	Thread::Result callSafe(const Func &, Value *paramsInOut);

	uint8_t *getMemory(uint32_t memIndex, uint32_t offset) const;
	uint8_t *getMemory(uint32_t memIndex, uint32_t offset, uint32_t size) const;

	const memory::MemPool &getPool() const;
	Runtime *getRuntime() const;
	::wasm::Thread *getThread() const;

protected:
	Arc<Runtime> _runtime;
	memory::MemPool _pool = memory::MemPool::Managed;
	::wasm::Thread *_thread = nullptr;
};

template <typename Callback>
inline Thread::Result Thread::prepare(const StringView &m, const StringView &f, const Callback &cb) {
	if (auto mod = _runtime->getRuntime()->getModule(::wasm::StringView(m.data(), m.size()))) {
		auto it = mod->exports.find(::wasm::StringView(f.data(), f.size()));
		if (it != mod->exports.end() && it->second.second == ::wasm::ExternalKind::Func) {
			const Func *func = mod->func[it->second.first].first;
			if (func) {
				return _thread->Prepare(*mod, *func, [&] (Value *buf, const auto &prep) {
					return cb(*mod, *func, buf, prep);
				});
			}
		}
	}
	return Thread::Result::TrapHostTrapped;
}

NS_SP_EXT_END(wasm)

#endif /* EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMRUNTIME_H_ */
