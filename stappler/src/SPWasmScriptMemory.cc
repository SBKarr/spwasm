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

#include "SPWasmScriptRuntime.h"

NS_SP_EXT_BEGIN(wasm)

static ::wasm::Result host_pool_acquire(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	MemPtr<ThreadContext> ctxPtr(thread->getUserStackPointer());

	auto threadCtx = ctxPtr.get(memCtx);
	auto pool = threadCtx->top(memCtx);
	buf[0].i32 = pool.addr();
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_push(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	MemPtr<ThreadContext> ctxPtr(thread->getUserStackPointer());

	auto threadCtx = ctxPtr.get(memCtx);
	threadCtx->push(memCtx, buf[0].i32);
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_pop(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	MemPtr<ThreadContext> ctxPtr(thread->getUserStackPointer());
	auto threadCtx = ctxPtr.get(memCtx);
	threadCtx->pop(memCtx);
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_create_unmanaged(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	buf[0].i32 = ADDRESS(memCtx, Pool::create(memCtx)).addr();
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_create(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	Pool *pool = nullptr;
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		pool = poolCtx.get(memCtx);
	} else {
		MemPtr<ThreadContext> ctxPtr(thread->getUserStackPointer());
		ThreadContext * threadCtx = ctxPtr.get(memCtx);
		pool = threadCtx->top(memCtx).get(memCtx);
	}
	buf[0].i32 = ADDRESS(memCtx, pool->make_child(memCtx)).addr();
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_destroy(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		Pool *pool = poolCtx.get(memCtx);
		pool->do_destroy(memCtx);
	}
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_clear(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		Pool *pool = poolCtx.get(memCtx);
		pool->clear(memCtx);
	}
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_alloc(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		Pool *pool = poolCtx.get(memCtx);

		MemPtr<uint32_t> sizePtr(buf[1].i32);

		uint32_t *ptr = sizePtr.get(memCtx);
		size_t size = *ptr;
		buf[0].i32 = pool->alloc(memCtx, size).addr();
		*ptr = uint32_t(size);
	}
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_palloc(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		Pool *pool = poolCtx.get(memCtx);

		size_t size = buf[1].i32;
		buf[0].i32 = pool->alloc(memCtx, size).addr();
	}
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_calloc(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		Pool *pool = poolCtx.get(memCtx);

		size_t size = buf[1].i32 * buf[2].i32;
		MemPtr<uint8_t> ret = pool->alloc(memCtx, size);
		memset(ret.get(memCtx), 0, size);
		buf[0].i32 = ret.addr();
	}
	return ::wasm::Result::Ok;
}

static ::wasm::Result host_pool_free(::wasm::Thread *thread, const HostFunc * func, Value* buf) {
	MemCtx memCtx(thread);
	if (buf[0].i32) {
		MemPtr<Pool> poolCtx(buf[0].i32);
		Pool *pool = poolCtx.get(memCtx);

		pool->free(memCtx, MemPtr<uint8_t>(buf[1].i32), size_t(buf[2].i32));
	}
	return ::wasm::Result::Ok;
}

void ScriptEnvironment::initHostMemFunc() {
	using Type = ::wasm::Type;

	auto mod = _env->getEnvModule();

	mod->addFunc("ws_mem_pool_acquire", &host_pool_acquire, { }, { Type::I32 } );
	mod->addFunc("ws_mem_pool_push", &host_pool_push, { Type::I32 }, { } );
	mod->addFunc("ws_mem_pool_pop", &host_pool_pop, { }, { } );
	mod->addFunc("ws_mem_pool_create_unmanaged", &host_pool_create_unmanaged, { }, { Type::I32 } );
	mod->addFunc("ws_mem_pool_create", &host_pool_create, { Type::I32 }, { Type::I32 } );
	mod->addFunc("ws_mem_pool_destroy", &host_pool_destroy, { Type::I32 }, { } );
	mod->addFunc("ws_mem_pool_clear", &host_pool_clear, { Type::I32 }, { } );

	mod->addFunc("ws_mem_pool_alloc", &host_pool_alloc, { Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("ws_mem_pool_palloc", &host_pool_palloc, { Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("ws_mem_pool_calloc", &host_pool_calloc, { Type::I32, Type::I32, Type::I32 }, { Type::I32 } );
	mod->addFunc("ws_mem_pool_free", &host_pool_free, { Type::I32, Type::I32, Type::I32 }, { } );
}

NS_SP_EXT_END(wasm)
