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

NS_SP_EXT_BEGIN(wasm)

static ::wasm::Result host_memcpy(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	auto mem = thread->GetMemoryPtr(0);
	auto dest = mem->get(buf[0].i32, buf[2].i32);
	auto src = mem->get(buf[1].i32, buf[2].i32);
	if (dest && src) {
		if (memcpy(dest, src, buf[2].i32)) {
			return ::wasm::Result::Ok; // result already on stack
		}
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_memmove(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	auto mem = thread->GetMemoryPtr(0);
	auto dest = mem->get(buf[0].i32, buf[2].i32);
	auto src = mem->get(buf[1].i32, buf[2].i32);
	if (dest && src) {
		if (memmove(dest, src, buf[2].i32)) {
			return ::wasm::Result::Ok; // result already on stack
		}
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_memcmp(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	auto mem = thread->GetMemoryPtr(0);
	auto s1 = mem->get(buf[0].i32, buf[2].i32);
	auto s2 = mem->get(buf[1].i32, buf[2].i32);
	if (s1 && s2) {
		auto res = memcmp(s1, s2, buf[2].i32);
		memcpy(&buf[0].i32, &res, sizeof(int));
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_memset(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto s = thread->GetMemory(::wasm::Index(0), buf[0].i32, buf[2].i32)) {
		if (memset(s, buf[1].i32, buf[2].i32)) {
			return ::wasm::Result::Ok; // result already on stack
		}
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_strlen(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto s = (const char *)thread->GetMemory(::wasm::Index(0), buf[0].i32)) {
		buf[0].i32 = uint32_t(strlen(s));
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_strcmp(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	auto mem = thread->GetMemoryPtr(0);
	auto s1 = (const char *)mem->get(buf[0].i32);
	auto s2 = (const char *)mem->get(buf[1].i32);
	if (s1 && s2) {
		buf[0].i32 = strcmp(s1, s2);
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_strncmp(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	auto mem = thread->GetMemoryPtr(0);
	auto s1 = (const char *)mem->get(buf[0].i32);
	auto s2 = (const char *)mem->get(buf[1].i32);
	if (s1 && s2) {
		buf[0].i32 = strncmp(s1, s2, size_t(buf[2].i32));
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_ws_print(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto d = (const char *)thread->GetMemory(::wasm::Index(0), buf[0].i32)) {
		if (const auto &cb = ((Environment *)func->ctx)->getPrintCallback()) {
			cb(d);
		} else {
			std::cout << (const char *)d;
		}
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

static ::wasm::Result host_ws_printn(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	if (auto d = thread->GetMemory(::wasm::Index(0), buf[0].i32, buf[1].i32)) {
		if (const auto &cb = ((Environment *)func->ctx)->getPrintCallback()) {
			cb(StringView((const char *)d, buf[1].i32));
		} else {
			std::cout.write((const char *)d, buf[1].i32);
		}
		return ::wasm::Result::Ok;
	}
	return ::wasm::Result::Error;
}

void Environment::initHostStringFunc() {
	using Type = ::wasm::Type;

	auto mod = _env->getEnvModule();

	mod->addFunc("memcpy", &host_memcpy, { Type::I32, Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("memmove", &host_memmove, { Type::I32, Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("memcmp", &host_memcmp, { Type::I32, Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("memset", &host_memset, { Type::I32, Type::I32, Type::I32 }, { Type::I32 } );

	mod->addFunc("strlen", &host_strlen, { Type::I32 }, { Type::I32 } );
	mod->addFunc("strcmp", &host_strcmp, { Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("strncmp", &host_strncmp, { Type::I32, Type::I32, Type::I32 }, { Type::I32 } );

	mod->addFunc("ws_print", &host_ws_print, { Type::I32 }, { }, this);
	mod->addFunc("ws_printn", &host_ws_printn, { Type::I32, Type::I32 }, { }, this);
}

NS_SP_EXT_END(wasm)
