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

#ifndef SRC_ENVIRONMENT_H_
#define SRC_ENVIRONMENT_H_

#include "Module.h"

namespace wasm {

struct HostFunc {
	Module::Signature sig;
	HostFuncCallback callback = nullptr;
	void *ctx = nullptr;

	Index getParamsCount() const { return sig.params.size(); }
	const Vector<Type> &getParamTypes() const { return sig.params; }

	Index getResultsCount() const { return sig.results.size(); }
	const Vector<Type> &getResultTypes() const { return sig.results; }

	HostFunc() = default;
	HostFunc(TypeInitList params, TypeInitList results, HostFuncCallback, void *ctx);
};

struct HostModule {
	using Global = Module::Global;

	Map<String, Global> globals;
	Map<String, HostFunc> funcs;

	void addGlobal(const StringView &, TypedValue &&value, bool mut);
	void addFunc(const StringView &, TypeInitList params, TypeInitList results, HostFuncCallback, void *ctx = nullptr);
};

struct RuntimeMemory {
	Limits limits;
	Vector<uint8_t> data;
	Index userDataOffset = 0;
};

struct RuntimeTable {
	Type type = Type::Anyfunc;
	Limits limits;
	Vector<Value> values;
};

using RuntimeGlobal = Module::Global;

struct RuntimeModule {
	Vector<RuntimeMemory *> memory;
	Vector<RuntimeTable *> tables;
	Vector<RuntimeGlobal *> globals;
	Vector<std::pair<const Func *, const HostFunc *>> func;

	Map<String, std::pair<Index, ExternalKind>> exports;

	const Module *module = nullptr;
	const HostModule *hostModule = nullptr;
};

struct LinkingPolicy {
	using ImportFuncCallback = bool (*) (HostFunc &target, const Module::Import &import, void *);
	using ImportGlobalCallback = bool (*) (RuntimeGlobal &target, const Module::Import &import, void *);
	using ImportMemoryCallback = bool (*) (RuntimeMemory &target, const Module::Import &import, void *);
	using ImportTableCallback = bool (*) (RuntimeTable &target, const Module::Import &import, void *);
	using InitMemoryCallback = bool (*) (const StringView &module, const StringView &env, RuntimeMemory &target, void *);
	using InitTableCallback = bool (*) (const StringView &module, const StringView &env, RuntimeTable &target, void *);

	ImportFuncCallback func;
	ImportGlobalCallback global;
	ImportMemoryCallback memory;
	ImportTableCallback table;

	InitMemoryCallback memoryInit;
	InitTableCallback tableInit;

	void *context = nullptr;
};

class Runtime {
public:
	virtual ~Runtime() { }

	bool init(const Environment *, const LinkingPolicy &);

	RuntimeModule *getModule(const StringView &);
	const RuntimeModule *getModule(const StringView &) const;

	const RuntimeModule *getModule(const Module *) const;

	bool isSignatureMatch(const Module::Signature &, const std::pair<const Func *, const HostFunc *> &func) const;
	bool isSignatureMatch(const Module::Signature &, const Module::Signature &) const;

	StringView getModuleName(const RuntimeModule *) const;
	std::pair<Index, StringView> getModuleFunctionName(const RuntimeModule &, const Func *) const;

	const Environment *getEnvironment() const;

	template <typename Callback>
	void pushErrorStream(const Callback &cb) const;

	virtual void onError(StringStream &) const;
	virtual void onThreadError(const Thread &) const;

protected:
	bool performCall(const RuntimeModule *module, Index func, Value *buf, Index initialSize);

	void performPreLink();

	bool linkExternalModules(const LinkingPolicy &);

	bool processFuncImport(const LinkingPolicy &, RuntimeModule &, Index i, const Module::IndexObject &index, Index &count);
	bool processGlobalImport(const LinkingPolicy &, RuntimeModule &, Index i, const Module::IndexObject &index, Index &count);
	bool processMemoryImport(const LinkingPolicy &, RuntimeModule &, Index i, const Module::IndexObject &index, Index &count);
	bool processTableImport(const LinkingPolicy &, RuntimeModule &, Index i, const Module::IndexObject &index, Index &count);

	bool loadRuntime(const LinkingPolicy &);

	void initMemory(RuntimeMemory &);
	bool emplaceMemoryData(RuntimeMemory &, const Module::Data &);

	void initTable(RuntimeTable &);
	bool emplaceTableElements(RuntimeTable &, const Module::Elements &);

	bool _lazyInit = false;
	const Environment *_env = nullptr;
	Map<String, RuntimeModule> _modules;
	Map<const Module *, const RuntimeModule *> _runtimeModules;

	Vector<RuntimeTable> _tables;
	Vector<RuntimeMemory> _memory;
	Vector<RuntimeGlobal> _globals;
	Vector<HostFunc> _funcs;
};

class Environment {
public:
	Environment();

	Module * loadModule(const StringView &, const uint8_t *, size_t, const ReadOptions & = ReadOptions());
	HostModule * makeHostModule(const StringView &);
	HostModule * getEnvModule();

	const Map<String, Module> &getExternalModules() const;
	const Map<String, HostModule> &getHostModules() const;

	bool getGlobalValue(TypedValue &, const StringView &module, const StringView &field) const;

	void onError(const StringView &, const StringStream &) const;

	template <typename Callback>
	void pushErrorStream(const StringView &, const Callback &cb) const;

private:
	bool getGlobalValueRecursive(TypedValue &, const StringView &module, const StringView &field, Index depth) const;

	HostModule *_envModule = nullptr;
	Map<String, HostModule> _hostModules;
	Map<String, Module> _externalModules;
};

template <typename Callback>
inline void Runtime::pushErrorStream(const Callback &cb) const {
	StringStream stream;
	cb(stream);
	onError(stream);
}

template <typename Callback>
inline void Environment::pushErrorStream(const StringView &tag, const Callback &cb) const {
	StringStream stream;
	cb(stream);
	onError(tag, stream);
}

}

#endif /* SRC_ENVIRONMENT_H_ */
