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

#include "RuntimeEnvironment.h"

namespace wasm {

ThreadedRuntime::ThreadedRuntime() : _mainThread(this) { }

bool ThreadedRuntime::init(const Environment *env, const LinkingThreadOptions &opts) {
	if (!Runtime::init(env, opts)) {
		return false;
	}

	return _mainThread.init(opts.valueStackSize, opts.callStackSize);
}

const Func *ThreadedRuntime::getExportFunc(const StringView &module, const StringView &name) const {
	if (auto mod = getModule(module)) {
		return getExportFunc(*mod, name);
	}
	return nullptr;
}
const Func *ThreadedRuntime::getExportFunc(const RuntimeModule &module, const StringView &name) const {
	auto it = module.exports.find(name);
	if (it != module.exports.end() && it->second.second == ExternalKind::Func) {
		if (module.func[it->second.first].first) {
			return module.func[it->second.first].first;
		}
	}
	return nullptr;
}

const RuntimeGlobal *ThreadedRuntime::getGlobal(const StringView &module, const StringView &name) const {
	if (auto mod = getModule(module)) {
		return getGlobal(*mod, name);
	}
	return nullptr;
}
const RuntimeGlobal *ThreadedRuntime::getGlobal(const RuntimeModule &module, const StringView &name) const {
	auto it = module.exports.find(name);
	if (it != module.exports.end() && it->second.second == ExternalKind::Global) {
		return module.globals[it->second.first];
	}
	return nullptr;
}

bool ThreadedRuntime::setGlobal(const StringView &module, const StringView &name, const Value &value) {
	if (auto mod = getModule(module)) {
		return setGlobal(*mod, name, value);
	}
	return false;
}

bool ThreadedRuntime::setGlobal(const RuntimeModule &module, const StringView &name, const Value &value) {
	auto it = module.exports.find(name);
	if (it != module.exports.end() && it->second.second == ExternalKind::Global) {
		if (module.globals[it->second.first]->mut) {
			module.globals[it->second.first]->value.value = value;
			return true;
		}
	}
	return false;
}

bool ThreadedRuntime::call(const RuntimeModule &module, const Func &func, Vector<Value> &paramsInOut) {
	paramsInOut.resize(std::max(func.sig->params.size(), func.sig->results.size()));
	if (call(module, func, paramsInOut.data())) {
		paramsInOut.resize(func.sig->results.size());
		return true;
	}
	return false;
}

bool ThreadedRuntime::call(const RuntimeModule &module, const Func &func, Value *paramsInOut) {
	auto result = _mainThread.Run(&module, &func, paramsInOut);
	if (result == Thread::Result::Returned || result == Thread::Result::Ok) {
		return true;
	}
	_env->pushErrorStream("Thread", [&] (std::ostream &stream) {
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
		_mainThread.PrintStackTrace(stream, 10, 10);
	});
	return false;
}

bool ThreadedRuntime::call(const Func &func, Vector<Value> &paramsInOut) {
	if (auto mod = getModule(func.module)) {
		return call(*mod, func, paramsInOut);
	}
	return false;
}
bool ThreadedRuntime::call(const Func &func, Value *paramsInOut) {
	if (auto mod = getModule(func.module)) {
		return call(*mod, func, paramsInOut);
	}
	return false;
}

Thread::Result ThreadedRuntime::callSafe(const RuntimeModule &module, const Func &func, Vector<Value> &paramsInOut) {
	paramsInOut.resize(std::max(func.sig->params.size(), func.sig->results.size()));
	auto res = callSafe(module, func, paramsInOut.data());
	if (res == Thread::Result::Ok || res == Thread::Result::Returned) {
		paramsInOut.resize(func.sig->results.size());
		return res;
	}
	return res;
}
Thread::Result ThreadedRuntime::callSafe(const RuntimeModule &module, const Func &func, Value *paramsInOut) {
	_silent = true;
	auto res = _mainThread.Run(&module, &func, paramsInOut);
	_silent = false;
	return res;
}

Thread::Result ThreadedRuntime::callSafe(const Func &func, Vector<Value> &paramsInOut) {
	if (auto mod = getModule(func.module)) {
		return callSafe(*mod, func, paramsInOut);
	}
	return Thread::Result::TrapHostTrapped;
}
Thread::Result ThreadedRuntime::callSafe(const Func &func, Value *paramsInOut) {
	if (auto mod = getModule(func.module)) {
		return callSafe(*mod, func, paramsInOut);
	}
	return Thread::Result::TrapHostTrapped;
}

void ThreadedRuntime::onError(StringStream &stream) const {
	if (!_silent) {
		Runtime::onError(stream);
	}
}
void ThreadedRuntime::onThreadError(const Thread &thread) const {
	if (!_silent) {
		Runtime::onThreadError(thread);
	}
}

}
