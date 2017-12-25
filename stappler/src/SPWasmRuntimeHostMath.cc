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
#include "SPWasmRuntime.h"

#include <math.h>

NS_SP_EXT_BEGIN(wasm)

#define HOST_DOUBLE_FUNC(name) \
	static ::wasm::Result host_ ## name ## d(::wasm::Thread *thread, const HostFunc * func, Value* buf) { \
		buf[0] = double(::name(buf[0].asDouble())); return ::wasm::Result::Ok; \
	}

#define HOST_FLOAT_FUNC(name) \
	static ::wasm::Result host_ ## name ## f(::wasm::Thread *thread, const HostFunc * func, Value* buf) { \
		buf[0] = float(::name ## f(buf[0].asFloat())); return ::wasm::Result::Ok; \
	}

#define HOST_FLOAT_DOUBLE_MATH_FUNC(name) HOST_DOUBLE_FUNC(name) HOST_FLOAT_FUNC(name)


#define ADD_DOUBLE_FUNC(mod, name) mod->addFunc("_ws_" #name "d", &host_ ## name ## d, { Type::F64 }, { Type::F64 } );
#define ADD_FLOAT_FUNC(mod, name) mod->addFunc("_ws_" #name "f", &host_ ## name ## f, { Type::F32 }, { Type::F32 } );

#define ADD_FLOAT_DOUBLE_MATH_FUNC(mod, name) ADD_DOUBLE_FUNC(mod, name) ADD_FLOAT_FUNC(mod, name)

HOST_FLOAT_DOUBLE_MATH_FUNC(cos)
HOST_FLOAT_DOUBLE_MATH_FUNC(sin)
HOST_FLOAT_DOUBLE_MATH_FUNC(tan)
HOST_FLOAT_DOUBLE_MATH_FUNC(acos)
HOST_FLOAT_DOUBLE_MATH_FUNC(asin)
HOST_FLOAT_DOUBLE_MATH_FUNC(atan)
HOST_FLOAT_DOUBLE_MATH_FUNC(cosh)
HOST_FLOAT_DOUBLE_MATH_FUNC(sinh)
HOST_FLOAT_DOUBLE_MATH_FUNC(tanh)
HOST_FLOAT_DOUBLE_MATH_FUNC(acosh)
HOST_FLOAT_DOUBLE_MATH_FUNC(asinh)
HOST_FLOAT_DOUBLE_MATH_FUNC(atanh)
HOST_FLOAT_DOUBLE_MATH_FUNC(exp)
HOST_FLOAT_DOUBLE_MATH_FUNC(log)
HOST_FLOAT_DOUBLE_MATH_FUNC(log10)
HOST_FLOAT_DOUBLE_MATH_FUNC(exp2)
HOST_FLOAT_DOUBLE_MATH_FUNC(sqrt)
HOST_FLOAT_DOUBLE_MATH_FUNC(ceil)
HOST_FLOAT_DOUBLE_MATH_FUNC(floor)
HOST_FLOAT_DOUBLE_MATH_FUNC(trunc)
HOST_FLOAT_DOUBLE_MATH_FUNC(round)
HOST_FLOAT_DOUBLE_MATH_FUNC(fabs)

static ::wasm::Result host_atan2d(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = double(::atan2(buf[0].asDouble(), buf[1].asDouble())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_atan2f(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = float(::atan2f(buf[0].asFloat(), buf[1].asFloat())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_lroundd(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = int32_t(::lround(buf[0].asDouble())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_lroundf(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = int32_t(::lroundf(buf[0].asFloat())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_fmodd(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = ::fmod(buf[0].asDouble(), buf[1].asDouble()); return ::wasm::Result::Ok;
}

static ::wasm::Result host_fmodf(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = float(::fmodf(buf[0].asFloat(), buf[1].asFloat())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_powd(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = ::pow(buf[0].asDouble(), buf[1].asDouble()); return ::wasm::Result::Ok;
}

static ::wasm::Result host_powf(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = float(::powf(buf[0].asFloat(), buf[1].asFloat())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_ldexpd(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = ::ldexp(buf[0].asDouble(), buf[1].asInt32()); return ::wasm::Result::Ok;
}

static ::wasm::Result host_ldexpf(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	buf[0] = float(::ldexpf(buf[0].asFloat(), buf[1].asInt32())); return ::wasm::Result::Ok;
}

static ::wasm::Result host_modfd(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto ptr = (double *)thread->GetMemory(::wasm::Index(0), buf[1].i32, sizeof(double))) {
		buf[0] = ::modf(buf[0].asDouble(), ptr);
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_modff(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto ptr = (float *)thread->GetMemory(::wasm::Index(0), buf[1].i32, sizeof(float))) {
		buf[0] = float(::modff(buf[0].asFloat(), ptr));
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_frexpd(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto ptr = (int32_t *)thread->GetMemory(::wasm::Index(0), buf[1].i32, sizeof(int32_t))) {
		buf[0] = ::frexp(buf[0].asDouble(), ptr);
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_frexpf(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto ptr = (int32_t *)thread->GetMemory(::wasm::Index(0), buf[1].i32, sizeof(int32_t))) {
		buf[0] = float(::frexpf(buf[0].asFloat(), ptr));
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_nand(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (buf[0].i32 == 0) {
		buf[0] = ::nan("");
		return ::wasm::Result::Ok;
	} else {
		if (auto ptr = (const char *)thread->GetMemory(::wasm::Index(0), buf[1].i32, sizeof(int32_t))) {
			buf[0] = ::nan(ptr);
			return ::wasm::Result::Ok;
		}
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_nanf(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (buf[0].i32 == 0) {
		buf[0] = float(::nanf(""));
		return ::wasm::Result::Ok;
	} else {
		if (auto ptr = (const char *)thread->GetMemory(::wasm::Index(0), buf[1].i32, sizeof(int32_t))) {
			buf[0] = float(::nanf(ptr));
			return ::wasm::Result::Ok;
		}
	}
	return ::wasm::Result::Error;
}

void Environment::initHostMathFunc() {
	using Type = ::wasm::Type;

	auto mod = _env->getEnvModule();

	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, cos)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, sin)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, tan)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, acos)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, asin)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, atan)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, cosh)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, sinh)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, tanh)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, acosh)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, asinh)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, atanh)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, exp)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, log)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, log10)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, exp2)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, sqrt)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, ceil)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, floor)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, trunc)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, round)
	ADD_FLOAT_DOUBLE_MATH_FUNC(mod, fabs)

	mod->addFunc("_ws_atan2d", &host_atan2d, { Type::F64, Type::F64 }, { Type::F64 } );
	mod->addFunc("_ws_atan2f", &host_atan2f, { Type::F32, Type::F32 }, { Type::F32 } );

	mod->addFunc("_ws_lroundd", &host_lroundd, { Type::F64 }, { Type::I32 } );
	mod->addFunc("_ws_lroundf", &host_lroundf, { Type::F32 }, { Type::I32 } );

	mod->addFunc("_ws_fmodd", &host_fmodd, { Type::F64, Type::F64 }, { Type::F64 } );
	mod->addFunc("_ws_fmodf", &host_fmodf, { Type::F32, Type::F32 }, { Type::F32 } );

	mod->addFunc("_ws_powd", &host_powd, { Type::F64, Type::F64 }, { Type::F64 } );
	mod->addFunc("_ws_powf", &host_powf, { Type::F32, Type::F32 }, { Type::F32 } );

	mod->addFunc("_ws_ldexpd", &host_ldexpd, { Type::F64, Type::I32 }, { Type::F64 } );
	mod->addFunc("_ws_ldexpf", &host_ldexpf, { Type::F32, Type::I32 }, { Type::F32 } );

	mod->addFunc("_ws_modfd", &host_modfd, { Type::F64, Type::I32 }, { Type::F64 } );
	mod->addFunc("_ws_modff", &host_modff, { Type::F32, Type::I32 }, { Type::F32 } );

	mod->addFunc("_ws_frexpd", &host_frexpd, { Type::F64, Type::I32 }, { Type::F64 } );
	mod->addFunc("_ws_frexpf", &host_frexpf, { Type::F32, Type::I32 }, { Type::F32 } );

	mod->addFunc("_ws_nand", &host_nand, { Type::I32 }, { Type::F64 } );
	mod->addFunc("_ws_nanf", &host_nanf, { Type::I32 }, { Type::F32 } );
}

NS_SP_EXT_END(wasm)
