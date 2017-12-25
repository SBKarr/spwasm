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

#ifndef WASMSCRIPT_H_
#define WASMSCRIPT_H_

typedef int int32_t;
typedef long long int int64_t;

typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef uint32_t size_t;
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;

#ifdef __cplusplus
#define WASM_EXPORT extern "C" __attribute__((visibility("default")))
#define WASM_IMPORT extern "C" __attribute__((visibility("default")))
#else
#define WASM_EXPORT __attribute__((visibility("default")))
#define WASM_IMPORT __attribute__((visibility("default"))) extern
#endif

#define WASM_SCRIPT_MAGIC \
		__attribute__((visibility("default"))) uint32_t __wasm_get_stack_pointer() { \
			char internalBuffer[1]; return (uint32_t)&internalBuffer + 1; \
		}

#define WASM_INLINE __attribute__((always_inline)) __attribute__((visibility("hidden"))) inline

#endif /* WASMSCRIPT_H_ */
