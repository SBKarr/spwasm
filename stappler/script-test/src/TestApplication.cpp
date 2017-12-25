/*
 * Copyright 2018 Roman Katuntsev <sbkarr@stappler.org>
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
#include "TestApplication.h"
#include "SPLog.h"

#include <iostream>
#include <math.h>
#include <inttypes.h>

NS_SP_EXT_BEGIN(app)

using TypedValue = ::wasm::TypedValue;
using Value = ::wasm::Value;
using Type = ::wasm::Type;
using Index = ::wasm::Index;

constexpr uint32_t F32_NEG = 0x80000000U;
constexpr uint32_t F32_NAN_BASE = 0x7f800000U;
constexpr uint32_t F32_NAN_BIT = 0x00400000U;
constexpr uint32_t F32_NAN = F32_NAN_BASE | F32_NAN_BIT;
constexpr uint32_t F32_NAN_NEG = F32_NAN | F32_NEG;

constexpr uint64_t F64_NEG = 0x8000000000000000ULL;
constexpr uint64_t F64_NAN_BASE = 0x7ff0000000000000ULL;
constexpr uint64_t F64_NAN_BIT = 0x0008000000000000ULL;
constexpr uint64_t F64_NAN = F64_NAN_BASE | F64_NAN_BIT;
constexpr uint64_t F64_NAN_NEG = F64_NAN | F64_NEG;

static bool compare_value(const TypedValue &tval, const Value &val) {
	switch (tval.type) {
	case Type::I32: return tval.value.i32 == val.i32; break;
	case Type::I64: return tval.value.i64 == val.i64; break;
	case Type::F32: return tval.value.f32_bits == val.f32_bits; break;
	case Type::F64: return tval.value.f64_bits == val.f64_bits; break;
	case Type::Any: return true; break;
	default: return false; break;
	}
	return false;
}

static bool assert_return(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf, TypedValue ret) {
	if (auto fn = runtime.getExportFunc(module, func)) {
		//fn->printInfo(std::cout);
		if (thread.call(*fn, buf.data())) {

			//stream << "ret: f32 " << buf.front().asFloat() << " ";
			if (compare_value(ret, buf.front())) {
				stream << "\"" << module << "\".\"" << func << "\": assert_return success\n";
				return true;
			}
		}
	}
	stream << "\"" << module << "\".\"" << func << "\": assert_return failed\n";
	return false;
}

static bool assert_trap(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf, wasm::Thread::Result res) {
	stream << "\"" << module << "\".\"" << func << "\": ";

	if (auto fn = runtime.getExportFunc(module, func)) {
		auto r = thread.callSafe(*fn, buf.data());
		if (r == res) {
			stream << "assert_trap success\n";
			return true;
		}
	}
	stream << "assert_trap failed\n";
	return false;
}

static bool assert_return_canonical_nan(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf) {
	stream << "\"" << module << "\".\"" << func << "\": ";
	if (auto fn = runtime.getExportFunc(module, func)) {
		auto &res = fn->sig->results;
		if (res.size() > 0 && (res.front() == Type::F32 || res.front() == Type::F64)) {
			if (thread.call(*fn, buf.data())) {
				if (buf.size() > 0) {
					bool ret = false;
					auto &val = buf.front();
					switch (res.front()) {
					case Type::F32: {
						ret = (val.f32_bits == F32_NAN || val.f32_bits == F32_NAN_NEG);
						break;
					}
					case Type::F64: {
						ret = (val.f64_bits == F64_NAN || val.f64_bits == F64_NAN_NEG);
						break;
					}
					default:
						break;
					}
					if (ret) {
						stream << "assert_return_canonical_nan success\n";
						return true;
					}
				}
			}
		}
	}
	stream << "assert_return_canonical_nan failed\n";
	return false;
}

static bool assert_return_arithmetic_nan(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf) {
	stream << "\"" << module << "\".\"" << func << "\": ";
	if (auto fn = runtime.getExportFunc(module, func)) {
		auto &res = fn->sig->results;
		if (res.size() > 0 && (res.front() == Type::F32 || res.front() == Type::F64)) {
			if (thread.call(*fn, buf.data())) {
				if (buf.size() > 0) {
					bool ret = false;
					auto &val = buf.front();
					switch (res.front()) {
					case Type::F32: ret = (val.f32_bits & F32_NAN) == F32_NAN; break;
					case Type::F64: ret = (val.f64_bits & F64_NAN) == F64_NAN; break;
					default: ret = false; break;
					}
					if (ret) {
						stream << "assert_return_arithmetic_nan success\n";
						return true;
					}
				}
			}
		}
	}
	stream << "assert_return_arithmetic_nan failed\n";
	return false;
}

static TypedValue parse_return_value(const sexpr::Token &token) {
	if (token.kind == sexpr::Token::List && token.vec.size() == 2) {
		auto t = token.vec[1].token;
		if (token.token == "i32.const") {
			bool unaryMinus = false;
			if (t[0] == '-') {
				unaryMinus = true;
				t = ::wasm::StringView(t.data() + 1, t.size() - 1);
			}

			uint32_t value = strtoumax(t.data(), nullptr, 0);
			if (unaryMinus) {
				return TypedValue(Type::I32, int32_t((value > INT32_MAX) ? INT32_MIN : -value));
			}
			return TypedValue(Type::I32, value);
		} else if (token.token == "i64.const") {
			bool unaryMinus = false;
			if (t[0] == '-') {
				unaryMinus = true;
				t = ::wasm::StringView(t.data() + 1, t.size() - 1);
			}

			uint64_t value = strtoumax(t.data(), nullptr, 0);
			if (unaryMinus) {
				return TypedValue(Type::I64, int64_t((value > INT64_MAX) ? INT64_MIN : -value));
			}
			return TypedValue(Type::I64, value);
		} else if (token.token == "f32.const") {
			if (strncmp(t.data(), "nan", 3) == 0 || strncmp(t.data(), "-nan", 4) == 0) {
				uint32_t value = F32_NAN_BASE;
				Index offset = 3;
				if (t[0] == '-') {
					value |= F32_NEG;
					++ offset;
				}
				return TypedValue(Type::F32,
						value | ((t[offset] == ':') ? uint32_t(strtoumax(t.data() + offset + 1, nullptr, 0)) : F32_NAN_BIT));
			}
			return TypedValue(Type::F32, strtof(t.data(), nullptr));
		} else if (token.token == "f64.const") {
			if (strncmp(t.data(), "nan", 3) == 0 || strncmp(t.data(), "-nan", 4) == 0) {
				uint64_t value = F64_NAN_BASE;
				Index offset = 3;
				if (t[0] == '-') {
					value |= F64_NEG;
					++ offset;
				}
				return TypedValue(Type::F64,
						value | ((t[offset] == ':') ? uint64_t(strtoumax(t.data() + offset + 1, nullptr, 0)) : F64_NAN_BIT));
			}
			return TypedValue(Type::F64, strtod(t.data(), nullptr));
		}
	}
	return TypedValue(Type::Any);
}

static Value parse_parameter_value(const sexpr::Token &token) {
	return parse_return_value(token).value;
}

static StringView read_invoke(const sexpr::Token &invoke, Vector<Value> &buf) {
	StringView funcName;
	if (invoke.kind == sexpr::Token::List && invoke.vec.size() >= 2) {
		if (invoke.token == "invoke") {
			funcName = StringView(invoke.vec[1].token.data(), invoke.vec[1].token.size());
			for (size_t i = 2; i < invoke.vec.size(); ++ i) {
				auto val = parse_parameter_value(invoke.vec[i]);
				buf.push_back(val);
			}
		}
	}

	if (buf.empty()) {
		buf.resize(1);
	}

	return funcName;
}

static bool run_assert_return(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	TypedValue result(Type::Any);
	StringView funcName = read_invoke(token.vec[1], buf);
	if (token.vec.size() > 2) {
		result = parse_return_value(token.vec[2]);
	}
	if (!funcName.empty()) {
		return assert_return(runtime, thread, stream, name, funcName, buf, result);
	}
	return false;
}

static bool run_assert_return_canonical_nan(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token.vec[1], buf);
	if (!funcName.empty()) {
		return assert_return_canonical_nan(runtime, thread, stream, name, funcName, buf);
	}
	return false;
}

static bool run_assert_return_arithmetic_nan(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token.vec[1], buf);
	if (!funcName.empty()) {
		return assert_return_arithmetic_nan(runtime, thread, stream, name, funcName, buf);
	}
	return false;
}

static bool run_assert_trap(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token.vec[1], buf);
	wasm::Thread::Result expected = wasm::Thread::Result::Ok;
	if (token.vec.size() > 2) {
		if (token.vec[2].token == "call stack exhausted") {
			expected = wasm::Thread::Result::TrapCallStackExhausted;
		} else if (token.vec[2].token == "value stack exhausted") {
			expected = wasm::Thread::Result::TrapValueStackExhausted;
		} else if (token.vec[2].token == "out of bounds memory access") {
			expected = wasm::Thread::Result::TrapMemoryAccessOutOfBounds;
		} else if (token.vec[2].token == "integer overflow") {
			expected = wasm::Thread::Result::TrapIntegerOverflow;
		} else if (token.vec[2].token == "invalid conversion to integer") {
			expected = wasm::Thread::Result::TrapInvalidConversionToInteger;
		} else if (token.vec[2].token == "unreachable executed" || token.vec[2].token == "unreachable") {
			expected = wasm::Thread::Result::TrapUnreachable;
		} else if (token.vec[2].token == "indirect call signature mismatch") {
			expected = wasm::Thread::Result::TrapIndirectCallSignatureMismatch;
		} else if (token.vec[2].token == "undefined element") {
			expected = wasm::Thread::Result::TrapUndefinedTableIndex;
		} else if (token.vec[2].token == "integer divide by zero") {
			expected = wasm::Thread::Result::TrapIntegerDivideByZero;
		} else {
			stream << token.vec[2].token << "\n";
			return false;
		}
	}
	if (!funcName.empty()) {
		return assert_trap(runtime, thread, stream, name, funcName, buf, expected);
	}
	return false;
}

static bool run_invoke(wasm::Runtime &runtime, wasm::Thread &thread, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token, buf);
	if (!funcName.empty()) {
		if (auto fn = runtime.getExportFunc(name, funcName)) {
			if (thread.call(*fn, buf.data())) {
				return true;
			}
		}
	}
	return false;
}

static TestApplication *s_sharedApp = nullptr;

TestApplication *TestApplication::getInstance() {
	if (!s_sharedApp) {
		s_sharedApp = new TestApplication;
	}
	return s_sharedApp;
}

TestApplication::TestApplication() {
	memory::pool::initialize();
	_env = Arc<wasm::Environment>::create();
	_env->init();

	_testPool = memory::MemPool(_pool.pool());

	memory::pool::push(_pool);
}

TestApplication::~TestApplication() {
	_env = nullptr;
	memory::pool::pop();
	memory::pool::terminate();
}

bool TestApplication::loadAsserts(const StringView &name, const Bytes &buf) {
	memory::pool::push(_testPool);
	_tests.emplace_back(Test{wasm::String(name.data(), name.size()), wasm::String((const char *)buf.data(),  buf.size())});

	auto &b = _tests.back();
	b.list = sexpr::parse(b.data.data());
	memory::pool::pop();
	return true;
}

bool TestApplication::loadModule(const StringView &name, const Bytes &buf) {
	if (_env->loadModule(name, buf.data(), buf.size())) {
		return true;
	}
	return false;
}

bool TestApplication::runTest(wasm::Runtime *runtime, wasm::Thread *thread, const Test &test) {
	if (runtime->getModule(test.name)) {
		bool success = true;
		StringStream stream;
		std::cout << "== Begin " << test.name << " ==\n";
		for (auto &it : test.list) {
			if (it.token == "assert_return") {
				if (!run_assert_return(*runtime, *thread, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_return_canonical_nan") {
				if (!run_assert_return_canonical_nan(*runtime, *thread, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_return_arithmetic_nan") {
				if (!run_assert_return_arithmetic_nan(*runtime, *thread, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_trap") {
				if (!run_assert_trap(*runtime, *thread, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_exhaustion") {
				if (!run_assert_trap(*runtime, *thread, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "invoke") {
				if (!run_invoke(*runtime, *thread, stream, test.name, it)) {
					success = false;
				}
			} else {
				std::cout << it.token << "\n";
			}
		}

		if (!success) {
			std::cout << stream.str();
			std::cout << "== Failed ==\n";
		} else {
			std::cout << "== Success ==\n";
		}
		return success;
	}
	return false;
}

void TestApplication::run() {
	log::format("Root", "allocated: %lu", _pool.getAllocatedBytes());
	log::format("Test", "allocated: %lu", _testPool.getAllocatedBytes());
	log::format("Env", "allocated: %lu", _env->getPool().getAllocatedBytes());

	auto runtime = Arc<wasm::Runtime>::create();
	if (runtime->init(_env)) {
		log::format("Runtime", "allocated: %lu", runtime->getPool().getAllocatedBytes());
		log::format("System", "allocated: %lu", memory::pool::get_allocator_allocated_bytes(_env->getPool()));
		auto thread = Arc<wasm::Thread>::create();
		if (thread->init(runtime)) {
			log::format("Thread", "allocated: %lu", thread->getPool().getAllocatedBytes());
			for (auto &it : _tests) {
				runTest(runtime, thread, it);
			}
			log::format("Thread", "allocated: %lu", thread->getPool().getAllocatedBytes());
		}
		log::format("Runtime", "allocated: %lu", runtime->getPool().getAllocatedBytes());
		log::format("System", "allocated: %lu", memory::pool::get_allocator_allocated_bytes(_env->getPool()));
	}
}

NS_SP_EXT_END(app)
