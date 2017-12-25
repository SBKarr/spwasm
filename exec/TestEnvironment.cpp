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

#include "TestEnvironment.h"
#include "SExpr.h"
#include <iostream>
#include <math.h>
#include <inttypes.h>

namespace wasm {
namespace test {

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

static Result spectest_print(const Thread *thread, const HostFunc * func, Value* buffer) {
	auto ptr = buffer[0].i32;

	if (auto mem = (const char *)thread->GetMemory(0, ptr)) {
		printf("%s\n", mem);
	}

	return Result::Ok;
}

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

static bool assert_return(ThreadedRuntime &runtime, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf, TypedValue ret) {
	if (auto fn = runtime.getExportFunc(module, func)) {
		//fn->printInfo(std::cout);
		if (runtime.call(*fn, buf.data())) {

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

static bool assert_trap(ThreadedRuntime &runtime, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf, Thread::Result res) {
	stream << "\"" << module << "\".\"" << func << "\": ";

	if (auto fn = runtime.getExportFunc(module, func)) {
		auto r = runtime.callSafe(*fn, buf.data());
		if (r == res) {
			stream << "assert_trap success\n";
			return true;
		}
	}
	stream << "assert_trap failed\n";
	return false;
}

static bool assert_return_canonical_nan(ThreadedRuntime &runtime, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf) {
	stream << "\"" << module << "\".\"" << func << "\": ";
	if (auto fn = runtime.getExportFunc(module, func)) {
		auto &res = fn->sig->results;
		if (res.size() > 0 && (res.front() == Type::F32 || res.front() == Type::F64)) {
			if (runtime.call(*fn, buf.data())) {
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

static bool assert_return_arithmetic_nan(ThreadedRuntime &runtime, std::ostream &stream, StringView module, StringView func, Vector<Value> &buf) {
	stream << "\"" << module << "\".\"" << func << "\": ";
	if (auto fn = runtime.getExportFunc(module, func)) {
		auto &res = fn->sig->results;
		if (res.size() > 0 && (res.front() == Type::F32 || res.front() == Type::F64)) {
			if (runtime.call(*fn, buf.data())) {
				if (buf.size() > 0) {
					bool ret = false;
					auto &val = buf.front();
					switch (res.front()) {
					case Type::F32: ret = (val.f32_bits & F32_NAN) == F32_NAN; break;
					case Type::F64: ret = (val.f64_bits & F64_NAN) == F64_NAN; break;
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

static TestEnvironment *s_instance = nullptr;

TestEnvironment *TestEnvironment::getInstance() {
	if (!s_instance) {
		s_instance = new TestEnvironment();
	}
	return s_instance;
}

TestEnvironment::TestEnvironment() {
	_testModule = makeHostModule("spectest");

	_testModule->addFunc("print", { Type::I32 }, {}, &spectest_print, this);
}

bool TestEnvironment::run() {
	wasm::ThreadedRuntime runtime;
	if (runtime.init(this, wasm::LinkingThreadOptions())) {

		for (auto &it : _tests) {
			runTest(runtime, it);
			//break;
		}

		/*if (auto mod = runtime.getModule("f32")) {
			//mod->module->printInfo(std::cout);
			if (auto fn = runtime.getExportFunc("f32", "add")) {
				//fn->printInfo(std::cout);
				assert_return( runtime, std::cout, "f32", "add", { float(-0x0p+0), uint32_t(NAN32_BITS | 0x200000) }, I64_CONST(0x164) );
			}
		}*/

		/*auto count = sizeof(s_tests) / sizeof(struct test_rec);

		for (Index i = 0; i < count; ++ i) {
			if (auto mod = runtime.getModule(s_tests[i].name)) {
				StringStream stream;
				std::cout << "== Begin " << s_tests[i].name << " ==\n";
				if (!s_tests[i].fn(runtime, stream, s_tests[i].name)) {
					std::cout << stream.str();
					std::cout << "== Failed ==\n";
				} else {
					std::cout << "== Success ==\n";
				}
			}
		}*/

		return true;
	} else {
		return false;
	}
}

bool TestEnvironment::loadAsserts(const StringView &name, const uint8_t *buf, size_t size) {
	_tests.emplace_back(Test{String(name.data(), name.size()), String((const char *)buf,  size)});

	auto &b = _tests.back();
	b.list = sexpr::parse(b.data);
	return true;
}

static TypedValue parse_return_value(const sexpr::Token &token) {
	if (token.kind == sexpr::Token::List && token.vec.size() == 2) {
		auto t = token.vec[1].token;
		if (token.token == "i32.const") {
			bool unaryMinus = false;
			if (t[0] == '-') {
				unaryMinus = true;
				t = StringView(t.data() + 1, t.size() - 1);
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
				t = StringView(t.data() + 1, t.size() - 1);
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
			funcName = invoke.vec[1].token;
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

static bool run_assert_return(wasm::ThreadedRuntime &runtime, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	TypedValue result(Type::Any);
	StringView funcName = read_invoke(token.vec[1], buf);
	if (token.vec.size() > 2) {
		result = parse_return_value(token.vec[2]);
	}
	if (!funcName.empty()) {
		return assert_return(runtime, stream, name, funcName, buf, result);
	}
	return false;
}

static bool run_assert_return_canonical_nan(wasm::ThreadedRuntime &runtime, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token.vec[1], buf);
	if (!funcName.empty()) {
		return assert_return_canonical_nan(runtime, stream, name, funcName, buf);
	}
	return false;
}

static bool run_assert_return_arithmetic_nan(wasm::ThreadedRuntime &runtime, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token.vec[1], buf);
	if (!funcName.empty()) {
		return assert_return_arithmetic_nan(runtime, stream, name, funcName, buf);
	}
	return false;
}

static bool run_assert_trap(wasm::ThreadedRuntime &runtime, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token.vec[1], buf);
	Thread::Result expected = Thread::Result::Ok;
	if (token.vec.size() > 2) {
		if (token.vec[2].token == "call stack exhausted") {
			expected = Thread::Result::TrapCallStackExhausted;
		} else if (token.vec[2].token == "value stack exhausted") {
			expected = Thread::Result::TrapValueStackExhausted;
		} else if (token.vec[2].token == "out of bounds memory access") {
			expected = Thread::Result::TrapMemoryAccessOutOfBounds;
		} else if (token.vec[2].token == "integer overflow") {
			expected = Thread::Result::TrapIntegerOverflow;
		} else if (token.vec[2].token == "invalid conversion to integer") {
			expected = Thread::Result::TrapInvalidConversionToInteger;
		} else if (token.vec[2].token == "unreachable executed" || token.vec[2].token == "unreachable") {
			expected = Thread::Result::TrapUnreachable;
		} else if (token.vec[2].token == "indirect call signature mismatch") {
			expected = Thread::Result::TrapIndirectCallSignatureMismatch;
		} else if (token.vec[2].token == "undefined element") {
			expected = Thread::Result::TrapUndefinedTableIndex;
		} else if (token.vec[2].token == "integer divide by zero") {
			expected = Thread::Result::TrapIntegerDivideByZero;
		} else {
			stream << token.vec[2].token << "\n";
			return false;
		}
	}
	if (!funcName.empty()) {
		return assert_trap(runtime, stream, name, funcName, buf, expected);
	}
	return false;
}

static bool run_invoke(wasm::ThreadedRuntime &runtime, std::ostream &stream, StringView name, const sexpr::Token &token) {
	Vector<Value> buf;
	StringView funcName = read_invoke(token, buf);
	if (!funcName.empty()) {
		if (auto fn = runtime.getExportFunc(name, funcName)) {
			if (runtime.call(*fn, buf.data())) {
				return true;
			}
		}
	}
	return false;
}

bool TestEnvironment::runTest(wasm::ThreadedRuntime &runtime, const Test &test) {
	if (auto mod = runtime.getModule(test.name)) {
		bool success = true;
		StringStream stream;
		std::cout << "== Begin " << test.name << " ==\n";
		for (auto &it : test.list) {
			if (it.token == "assert_return") {
				if (!run_assert_return(runtime, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_return_canonical_nan") {
				if (!run_assert_return_canonical_nan(runtime, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_return_arithmetic_nan") {
				if (!run_assert_return_arithmetic_nan(runtime, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_trap") {
				if (!run_assert_trap(runtime, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "assert_exhaustion") {
				if (!run_assert_trap(runtime, stream, test.name, it)) {
					success = false;
				}
			} else if (it.token == "invoke") {
				if (!run_invoke(runtime, stream, test.name, it)) {
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

}
}
