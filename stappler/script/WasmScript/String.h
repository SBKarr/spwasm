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

#ifndef WASMSCRIPT_STRING_H_
#define WASMSCRIPT_STRING_H_

#include <WasmScript.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"

WASM_IMPORT void *memcpy(void *dest, const void *src, size_t n);
WASM_IMPORT void *memmove(void *dest, const void *src, size_t n);
WASM_IMPORT int memcmp(const char *s1, const char *s2, size_t n);
WASM_IMPORT void *memset(char *, int z, size_t);

WASM_IMPORT size_t strlen(const char *);
WASM_IMPORT int strcmp(const char *, const char *);
WASM_IMPORT int strncmp(const char *, const char *, size_t);

WASM_IMPORT void ws_print(const char *);
WASM_IMPORT void ws_printn(const char *);

#pragma clang diagnostic push

#endif /* EXTENSIONS_SPWASM_STAPPLER_SCRIPT_WASMSCRIPT_STRING_H_ */
