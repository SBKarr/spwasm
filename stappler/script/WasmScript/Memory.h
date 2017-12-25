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

#ifndef WASMSCRIPT_MEMORY_H_
#define WASMSCRIPT_MEMORY_H_

#include <WasmScript.h>

typedef struct mem_pool_t mem_pool_t;

WASM_IMPORT mem_pool_t *ws_mem_pool_acquire();
WASM_IMPORT void ws_mem_pool_push(mem_pool_t *);
WASM_IMPORT void ws_mem_pool_pop();

WASM_IMPORT mem_pool_t *ws_mem_pool_create_unmanaged();

// if parent is null, set parent to root thread pool
WASM_IMPORT mem_pool_t *ws_mem_pool_create(mem_pool_t *parent);

WASM_IMPORT void ws_mem_pool_destroy(mem_pool_t *);
WASM_IMPORT void ws_mem_pool_clear(mem_pool_t *);

WASM_IMPORT void *ws_mem_pool_alloc(mem_pool_t *, size_t *);
WASM_IMPORT void *ws_mem_pool_palloc(mem_pool_t *, size_t);
WASM_IMPORT void *ws_mem_pool_calloc(mem_pool_t *, size_t count, size_t eltsize);
WASM_IMPORT void ws_mem_pool_free(mem_pool_t *, void *ptr, size_t size);

#ifdef __cplusplus
namespace script {
namespace mempool {

using pool_t = mem_pool_t;

WASM_INLINE pool_t *acquire() { return ws_mem_pool_acquire(); }

WASM_INLINE void push(pool_t *p) { ws_mem_pool_push(p); }
WASM_INLINE void pop() { ws_mem_pool_pop(); }

WASM_INLINE pool_t *create() { return ws_mem_pool_create_unmanaged(); }

// if parent is null, set parent to root thread pool
WASM_INLINE pool_t *create(pool_t *parent) { return ws_mem_pool_create(parent); }

WASM_INLINE void destroy(pool_t *pool) { ws_mem_pool_destroy(pool); }
WASM_INLINE void clear(pool_t *pool) { ws_mem_pool_clear(pool); }

WASM_INLINE void *alloc(pool_t *p, size_t &s) { return ws_mem_pool_alloc(p, &s); }
WASM_INLINE void *palloc(pool_t *p, size_t s) { return ws_mem_pool_palloc(p, s); }
WASM_INLINE void *calloc(pool_t *p, size_t count, size_t eltsize) { return ws_mem_pool_calloc(p, count, eltsize); }
WASM_INLINE void free(pool_t *p, void *ptr, size_t size) { return ws_mem_pool_free(p, ptr, size); }

}
}
#endif

#endif /* WASMSCRIPT_MEMORY_H_ */
