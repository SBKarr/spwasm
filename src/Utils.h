/*
 * Copyright 2017 WebAssembly Community Group participants
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

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include <stddef.h>
#include <type_traits>
#include <assert.h>
#include <string.h>

#include "Std.h"
#include "StringView.h"

#define WASM_ASSERT(val) assert(val)

namespace wasm {

enum class Result {
	Ok,
	Error,
};

inline Result operator|(Result lhs, Result rhs) {
	return (lhs == Result::Error || rhs == Result::Error) ? Result::Error : Result::Ok;
}

inline bool Succeeded(Result result) { return result == Result::Ok; }
inline bool Failed(Result result) { return result == Result::Error; }

using Index = uint32_t;    // An index into one of the many index spaces.
using Address = uint32_t;  // An address or size in linear memory.
using Offset = size_t;     // An offset into a host's file or memory buffer.

static constexpr Address kInvalidAddress = Address(~0);
static constexpr Index kInvalidIndex = Index(~0);
static constexpr Offset kInvalidOffset = Offset(~0);

static constexpr auto WABT_PAGE_SIZE = 0x10000; /* 64k */
static constexpr auto WABT_MAX_PAGES = 0x10000; /* # of pages that fit in 32-bit address space */

struct Limits {
	uint64_t initial = 0;
	uint64_t max = 0;
	bool has_max = false;
	bool is_shared = false;
};

enum class LimitsShareable {
	Allowed,
	NotAllowed
};

enum class BinarySection {
	Custom = 0,
	Type = 1,
	Import = 2,
	Function = 3,
	Table = 4,
	Memory = 5,
	Global = 6,
	Export = 7,
	Start = 8,
	Elem = 9,
	Code = 10,
	Data = 11,
	Invalid,
	First = Custom,
	Last = Data,
};

/* matches binary format, do not change */
enum class Type {
	I32 = -0x01,
	I64 = -0x02,
	F32 = -0x03,
	F64 = -0x04,
	Anyfunc = -0x10,
	Func = -0x20,
	Void = -0x40,
	___ = Void, /* convenient for the opcode table in opcode.h */
	Any = 0, /* Not actually specified, but useful for type-checking */
};

/* matches binary format, do not change */
enum class ExternalKind {
	Func = 0,
	Table = 1,
	Memory = 2,
	Global = 3,
	Except = 4,

	First = Func,
	Last = Except,
};

enum class RelocType {
	FuncIndexLEB = 0,       // e.g. Immediate of call instruction
	TableIndexSLEB = 1,     // e.g. Loading address of function
	TableIndexI32 = 2,      // e.g. Function address in DATA
	MemoryAddressLEB = 3,   // e.g. Memory address in load/store offset immediate
	MemoryAddressSLEB = 4,  // e.g. Memory address in i32.const
	MemoryAddressI32 = 5,   // e.g. Memory address in DATA
	TypeIndexLEB = 6,       // e.g. Immediate type in call_indirect
	GlobalIndexLEB = 7,     // e.g. Immediate of get_global inst

	First = FuncIndexLEB,
	Last = GlobalIndexLEB,
};

enum class LabelType {
	Func,
	Block,
	Loop,
	If,
	Else,
	Try,
	Catch,

	First = Func,
	Last = Catch,
};

using TypeVector = Vector<Type>;

class Features {
public:
	void enableAll() {
		_exceptionsEnabled = true;
		_satFloatToIntEnabled = true;
		_threadsEnabled = true;
	}

	bool isExceptionsEnabled() const { return _exceptionsEnabled; }
	bool isSatFloatToIntEnabled() const { return _satFloatToIntEnabled; }
	bool isThreadsEnabled() const { return _threadsEnabled; }

	void setExceptionsEnabled(bool value) { _exceptionsEnabled = value; }
	void setSatFloatToIntEnabled(bool value) { _satFloatToIntEnabled = value; }
	void setThreadsEnabled(bool value) { _threadsEnabled = value; }

private:
	bool _exceptionsEnabled = false;
	bool _satFloatToIntEnabled = false;
	bool _threadsEnabled = false;
};

struct ReadOptions {
	ReadOptions() = default;
	ReadOptions(const Features& features, bool read_debug_names, bool stop_on_first_error)
	: features(features), read_debug_names(read_debug_names), stop_on_first_error(stop_on_first_error) { }

	Features features;
	bool read_debug_names = false;
	bool stop_on_first_error = true;
};

template <typename T>
struct ValueTypeRepT;

template <> struct ValueTypeRepT<int32_t> { typedef uint32_t type; };
template <> struct ValueTypeRepT<uint32_t> { typedef uint32_t type; };
template <> struct ValueTypeRepT<int64_t> { typedef uint64_t type; };
template <> struct ValueTypeRepT<uint64_t> { typedef uint64_t type; };
template <> struct ValueTypeRepT<float> { typedef uint32_t type; };
template <> struct ValueTypeRepT<double> { typedef uint64_t type; };

template <typename T>
using ValueTypeRep = typename ValueTypeRepT<T>::type;

union Value {
	uint32_t i32;
	uint64_t i64;
	ValueTypeRep<float> f32_bits;
	ValueTypeRep<double> f64_bits;

	Value() = default;
	Value(const Value &) = default;
	Value &operator=(const Value &) = default;

	Value(uint32_t value) : i32(value) { }
	Value(uint64_t value) : i64(value) { }
	Value(int32_t value) { memcpy(&i32, &value, sizeof(int32_t)); }
	Value(int64_t value) { memcpy(&i64, &value, sizeof(int64_t)); }
	Value(float value) { memcpy(&f32_bits, &value, sizeof(float)); }
	Value(double value) { memcpy(&f64_bits, &value, sizeof(double)); }

	float asFloat() { float ret; memcpy(&ret, &f32_bits, sizeof(float)); return ret; }
	double asDouble() { double ret; memcpy(&ret, &f64_bits, sizeof(double)); return ret; }
};

struct TypedValue {
	TypedValue() { }
	explicit TypedValue(Type type) : type(type) { }
	TypedValue(Type type, const Value& value) : type(type), value(value) { }

	Type type = Type::Void;
	Value value;
};

using TypedValues = Vector<TypedValue> ;

}  // namespace wasm


#define WABT_PRINTF_FORMAT(format_arg, first_arg) __attribute__((format(printf, (format_arg), (first_arg))))
#define WABT_UNUSED __attribute__ ((unused))
#define WABT_WARN_UNUSED __attribute__ ((warn_unused_result))
#define WABT_INLINE inline
#define WABT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define WABT_LIKELY(x) __builtin_expect(!!(x), 1)
#define WABT_UNREACHABLE __builtin_unreachable()


#endif /* SRC_UTILS_H_ */
