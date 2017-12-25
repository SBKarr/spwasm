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

#include <string.h>
#include "Environment.h"
#include "Thread.h"

namespace wasm {

HostFunc::HostFunc(TypeInitList params, TypeInitList results, HostFuncCallback cb, void *ctx)
: sig(params, results), callback(cb), ctx(ctx) { }

void HostModule::addGlobal(const StringView &str, TypedValue &&value, bool mut) {
	globals.emplace(std::piecewise_construct,
			std::forward_as_tuple(String(str.data(), str.size())),
			std::forward_as_tuple(std::move(value), mut));
}

void HostModule::addFunc(const StringView &str, TypeInitList params, TypeInitList results, HostFuncCallback cb, void *ctx) {
	funcs.emplace(std::piecewise_construct,
			std::forward_as_tuple(String(str.data(), str.size())),
			std::forward_as_tuple(params, results, cb, ctx));
}

Environment::Environment() {
	_envModule = makeHostModule("env");
}

Module * Environment::loadModule(const StringView &name, const uint8_t *data, size_t size, const ReadOptions &opts) {
	auto it = _externalModules.emplace(String(name.data(), name.size()), Module()).first;
	if (it->second.init(this, data, size, opts)) {
		return &it->second;
	} else {
		_externalModules.erase(it);
	}
	return nullptr;
}

HostModule * Environment::makeHostModule(const StringView &name) {
	auto it = _hostModules.emplace(String(name.data(), name.size()), HostModule()).first;
	return &it->second;
}

HostModule * Environment::getEnvModule() {
	return _envModule;
}

const Map<String, Module> &Environment::getExternalModules() const {
	return _externalModules;
}

const Map<String, HostModule> &Environment::getHostModules() const {
	return _hostModules;
}

bool Environment::getGlobalValue(TypedValue &value, const StringView &module, const StringView &field) const {
	return getGlobalValueRecursive(value, module, field, 0);
}

void Environment::onError(const StringView &tag, const StringStream &stream) const {
	printf("Error: %.*s: %s\n", int(tag.size()), tag.data(), stream.str().data());
}

bool Environment::getGlobalValueRecursive(TypedValue &value, const StringView &module, const StringView &field, Index depth) const {
	if (depth >= 16) {
		return false;
	}

	auto it = _externalModules.find(module);
	if (it != _externalModules.end()) {
		const Module &mod = it->second;
		auto &exports = mod.getExports();
		for (auto &exp_it : exports) {
			if (exp_it.kind == ExternalKind::Global && exp_it.name == field) {
				if (exp_it.index.import) {
					if (auto g = mod.getImportGlobal(exp_it.index.index)) {
						return getGlobalValueRecursive(value, g->module, g->field, depth + 1);
					}
				} else {
					if (auto g = mod.getGlobal(exp_it.index.index)) {
						value = g->value;
						return true;
					}
				}
				return false;
			}
		}
	}

	auto hostIt = _hostModules.find(module);
	if (hostIt != _hostModules.end()) {
		const HostModule &mod = hostIt->second;
		auto gIt = mod.globals.find(field);
		if (gIt != mod.globals.end()) {
			value = gIt->second.value;
			return true;
		}
	}

	return false;
}


bool Runtime::init(const Environment *env, const LinkingPolicy &policy) {
	_env = env;

	performPreLink();

	if (!linkExternalModules(policy)) {
		return false;
	}

	return loadRuntime(policy);
}

RuntimeModule *Runtime::getModule(const StringView &name) {
	auto it = _modules.find(name);
	if (it != _modules.end()) {
		return &it->second;
	}
	return nullptr;
}

const RuntimeModule *Runtime::getModule(const StringView &name) const {
	auto it = _modules.find(name);
	if (it != _modules.end()) {
		return &it->second;
	}
	return nullptr;
}

const RuntimeModule *Runtime::getModule(const Module *mod) const {
	auto it = _runtimeModules.find(mod);
	if (it != _runtimeModules.end()) {
		return it->second;
	}
	return nullptr;
}

void Runtime::performPreLink() {
	Index funcCount = 0;
	Index globalCount = 0;
	Index memoryCount = 0;
	Index tableCount = 0;

	// prepare imports from host modules

	for (auto &it : _env->getHostModules()) {
		RuntimeModule &mod = _modules.emplace(it.first, RuntimeModule()).first->second;
		mod.hostModule = &it.second;
	}

	for (auto &it : _env->getExternalModules()) {
		const Module &source = it.second;

		for (auto &importIt : source.getImports()) {
			if (auto mod = getModule(importIt.module)) {
				switch (importIt.kind) {
				case ExternalKind::Func:
					if (auto host = mod->hostModule) { // follow only imports from host module
						auto hostFnIt = host->funcs.find(importIt.field); // check if function defined in host module
						if (hostFnIt == host->funcs.end()) {
							// mark unknown export and function object
							mod->exports.emplace(importIt.field, std::make_pair(kInvalidIndex, ExternalKind::Func));
							++ funcCount;
						}
					}
					break;
				case ExternalKind::Global:
					if (auto host = mod->hostModule) {
						auto hostGlIt = host->globals.find(importIt.field);
						if (hostGlIt == host->globals.end() || hostGlIt->second.mut) {
							// mark unknown export and function object
							mod->exports.emplace(importIt.field, std::make_pair(kInvalidIndex, ExternalKind::Global));
							++ globalCount;
						}
					}
					break;
				case ExternalKind::Memory:
					mod->exports.emplace(importIt.field, std::make_pair(kInvalidIndex, ExternalKind::Memory));
					++ memoryCount;
					break;
				case ExternalKind::Table:
					mod->exports.emplace(importIt.field, std::make_pair(kInvalidIndex, ExternalKind::Table));
					++ tableCount;
					break;
				}
			}
		}

		// process mutable globals
		for (auto &globalIt : source.getGlobalIndexVec()) {
			if (!globalIt.import) {
				if (auto g = source.getGlobal(globalIt.index)) {
					if (g->mut) {
						++ globalCount;
					}
				}
			}
		}

		for (auto &memoryIt : source.getMemoryIndexVec()) {
			if (!memoryIt.import) {
				++ memoryCount;
			}
		}

		for (auto &tableIt : source.getTableIndexVec()) {
			if (!tableIt.import) {
				++ tableCount;
			}
		}
	}

	_funcs.resize(funcCount);
	_globals.resize(globalCount);
	_memory.resize(memoryCount);
	_tables.resize(tableCount);

	// process environmental functions from host module
	for (auto &it : _modules) {
		if (auto host = it.second.hostModule) {
			// count unknown imports
			Index modGlobalCount = 0;
			Index modFuncCount = 0;
			for (auto &expIt : it.second.exports) {
				switch (expIt.second.second) {
				case ExternalKind::Global: ++ modGlobalCount; break;
				case ExternalKind::Func: ++ modFuncCount; break;
				default: break;
				}
			}

			// reserve environmental + unknown
			it.second.func.reserve(host->funcs.size() + modFuncCount);
			for (auto &funcIt : host->funcs) {
				it.second.func.emplace_back(nullptr, &funcIt.second);
				it.second.exports.emplace(funcIt.first, std::make_pair(it.second.func.size() - 1, ExternalKind::Func));
			}

			Index mutableGlobal = 0;
			it.second.globals.reserve(host->globals.size() + modGlobalCount);
			for (auto &globalIt : host->globals) {
				if (!globalIt.second.mut) {
					// export non-mutable globals directly from environment
					it.second.globals.emplace_back(const_cast<RuntimeGlobal *>(&globalIt.second));
					it.second.exports.emplace(globalIt.first, std::make_pair(it.second.globals.size() - 1, ExternalKind::Global));
				} else {
					// export mutable globals directly by copy
					_globals[globalCount - 1 - mutableGlobal] = globalIt.second;
					it.second.globals.emplace_back(&_globals[globalCount - 1 - mutableGlobal]);
					it.second.exports.emplace(globalIt.first, std::make_pair(it.second.globals.size() - 1, ExternalKind::Global));
					++ mutableGlobal;
				}
			}
		}
	}
}

bool Runtime::linkExternalModules(const LinkingPolicy &policy) {
	Index funcCount = 0;
	Index globalCount = 0;
	Index memoryCount = 0;
	Index tableCount = 0;

	// link defined objects
	for (auto &it : _env->getExternalModules()) {
		const Module &source = it.second;
		RuntimeModule &mod = _modules.emplace(it.first, RuntimeModule()).first->second;
		_runtimeModules.emplace(&it.second, &mod);
		mod.module = &source;

		auto &funcVec = source.getFuncIndexVec();
		auto &globalVec = source.getGlobalIndexVec();
		auto &memoryVec = source.getMemoryIndexVec();
		auto &tableVec = source.getTableIndexVec();

		mod.func.reserve(funcVec.size());
		mod.globals.reserve(globalVec.size());
		mod.memory.reserve(memoryVec.size());
		mod.tables.reserve(tableVec.size());

		for (auto &funcIt : funcVec) {
			if (!funcIt.import) {
				if (auto f = source.getFunc(funcIt.index)) {
					mod.func.emplace_back(f, nullptr);
				}
			} else {
				mod.func.emplace_back(nullptr, nullptr);
			}
		}

		for (auto &globalIt : globalVec) {
			if (!globalIt.import) {
				if (auto g = source.getGlobal(globalIt.index)) {
					if (g->mut) {
						// copy mutable global
						_globals[globalCount] = *g;
						mod.globals.emplace_back(&_globals[globalCount]);
						++ globalCount;
					} else {
						mod.globals.emplace_back(const_cast<RuntimeGlobal *>(g));
					}
				}
			} else {
				mod.globals.emplace_back(nullptr);
			}
		}

		for (auto &memoryIt : memoryVec) {
			if (!memoryIt.import) {
				if (auto mem = source.getMemory(memoryIt.index)) {
					_memory[memoryCount].limits = mem->limits;
				}
				mod.memory.emplace_back(&_memory[memoryCount]);
				++ memoryCount;
			} else {
				mod.memory.emplace_back(nullptr);
			}
		}

		for (auto &tableIt : tableVec) {
			if (!tableIt.import) {
				if (auto tbl = source.getTable(tableIt.index)) {
					_tables[tableCount].limits = tbl->limits;
				}
				mod.tables.emplace_back(&_tables[tableCount]);
				++ tableCount;
			} else {
				mod.tables.emplace_back(nullptr);
			}
		}

		// link exports
		for (auto &exportIt : source.getExports()) {
			mod.exports.emplace(exportIt.name, std::make_pair(exportIt.object, exportIt.kind));
		}
	}

	// all exports was filled, fill imports

	for (auto &it : _modules) {
		RuntimeModule &source = it.second;
		if (source.module) {
			auto mod = source.module;

			auto &funcVec = mod->getFuncIndexVec();
			auto &globalVec = mod->getGlobalIndexVec();
			auto &memoryVec = mod->getMemoryIndexVec();
			auto &tableVec = mod->getTableIndexVec();

			size_t i = 0;
			for (auto &funcIt : funcVec) {
				if (funcIt.import && !processFuncImport(policy, source, i, funcIt, funcCount)) {
					_env->pushErrorStream("Runtile", [&] (std::ostream &stream) {
						stream << "Fail to link with \"" << it.first << "\"";
					});
					return false;
				}
				++ i;
			}

			i = 0;
			for (auto &globalIt : globalVec) {
				if (globalIt.import && !processGlobalImport(policy, source, i, globalIt, globalCount)) {
					return false;
				}
				++ i;
			}

			i = 0;
			for (auto &memoryIt : memoryVec) {
				if (memoryIt.import && !processMemoryImport(policy, source, i, memoryIt, memoryCount)) {
					return false;
				}
				++ i;
			}

			i = 0;
			for (auto &tableIt : tableVec) {
				if (tableIt.import && !processTableImport(policy, source, i, tableIt, tableCount)) {
					return false;
				}
				++ i;
			}
		}
	}

	return true;
}

bool Runtime::processFuncImport(const LinkingPolicy &policy, RuntimeModule &module, Index i, const Module::IndexObject &index, Index &count) {
	auto import = module.module->getImportFunc(index.index);
	if (!import) {
		pushErrorStream([&] (std::ostream &stream) { stream << "Fail to link import: invalid id: " << index.index; });
		return false;
	}

	auto sourceModule = getModule(import->module);
	if (!sourceModule) {
		pushErrorStream([&] (std::ostream &stream) {
			stream << "Fail to link import: \"" << import->module << "\".\"" << import->field << "\": ";
			stream << "module \"" << import->module << "\" not found";
		});
		return false;
	}

	auto exportIt = sourceModule->exports.find(import->field);
	if (exportIt == sourceModule->exports.end()) {
		pushErrorStream([&] (std::ostream &stream) {
			stream << "Fail to link import: \"" << import->module << "\".\"" << import->field << "\": ";
			stream << "field \"" << import->module << "\".\"" << import->field << "\" is not exported";
		});
		return false;
	}

	bool doImport = false;
	if (exportIt->second.first < sourceModule->func.size()) {
		auto &fn = sourceModule->func[exportIt->second.first];
		if (fn.first || fn.second) {
			// function was successfully defined, perform import
			if (!isSignatureMatch(*import->func.sig, fn)) {
				pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import->module << "\".\"" << import->field << "\": ";
					stream << "invalid export function signature";
				});
				return false;
			}
			module.func[i] = fn;
			return true;
		} else {
			// function was not exported, try recursive import
			doImport = true;
		}
	}

	if (doImport || exportIt->second.first == kInvalidIndex) {
		if (sourceModule->module) {
			// try recursive import
			if (auto idx = sourceModule->module->getFunctionIndex(exportIt->second.first)) {
				if (idx->import && processFuncImport(policy, *sourceModule, exportIt->second.first, *idx, count)) {
					module.func[i] = sourceModule->func[exportIt->second.first];
				} else {
					return false;
				}
			} else {
				pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import->module << "\".\"" << import->field << "\": ";
					stream << "invalid function index: " << exportIt->second.first;
				});
				return false;
			}
		} else if (sourceModule->hostModule) {
			// function was not defined in environment, try to use policy
			auto &fn = _funcs[count];
			if (!policy.func || !policy.func(fn, *import, policy.context)) {
				pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import->module << "\".\"" << import->field << "\": ";
					stream << "fail to request host function";
				});
				return false;
			}

			if (!isSignatureMatch(*import->func.sig, fn.sig)) {
				pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import->module << "\".\"" << import->field << "\": ";
					stream << "invalid host function signature";
				});
				return false;
			}

			sourceModule->func.emplace_back(nullptr, &fn);
			exportIt->second.first = sourceModule->func.size() - 1;
			module.func[i].second = &fn;
			++ count;
		}
	}

	return true;
}

template <typename ImportStruct, typename RuntimeType, typename ValidateCallback, typename RecursiveCallback, typename HostCallback>
bool processImport(const Runtime *env, RuntimeModule &module, RuntimeModule &sourceModule, const ImportStruct &import, Index i, Index &count,
		Vector<RuntimeType *> &sourceVec, Vector<RuntimeType *> &targetVec, Vector<RuntimeType> &globalVec,
		const ValidateCallback &validateCallback, const RecursiveCallback &recursiveCallback, const HostCallback &hostCallback) {
	auto exportIt = sourceModule.exports.find(import.field);
	if (exportIt == sourceModule.exports.end()) {
		env->pushErrorStream([&] (std::ostream &stream) {
			stream << "Fail to link import: \"" << import.module << "\".\"" << import.field << "\": ";
			stream << "field \"" << import.module << "\".\"" << import.field << "\" is not exported";
		});
		return false;
	}

	bool doImport = false;
	if (exportIt->second.first < sourceVec.size()) {
		auto obj = sourceVec[exportIt->second.first];
		if (obj) {
			if (!validateCallback(*obj, import)) {
				env->pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import.module << "\".\"" << import.field << "\": ";
					stream << "export field validation failed";
				});
				return false;
			}
			targetVec[i] = obj;
			return true;
		} else {
			doImport = true;
		}
	}

	if (doImport || exportIt->second.first == kInvalidIndex) {
		if (sourceModule.module) {
			// try recursive import
			if (recursiveCallback(sourceModule, exportIt->second.first, count)) {
				targetVec[i] = sourceVec[exportIt->second.first];
			} else {
				env->pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import.module << "\".\"" << import.field << "\": ";
					stream << "invalid object index: " << exportIt->second.first;
				});
				return false;
			}
		} else if (sourceModule.hostModule) {
			// global was not defined in environment, try to use policy
			auto &obj = globalVec[count];
			if (!hostCallback(obj, import)) {
				env->pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import.module << "\".\"" << import.field << "\": ";
					stream << "fail to request host field";
				});
				return false;
			}

			if (!validateCallback(obj, import)) {
				env->pushErrorStream([&] (std::ostream &stream) {
					stream << "Fail to link import: \"" << import.module << "\".\"" << import.field << "\": ";
					stream << "host field validation failed";
				});
				return false;
			}

			sourceVec.emplace_back(&obj);
			exportIt->second.first = sourceVec.size() - 1;
			targetVec[i] = &obj;
			++ count;
		}
	}

	return true;
}

bool Runtime::processGlobalImport(const LinkingPolicy &policy, RuntimeModule &module, Index i, const Module::IndexObject &index, Index &count) {
	auto import = module.module->getImportGlobal(index.index);
	if (!import) { return false; }

	auto sourceModule = getModule(import->module);
	if (!sourceModule) { return false; }

	return processImport(this, module, *sourceModule, *import, i, count, sourceModule->globals, module.globals, _globals,
			[&] (RuntimeGlobal &obj, const Module::Import &import) -> bool {
		return obj.value.type == import.global.type;
	}, [&] (RuntimeModule &sourceModule, Index i, Index &count) -> bool {
		if (auto idx = sourceModule.module->getGlobalIndex(i)) {
			if (idx->import && processGlobalImport(policy, sourceModule, i, *idx, count)) {
				return true;
			}
		}
		return false;
	}, [&] (RuntimeGlobal &obj, const Module::Import &import) {
		if (!policy.global || !policy.global(obj, import, policy.context)) {
			return false;
		}
		return true;
	});
}

bool Runtime::processMemoryImport(const LinkingPolicy &policy, RuntimeModule &module, Index i, const Module::IndexObject &index, Index &count) {
	auto import = module.module->getImportMemory(index.index);
	if (!import) { return false; }

	auto sourceModule = getModule(import->module);
	if (!sourceModule) { return false; }

	return processImport(this, module, *sourceModule, *import, i, count, sourceModule->memory, module.memory, _memory,
			[&] (RuntimeMemory &obj, const Module::Import &import) -> bool {
		if (import.memory.limits.initial > obj.limits.initial) {
			obj.limits.initial = import.table.limits.initial;
		}
		return true;
	}, [&] (RuntimeModule &sourceModule, Index i, Index &count) -> bool {
		if (auto idx = sourceModule.module->getMemoryIndex(i)) {
			if (idx->import && processMemoryImport(policy, sourceModule, i, *idx, count)) {
				return true;
			}
		}
		return false;
	}, [&] (RuntimeMemory &obj, const Module::Import &import) {
		if (!policy.memory || !policy.memory(obj, import, policy.context)) {
			return false;
		}
		return true;
	});
}

bool Runtime::processTableImport(const LinkingPolicy &policy, RuntimeModule &module, Index i, const Module::IndexObject &index, Index &count) {
	auto import = module.module->getImportTable(index.index);
	if (!import) { return false; }

	auto sourceModule = getModule(import->module);
	if (!sourceModule) { return false; }

	return processImport(this, module, *sourceModule, *import, i, count, sourceModule->tables, module.tables, _tables,
			[&] (RuntimeTable &obj, const Module::Import &import) -> bool {
		if (import.table.limits.initial > obj.limits.initial) {
			obj.limits.initial = import.table.limits.initial;
		}
		return true;
	}, [&] (RuntimeModule &sourceModule, Index i, Index &count) -> bool {
		if (auto idx = sourceModule.module->getTableIndex(i)) {
			if (idx->import && processTableImport(policy, sourceModule, i, *idx, count)) {
				return true;
			}
		}
		return false;
	}, [&] (RuntimeTable &obj, const Module::Import &import) {
		if (!policy.table || !policy.table(obj, import, policy.context)) {
			return false;
		}
		return true;
	});
}

bool Runtime::isSignatureMatch(const Module::Signature &sig, const std::pair<const Func *, const HostFunc *> &func) const {
	if (func.second) {
		return isSignatureMatch(sig, func.second->sig);
	} else if (func.first) {
		return isSignatureMatch(sig, *func.first->sig);
	}
	return false;
}

bool Runtime::isSignatureMatch(const Module::Signature &sig1, const Module::Signature &sig2) const {
	if (sig1.params == sig2.params && sig1.results == sig2.results) {
		return true;
	} else {
		pushErrorStream([&] (std::ostream &stream) {
			stream << "Signature matching failed: ";
			sig1.printInfo(stream);
			stream << " vs ";
			sig2.printInfo(stream);
		});
		return false;
	}
}

StringView Runtime::getModuleName(const RuntimeModule *mod) const {
	for (auto &it : _modules) {
		if (mod == &it.second) {
			return it.first;
		}
	}
	return StringView();
}

std::pair<Index, StringView> Runtime::getModuleFunctionName(const RuntimeModule &mod, const Func *func) const {
	Index idx = kInvalidIndex;
	Index i = 0;
	for (auto &it : mod.func) {
		if (it.first == func) {
			idx = i;
			break;
		}
		++ i;
	}

	if (idx != kInvalidIndex && mod.module) {
		if (auto idxObj = mod.module->getFunctionIndex(idx)) {
			if (idxObj->import) {
				if (auto import = mod.module->getImportFunc(idxObj->index)) {
					return std::pair<Index, StringView>(idx, import->field);
				}
			} else {
				for (auto &expIt : mod.exports) {
					if (expIt.second.second == ExternalKind::Func && expIt.second.first == idx) {
						return std::pair<Index, StringView>(idx, expIt.first);
					}
				}
			}
		}
	}
	return std::pair<Index, StringView>(idx, StringView());
}

const Environment *Runtime::getEnvironment() const {
	return _env;
}


void Runtime::onError(StringStream &stream) const {
	_env->onError("Runtime", stream);
}

void Runtime::onThreadError(const Thread &thread) const {
	pushErrorStream([&] (std::ostream &s) {
		thread.PrintStackTrace(s);
	});
}

bool Runtime::loadRuntime(const LinkingPolicy &policy) {
	if (_lazyInit) {
		return true;
	}

	for (auto &it : _memory) {
		initMemory(it);
	}

	for (auto &it : _tables) {
		initTable(it);
	}

	for (auto &it : _modules) {
		auto &name = it.first;
		auto &mod = it.second;
		if (mod.module) {
			for (auto &it : mod.module->getMemoryData()) {
				if (!emplaceMemoryData(*mod.memory[it.memory], it)) {
					pushErrorStream([&] (std::ostream &stream) {
						stream << "Memory initialization failed for " << "\"" << name << "\"";
					});
					return false;
				}
			}
			for (auto &it : mod.module->getTableElements()) {
				if (!emplaceTableElements(*mod.tables[it.table], it)) {
					pushErrorStream([&] (std::ostream &stream) {
						stream << "Table initialization failed for " << "\"" << name << "\"";
					});
					return false;
				}
			}
		} else if (mod.hostModule) {
			for (auto &expIt : mod.exports) {
				if (expIt.second.second == ExternalKind::Memory && policy.memoryInit) {
					if (!policy.memoryInit(it.first, expIt.first, *mod.memory[expIt.second.first], policy.context)) {
						pushErrorStream([&] (std::ostream &stream) {
							stream << "Host memory initialization failed for "
									<< "\"" << it.first << "\".\"" << expIt.first << "\"";
						});
						return false;
					}
				} else if (expIt.second.second == ExternalKind::Table && policy.tableInit) {
					if (!policy.tableInit(it.first, expIt.first, *mod.tables[expIt.second.first], policy.context)) {
						pushErrorStream([&] (std::ostream &stream) {
							stream << "Host table initialization failed for "
									<< "\"" << it.first << "\".\"" << expIt.first << "\"";
						});
						return false;
					}
				}
			}
		}
	}

	return true;
}

void Runtime::initMemory(RuntimeMemory &memory) {
	memory.data.resize(memory.limits.initial * WABT_PAGE_SIZE);
}

bool Runtime::emplaceMemoryData(RuntimeMemory &memory, const Module::Data &data) {
	if (memory.data.empty() && !data.data.empty() && data.offset == 0 && data.data.size() < WABT_PAGE_SIZE) {
		memory.data.resize(1 * WABT_PAGE_SIZE);
	}
	if (memory.data.size() < data.offset + data.data.size()) {
		pushErrorStream([&] (std::ostream &stream) {
			stream << "Fail to emplace memory data, position out of bounds: " << data.offset << ":" << data.data.size();
		});
		return false;
	}

	auto ptr = memory.data.data() + data.offset;
	memcpy(ptr, data.data.data(), data.data.size());

	if (data.offset + data.data.size() > memory.userDataOffset) {
		memory.userDataOffset = data.offset + data.data.size();
	}
	return true;
}

void Runtime::initTable(RuntimeTable &table) {
	Value fillValue; fillValue.i32 = kInvalidIndex;
	table.values.resize(table.limits.initial, fillValue);
}

bool Runtime::emplaceTableElements(RuntimeTable &table, const Module::Elements &elements) {
	if (table.values.empty() && !elements.values.empty() && elements.offset == 0) {
		table.values.resize(elements.values.size());
	}
	if (table.values.size() < elements.offset + elements.values.size()) {
		pushErrorStream([&] (std::ostream &stream) {
			stream << "Fail to emplace elements, position out of bounds: " << elements.offset << ":" << elements.values.size();
		});
		return false;
	}
	Index i = 0;
	for (auto &it : elements.values) {
		table.values[i + elements.offset] = uint32_t(it);
		++ i;
	}
	return true;
}

}
