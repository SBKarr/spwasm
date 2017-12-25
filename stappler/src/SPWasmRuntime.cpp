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

#include "SPCommon.h"
#include "SPWasmRuntime.h"
#include "SPLog.h"
#include "wasm/Binary.h"
#include "wasm/Thread.h"

#include "SPWasmRuntimeHostMath.cc"
#include "SPWasmRuntimeHostString.cc"

NS_SP_EXT_BEGIN(wasm)

Environment::~Environment() {
	if (_env) {
		_env->~Environment();
	}
}

bool Environment::init() {
	memory::pool::push(_pool);

	auto mem = memory::pool::palloc(_pool, sizeof(::wasm::Environment));
	_env = new (mem) ::wasm::Environment();
	_env->setErrorCallback([this] (const ::wasm::StringView &tag, const StringStream &stream) {
		onError(StringView(tag.data(), tag.size()), stream);
	});

	initHostMathFunc();
	initHostStringFunc();

	memory::pool::pop();
	return true;
}

void Environment::setPrintCallback(const PrintCallback &cb) {
	_printCallback = cb;
}
const Environment::PrintCallback &Environment::getPrintCallback() const {
	return _printCallback;
}

Module * Environment::loadModule(const StringView &name, const uint8_t *data, size_t size, const ReadOptions &opts) {
	if (!_loaderPool) {
		_loaderPool = memory::MemPool(_pool.pool());
	}

	Module *ret = nullptr;
	do {
		memory::pool::push(_loaderPool);
		::wasm::ModuleReader reader;
		memory::pool::pop();

		memory::pool::push(_pool);
		ret = _env->loadModule(::wasm::StringView(name.data(), name.size()), reader, data, size, opts);
		memory::pool::pop();
	} while(0);
	_loaderPool.clear();

	return ret;
}
HostModule * Environment::makeHostModule(const StringView &name) {
	memory::pool::push(_pool);
	auto ret = _env->makeHostModule(::wasm::StringView(name.data(), name.size()));
	memory::pool::pop();
	return ret;
}
HostModule * Environment::getEnvModule() const {
	return _env->getEnvModule();
}

const Map<String, Module> &Environment::getExternalModules() const {
	return _env->getExternalModules();
}
const Map<String, HostModule> &Environment::getHostModules() const {
	return _env->getHostModules();
}

const memory::MemPool &Environment::getPool() const {
	return _pool;
}

void Environment::onError(const StringView &tag, const StringStream &stream) const {
	log::text(tag, stream.weak());
}


Runtime::~Runtime() {
	_pool = memory::MemPool::None;
	if (_runtime) {
		_runtime->~Runtime();
	}
}

class ImportInternal {
public:
	static bool onImportFunc(HostFunc &target, const Module::Import &import, void *ptr) {
		return ((Runtime *)ptr)->onImportFunc(target, import);
	}

	static bool onImportGlobal(RuntimeGlobal &target, const Module::Import &import, void *ptr) {
		return ((Runtime *)ptr)->onImportGlobal(target, import);
	}

	static bool onImportMemory(RuntimeMemory &target, const Module::Import &import, void *ptr) {
		return ((Runtime *)ptr)->onImportMemory(target, import);
	}

	static bool onImportTable(RuntimeTable &target, const Module::Import &import, void *ptr) {
		return ((Runtime *)ptr)->onImportTable(target, import);
	}

	static bool onInitMemory(const ::wasm::StringView &module, const ::wasm::StringView &name, RuntimeMemory &target, void *ptr) {
		return ((Runtime *)ptr)->onInitMemory(StringView(module.data(), module.size()), StringView(name.data(), name.size()), target);
	}

	static bool onInitTable(const ::wasm::StringView &module, const ::wasm::StringView &name, RuntimeTable &target, void *ptr) {
		return ((Runtime *)ptr)->onInitTable(StringView(module.data(), module.size()), StringView(name.data(), name.size()), target);
	}

	static bool onAlloc(const RuntimeMemory &mem, uint32_t size, RuntimeMemory::Action a, void *ptr) {
		return ((Runtime *)ptr)->onMemoryAction(mem, size, a);
	}
};

bool Runtime::init(const Arc<Environment> &env) {
	LinkingPolicy p;

	p.func = &ImportInternal::onImportFunc;
	p.global = &ImportInternal::onImportGlobal;
	p.memory = &ImportInternal::onImportMemory;
	p.table = &ImportInternal::onImportTable;
	p.memoryInit = &ImportInternal::onInitMemory;
	p.tableInit = &ImportInternal::onInitTable;
	p.allocator = &ImportInternal::onAlloc;
	p.context = this;

	_env = env;
	memory::pool::push(_pool);

	auto mem = memory::pool::palloc(_pool, sizeof(::wasm::Runtime));
	_runtime = new (mem) ::wasm::Runtime();
	auto ret = _runtime->init(env->_env, p);

	memory::pool::pop();
	return ret;
}

const RuntimeModule *Runtime::getModule(const StringView &name) const {
	return _runtime->getModule(::wasm::StringView(name.data(), name.size()));
}

const RuntimeModule *Runtime::getModule(const Module *mod) const {
	return _runtime->getModule(mod);
}

const Func *Runtime::getExportFunc(const StringView &module, const StringView &name) const {
	if (auto mod = _runtime->getModule(::wasm::StringView(module.data(), module.size()))) {
		return getExportFunc(*mod, name);
	}
	return nullptr;
}
const Func *Runtime::getExportFunc(const RuntimeModule &module, const StringView &name) const {
	auto it = module.exports.find(::wasm::StringView(name.data(), name.size()));
	if (it != module.exports.end() && it->second.second == ::wasm::ExternalKind::Func) {
		if (module.func[it->second.first].first) {
			return module.func[it->second.first].first;
		}
	}
	return nullptr;
}

const RuntimeGlobal *Runtime::getGlobal(const StringView &module, const StringView &name) const {
	if (auto mod = _runtime->getModule(::wasm::StringView(module.data(), module.size()))) {
		return getGlobal(*mod, name);
	}
	return nullptr;
}
const RuntimeGlobal *Runtime::getGlobal(const RuntimeModule &module, const StringView &name) const {
	auto it = module.exports.find(::wasm::StringView(name.data(), name.size()));
	if (it != module.exports.end() && it->second.second == ::wasm::ExternalKind::Global) {
		return module.globals[it->second.first];
	}
	return nullptr;
}

bool Runtime::setGlobal(const StringView &module, const StringView &name, const Value &value) {
	if (auto mod = _runtime->getModule(::wasm::StringView(module.data(), module.size()))) {
		return setGlobal(*mod, name, value);
	}
	return false;
}

bool Runtime::setGlobal(const RuntimeModule &module, const StringView &name, const Value &value) {
	auto it = module.exports.find(::wasm::StringView(name.data(), name.size()));
	if (it != module.exports.end() && it->second.second == ::wasm::ExternalKind::Global) {
		if (module.globals[it->second.first]->mut) {
			module.globals[it->second.first]->value.value = value;
			return true;
		}
	}
	return false;
}

const memory::MemPool &Runtime::getPool() const {
	return _pool;
}

const ::wasm::Runtime *Runtime::getRuntime() const {
	return _runtime;
}

bool Runtime::onMemoryAction(const RuntimeMemory &mem, uint32_t size, RuntimeMemory::Action a) {
	switch (a) {
	case RuntimeMemory::Action::Alloc: {
		auto p = memory::pool::create(_pool);
		mem.data = (uint8_t *)memory::pool::palloc(p, size);
		memset(mem.data, 0, size);
		mem.size = size;
		mem.ctx = p;
		break;
	}
	case RuntimeMemory::Action::Realloc: {
		auto p = memory::pool::create(_pool);
		auto data = (uint8_t *)memory::pool::palloc(p, size);
		memcpy(data, mem.data, mem.size);
		memset(data + mem.size, 0, size - mem.size);
		memory::pool::destroy((memory::pool_t *)mem.ctx);
		mem.data = data;
		mem.size = size;
		mem.ctx = p;
		break;
	}
	case RuntimeMemory::Action::Free:
		if (_pool) {
			memory::pool::destroy((memory::pool_t *)mem.ctx);
		}
		mem.data = nullptr;
		mem.size = 0;
		mem.ctx = nullptr;
		break;
	}
	return true;
}

bool Runtime::onImportFunc(HostFunc &target, const Module::Import &import) {
	return false;
}

bool Runtime::onImportGlobal(RuntimeGlobal &target, const Module::Import &import) {
	return false;
}

bool Runtime::onImportMemory(RuntimeMemory &target, const Module::Import &import) {
	return false;
}

bool Runtime::onImportTable(RuntimeTable &target, const Module::Import &import) {
	return false;
}

bool Runtime::onInitMemory(const StringView &module, const StringView &env, RuntimeMemory &target) {
	return false;
}

bool Runtime::onInitTable(const StringView &module, const StringView &env, RuntimeTable &target) {
	return false;
}


Thread::~Thread() {
	if (_thread) {
		_thread->~Thread();
	}
}

bool Thread::init(const Arc<Runtime> &runtime, uint32_t tag, uint32_t valueStack, uint32_t callStack) {
	_runtime = runtime;
	memory::pool::push(_pool);
	auto mem = memory::pool::palloc(_pool, sizeof(::wasm::Thread));
	_thread = new (mem) ::wasm::Thread(runtime->_runtime, tag);
	auto ret = _thread->init(valueStack, callStack);
	memory::pool::pop();
	return ret;
}

bool Thread::run(const RuntimeModule &module, const Func &func, Value *buffer) {
	return _thread->Run(module, func, buffer) == ::wasm::Thread::Result::Ok;
}

uint8_t *Thread::getMemory(uint32_t memIndex, uint32_t offset) const {
	return _thread->GetMemory(memIndex, offset);
}
uint8_t *Thread::getMemory(uint32_t memIndex, uint32_t offset, uint32_t size) const {
	return _thread->GetMemory(memIndex, offset, size);
}

const memory::MemPool &Thread::getPool() const {
	return _pool;
}

Runtime *Thread::getRuntime() const {
	return _runtime;
}

::wasm::Thread *Thread::getThread() const {
	return _thread;
}

bool Thread::call(const RuntimeModule &module, const Func &func, Vector<Value> &paramsInOut) {
	paramsInOut.resize(std::max(func.sig->params.size(), func.sig->results.size()));
	if (call(module, func, paramsInOut.data())) {
		paramsInOut.resize(func.sig->results.size());
		return true;
	}
	return false;
}

bool Thread::call(const RuntimeModule &module, const Func &func, Value *paramsInOut) {
	auto result = _thread->Run(module, func, paramsInOut);
	if (result == Thread::Result::Returned || result == Thread::Result::Ok) {
		return true;
	}
	::wasm::StringStream stream;
	switch (result) {
	case Thread::Result::Returned:
	case Thread::Result::Ok:
		break;
	case Thread::Result::TrapMemoryAccessOutOfBounds:
		stream << "Execution failed: out of bounds memory access";
		break;
	case Thread::Result::TrapAtomicMemoryAccessUnaligned:
		stream << "Execution failed: atomic memory access is unaligned";
		break;
	case Thread::Result::TrapIntegerOverflow:
		stream << "Execution failed: integer overflow";
		break;
	case Thread::Result::TrapIntegerDivideByZero:
		stream << "Execution failed: integer divide by zero";
		break;
	case Thread::Result::TrapInvalidConversionToInteger:
		stream << "Execution failed: invalid conversion to integer (float is NaN)";
		break;
	case Thread::Result::TrapUndefinedTableIndex:
		stream << "Execution failed: function table index is out of bounds";
		break;
	case Thread::Result::TrapUninitializedTableElement:
		stream << "Execution failed: function table element is uninitialized";
		break;
	case Thread::Result::TrapUnreachable:
		stream << "Execution failed: unreachable instruction executed";
		break;
	case Thread::Result::TrapIndirectCallSignatureMismatch:
		stream << "Execution failed: call indirect signature doesn't match function table signature";
		break;
	case Thread::Result::TrapCallStackExhausted:
		stream << "Execution failed: call stack exhausted, ran out of call stack frames (probably infinite recursion)";
		break;
	case Thread::Result::TrapValueStackExhausted:
		stream << "Execution failed: value stack exhausted, ran out of value stack space";
		break;
	case Thread::Result::TrapUserStackExhausted:
		stream << "Execution failed: user stack exhausted";
		break;
	case Thread::Result::TrapHostResultTypeMismatch:
		stream << "Execution failed: host result type mismatch";
		break;
	case Thread::Result::TrapHostTrapped:
		stream << "Execution failed: import function call was not successful";
		break;
	case Thread::Result::ArgumentTypeMismatch:
		stream << "Execution failed: argument type mismatch";
		break;
	case Thread::Result::UnknownExport:
		stream << "Execution failed: unknown export";
		break;
	case Thread::Result::ExportKindMismatch:
		stream << "Execution failed: export kind mismatch";
		break;
	}
	stream << "\n";
	_thread->PrintStackTrace(stream, 10, 10);
	log::text("Thread", stream.str());
	return false;
}

bool Thread::call(const Func &func, Vector<Value> &paramsInOut) {
	if (auto mod = _runtime->getModule(func.module)) {
		return call(*mod, func, paramsInOut);
	}
	return false;
}
bool Thread::call(const Func &func, Value *paramsInOut) {
	if (auto mod = _runtime->getModule(func.module)) {
		return call(*mod, func, paramsInOut);
	}
	return false;
}

Thread::Result Thread::callSafe(const RuntimeModule &module, const Func &func, Vector<Value> &paramsInOut) {
	paramsInOut.resize(std::max(func.sig->params.size(), func.sig->results.size()));
	auto res = callSafe(module, func, paramsInOut.data());
	if (res == Thread::Result::Ok || res == Thread::Result::Returned) {
		paramsInOut.resize(func.sig->results.size());
		return res;
	}
	return res;
}
Thread::Result Thread::callSafe(const RuntimeModule &module, const Func &func, Value *paramsInOut) {
	return _thread->Run(module, func, paramsInOut, true);
}

Thread::Result Thread::callSafe(const Func &func, Vector<Value> &paramsInOut) {
	if (auto mod = _runtime->getModule(func.module)) {
		return callSafe(*mod, func, paramsInOut);
	}
	return Thread::Result::TrapHostTrapped;
}
Thread::Result Thread::callSafe(const Func &func, Value *paramsInOut) {
	if (auto mod = _runtime->getModule(func.module)) {
		return callSafe(*mod, func, paramsInOut);
	}
	return Thread::Result::TrapHostTrapped;
}

NS_SP_EXT_END(wasm)
