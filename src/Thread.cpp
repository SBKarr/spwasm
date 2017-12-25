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

#include "Thread.h"
#include "Opcode.h"
#include "Environment.h"
#include <cmath>
#include <type_traits>
#include <string.h>

namespace wasm {

#define WABT_TABLE_ENTRY_SIZE (sizeof(Offset) + sizeof(uint32_t) + sizeof(uint8_t))
#define WABT_TABLE_ENTRY_OFFSET_OFFSET 0
#define WABT_TABLE_ENTRY_DROP_OFFSET sizeof(uint32_t)
#define WABT_TABLE_ENTRY_KEEP_OFFSET (sizeof(Offset) + sizeof(uint32_t))

inline int Clz(unsigned x) { return x ? __builtin_clz(x) : sizeof(x) * 8; }
inline int Clz(unsigned long x) { return x ? __builtin_clzl(x) : sizeof(x) * 8; }
inline int Clz(unsigned long long x) { return x ? __builtin_clzll(x) : sizeof(x) * 8; }

inline int Ctz(unsigned x) { return x ? __builtin_ctz(x) : sizeof(x) * 8; }
inline int Ctz(unsigned long x) { return x ? __builtin_ctzl(x) : sizeof(x) * 8; }
inline int Ctz(unsigned long long x) { return x ? __builtin_ctzll(x) : sizeof(x) * 8; }

inline int Popcount(unsigned x) { return __builtin_popcount(x); }
inline int Popcount(unsigned long x) { return __builtin_popcountl(x); }
inline int Popcount(unsigned long long x) { return __builtin_popcountll(x); }

#define wabt_convert_uint64_to_double(x) static_cast<double>(x)
#define wabt_convert_uint64_to_float(x) static_cast<float>(x)

template <typename Dst, typename Src>
Dst Bitcast(Src value) {
	static_assert(sizeof(Src) == sizeof(Dst), "Bitcast sizes must match.");
	Dst result;
	memcpy(&result, &value, sizeof(result));
	return result;
}

uint32_t ToRep(bool x) { return x ? 1 : 0; }
uint32_t ToRep(uint32_t x) { return x; }
uint64_t ToRep(uint64_t x) { return x; }
uint32_t ToRep(int32_t x) { return Bitcast<uint32_t>(x); }
uint64_t ToRep(int64_t x) { return Bitcast<uint64_t>(x); }
uint32_t ToRep(float x) { return Bitcast<uint32_t>(x); }
uint64_t ToRep(double x) { return Bitcast<uint64_t>(x); }

template <typename Dst, typename Src>
Dst FromRep(Src x);

template <> uint32_t FromRep<uint32_t>(uint32_t x) { return x; }
template <> uint64_t FromRep<uint64_t>(uint64_t x) { return x; }
template <> int32_t FromRep<int32_t>(uint32_t x) { return Bitcast<int32_t>(x); }
template <> int64_t FromRep<int64_t>(uint64_t x) { return Bitcast<int64_t>(x); }
template <> float FromRep<float>(uint32_t x) { return Bitcast<float>(x); }
template <> double FromRep<double>(uint64_t x) { return Bitcast<double>(x); }

template <typename T>
struct FloatTraits;

template <typename R, typename T>
bool IsConversionInRange(ValueTypeRep<T> bits);

/* 3 32222222 222...00
 * 1 09876543 210...10
 * -------------------
 * 0 00000000 000...00 => 0x00000000 => 0
 * 0 10011101 111...11 => 0x4effffff => 2147483520                  (~INT32_MAX)
 * 0 10011110 000...00 => 0x4f000000 => 2147483648
 * 0 10011110 111...11 => 0x4f7fffff => 4294967040                 (~UINT32_MAX)
 * 0 10111110 111...11 => 0x5effffff => 9223371487098961920         (~INT64_MAX)
 * 0 10111110 000...00 => 0x5f000000 => 9223372036854775808
 * 0 10111111 111...11 => 0x5f7fffff => 18446742974197923840       (~UINT64_MAX)
 * 0 10111111 000...00 => 0x5f800000 => 18446744073709551616
 * 0 11111111 000...00 => 0x7f800000 => inf
 * 0 11111111 000...01 => 0x7f800001 => nan(0x1)
 * 0 11111111 111...11 => 0x7fffffff => nan(0x7fffff)
 * 1 00000000 000...00 => 0x80000000 => -0
 * 1 01111110 111...11 => 0xbf7fffff => -1 + ulp      (~UINT32_MIN, ~UINT64_MIN)
 * 1 01111111 000...00 => 0xbf800000 => -1
 * 1 10011110 000...00 => 0xcf000000 => -2147483648                  (INT32_MIN)
 * 1 10111110 000...00 => 0xdf000000 => -9223372036854775808         (INT64_MIN)
 * 1 11111111 000...00 => 0xff800000 => -inf
 * 1 11111111 000...01 => 0xff800001 => -nan(0x1)
 * 1 11111111 111...11 => 0xffffffff => -nan(0x7fffff)
 */

template<>
struct FloatTraits<float> {
	static const uint32_t kMax = 0x7f7fffffU;
	static const uint32_t kInf = 0x7f800000U;
	static const uint32_t kNegMax = 0xff7fffffU;
	static const uint32_t kNegInf = 0xff800000U;
	static const uint32_t kNegOne = 0xbf800000U;
	static const uint32_t kNegZero = 0x80000000U;
	static const uint32_t kQuietNan = 0x7fc00000U;
	static const uint32_t kQuietNegNan = 0xffc00000U;
	static const uint32_t kQuietNanBit = 0x00400000U;
	static const int kSigBits = 23;
	static const uint32_t kSigMask = 0x7fffff;
	static const uint32_t kSignMask = 0x80000000U;

	static bool IsNan(uint32_t bits) {
		return (bits > kInf && bits < kNegZero) || (bits > kNegInf);
	}

	static bool IsZero(uint32_t bits) {
		return bits == 0 || bits == kNegZero;
	}

	static bool IsCanonicalNan(uint32_t bits) {
		return bits == kQuietNan || bits == kQuietNegNan;
	}

	static bool IsArithmeticNan(uint32_t bits) {
		return (bits & kQuietNan) == kQuietNan;
	}
};

bool IsCanonicalNan(uint32_t bits) {
	return FloatTraits<float>::IsCanonicalNan(bits);
}

bool IsArithmeticNan(uint32_t bits) {
	return FloatTraits<float>::IsArithmeticNan(bits);
}

template<>
bool IsConversionInRange<int32_t, float>(uint32_t bits) {
	return (bits < 0x4f000000U) || (bits >= FloatTraits<float>::kNegZero && bits <= 0xcf000000U);
}

template<>
bool IsConversionInRange<int64_t, float>(uint32_t bits) {
	return (bits < 0x5f000000U) || (bits >= FloatTraits<float>::kNegZero && bits <= 0xdf000000U);
}

template<>
bool IsConversionInRange<uint32_t, float>(uint32_t bits) {
	return (bits < 0x4f800000U) || (bits >= FloatTraits<float>::kNegZero && bits < FloatTraits<float>::kNegOne);
}

template<>
bool IsConversionInRange<uint64_t, float>(uint32_t bits) {
	return (bits < 0x5f800000U) || (bits >= FloatTraits<float>::kNegZero && bits < FloatTraits<float>::kNegOne);
}

/*
 * 6 66655555555 5544..2..222221...000
 * 3 21098765432 1098..9..432109...210
 * -----------------------------------
 * 0 00000000000 0000..0..000000...000 0x0000000000000000 => 0
 * 0 10000011101 1111..1..111000...000 0x41dfffffffc00000 => 2147483647           (INT32_MAX)
 * 0 10000011110 1111..1..111100...000 0x41efffffffe00000 => 4294967295           (UINT32_MAX)
 * 0 10000111101 1111..1..111111...111 0x43dfffffffffffff => 9223372036854774784  (~INT64_MAX)
 * 0 10000111110 0000..0..000000...000 0x43e0000000000000 => 9223372036854775808
 * 0 10000111110 1111..1..111111...111 0x43efffffffffffff => 18446744073709549568 (~UINT64_MAX)
 * 0 10000111111 0000..0..000000...000 0x43f0000000000000 => 18446744073709551616
 * 0 10001111110 1111..1..000000...000 0x47efffffe0000000 => 3.402823e+38         (FLT_MAX)
 * 0 11111111111 0000..0..000000...000 0x7ff0000000000000 => inf
 * 0 11111111111 0000..0..000000...001 0x7ff0000000000001 => nan(0x1)
 * 0 11111111111 1111..1..111111...111 0x7fffffffffffffff => nan(0xfff...)
 * 1 00000000000 0000..0..000000...000 0x8000000000000000 => -0
 * 1 01111111110 1111..1..111111...111 0xbfefffffffffffff => -1 + ulp             (~UINT32_MIN, ~UINT64_MIN)
 * 1 01111111111 0000..0..000000...000 0xbff0000000000000 => -1
 * 1 10000011110 0000..0..000000...000 0xc1e0000000000000 => -2147483648          (INT32_MIN)
 * 1 10000111110 0000..0..000000...000 0xc3e0000000000000 => -9223372036854775808 (INT64_MIN)
 * 1 10001111110 1111..1..000000...000 0xc7efffffe0000000 => -3.402823e+38        (-FLT_MAX)
 * 1 11111111111 0000..0..000000...000 0xfff0000000000000 => -inf
 * 1 11111111111 0000..0..000000...001 0xfff0000000000001 => -nan(0x1)
 * 1 11111111111 1111..1..111111...111 0xffffffffffffffff => -nan(0xfff...)
 */

template<>
struct FloatTraits<double> {
	static const uint64_t kInf = 0x7ff0000000000000ULL;
	static const uint64_t kNegInf = 0xfff0000000000000ULL;
	static const uint64_t kNegOne = 0xbff0000000000000ULL;
	static const uint64_t kNegZero = 0x8000000000000000ULL;
	static const uint64_t kQuietNan = 0x7ff8000000000000ULL;
	static const uint64_t kQuietNegNan = 0xfff8000000000000ULL;
	static const uint64_t kQuietNanBit = 0x0008000000000000ULL;
	static const int kSigBits = 52;
	static const uint64_t kSigMask = 0xfffffffffffffULL;
	static const uint64_t kSignMask = 0x8000000000000000ULL;

	static bool IsNan(uint64_t bits) {
		return (bits > kInf && bits < kNegZero) || (bits > kNegInf);
	}

	static bool IsZero(uint64_t bits) {
		return bits == 0 || bits == kNegZero;
	}

	static bool IsCanonicalNan(uint64_t bits) {
		return bits == kQuietNan || bits == kQuietNegNan;
	}

	static bool IsArithmeticNan(uint64_t bits) {
		return (bits & kQuietNan) == kQuietNan;
	}
};

bool IsCanonicalNan(uint64_t bits) {
	return FloatTraits<double>::IsCanonicalNan(bits);
}

bool IsArithmeticNan(uint64_t bits) {
	return FloatTraits<double>::IsArithmeticNan(bits);
}

template<>
bool IsConversionInRange<int32_t, double>(uint64_t bits) {
	return (bits <= 0x41dfffffffc00000ULL) || (bits >= FloatTraits<double>::kNegZero && bits <= 0xc1e0000000000000ULL);
}

template<>
bool IsConversionInRange<int64_t, double>(uint64_t bits) {
	return (bits < 0x43e0000000000000ULL) || (bits >= FloatTraits<double>::kNegZero && bits <= 0xc3e0000000000000ULL);
}

template<>
bool IsConversionInRange<uint32_t, double>(uint64_t bits) {
	return (bits <= 0x41efffffffe00000ULL) || (bits >= FloatTraits<double>::kNegZero && bits < FloatTraits<double>::kNegOne);
}

template<>
bool IsConversionInRange<uint64_t, double>(uint64_t bits) {
	return (bits < 0x43f0000000000000ULL) || (bits >= FloatTraits<double>::kNegZero && bits < FloatTraits<double>::kNegOne);
}

template<>
bool IsConversionInRange<float, double>(uint64_t bits) {
	return (bits <= 0x47efffffe0000000ULL) || (bits >= FloatTraits<double>::kNegZero && bits <= 0xc7efffffe0000000ULL);
}

// The WebAssembly rounding mode means that these values (which are > F32_MAX)
// should be rounded to F32_MAX and not set to infinity. Unfortunately, UBSAN
// complains that the value is not representable as a float, so we'll special
// case them.
bool IsInRangeF64DemoteF32RoundToF32Max(uint64_t bits) {
	return bits > 0x47efffffe0000000ULL && bits < 0x47effffff0000000ULL;
}

bool IsInRangeF64DemoteF32RoundToNegF32Max(uint64_t bits) {
	return bits > 0xc7efffffe0000000ULL && bits < 0xc7effffff0000000ULL;
}

template <typename T, typename MemType> struct ExtendMemType;
template<> struct ExtendMemType<uint32_t, uint8_t> { typedef uint32_t type; };
template<> struct ExtendMemType<uint32_t, int8_t> { typedef int32_t type; };
template<> struct ExtendMemType<uint32_t, uint16_t> { typedef uint32_t type; };
template<> struct ExtendMemType<uint32_t, int16_t> { typedef int32_t type; };
template<> struct ExtendMemType<uint32_t, uint32_t> { typedef uint32_t type; };
template<> struct ExtendMemType<uint32_t, int32_t> { typedef int32_t type; };
template<> struct ExtendMemType<uint64_t, uint8_t> { typedef uint64_t type; };
template<> struct ExtendMemType<uint64_t, int8_t> { typedef int64_t type; };
template<> struct ExtendMemType<uint64_t, uint16_t> { typedef uint64_t type; };
template<> struct ExtendMemType<uint64_t, int16_t> { typedef int64_t type; };
template<> struct ExtendMemType<uint64_t, uint32_t> { typedef uint64_t type; };
template<> struct ExtendMemType<uint64_t, int32_t> { typedef int64_t type; };
template<> struct ExtendMemType<uint64_t, uint64_t> { typedef uint64_t type; };
template<> struct ExtendMemType<uint64_t, int64_t> { typedef int64_t type; };
template<> struct ExtendMemType<float, float> { typedef float type; };
template<> struct ExtendMemType<double, double> { typedef double type; };

template <typename T, typename MemType> struct WrapMemType;
template<> struct WrapMemType<uint32_t, uint8_t> { typedef uint8_t type; };
template<> struct WrapMemType<uint32_t, uint16_t> { typedef uint16_t type; };
template<> struct WrapMemType<uint32_t, uint32_t> { typedef uint32_t type; };
template<> struct WrapMemType<uint64_t, uint8_t> { typedef uint8_t type; };
template<> struct WrapMemType<uint64_t, uint16_t> { typedef uint16_t type; };
template<> struct WrapMemType<uint64_t, uint32_t> { typedef uint32_t type; };
template<> struct WrapMemType<uint64_t, uint64_t> { typedef uint64_t type; };
template<> struct WrapMemType<float, float> { typedef uint32_t type; };
template<> struct WrapMemType<double, double> { typedef uint64_t type; };

template <typename T>
Value MakeValue(ValueTypeRep<T>);

template<>
Value MakeValue<uint32_t>(uint32_t v) {
	Value result;
	result.i32 = v;
	return result;
}

template<>
Value MakeValue<int32_t>(uint32_t v) {
	Value result;
	result.i32 = v;
	return result;
}

template<>
Value MakeValue<uint64_t>(uint64_t v) {
	Value result;
	result.i64 = v;
	return result;
}

template<>
Value MakeValue<int64_t>(uint64_t v) {
	Value result;
	result.i64 = v;
	return result;
}

template<>
Value MakeValue<float>(uint32_t v) {
	Value result;
	result.f32_bits = v;
	return result;
}

template<>
Value MakeValue<double>(uint64_t v) {
	Value result;
	result.f64_bits = v;
	return result;
}

template <typename T> ValueTypeRep<T> GetValue(Value);
template<> uint32_t GetValue<int32_t>(Value v) { return v.i32; }
template<> uint32_t GetValue<uint32_t>(Value v) { return v.i32; }
template<> uint64_t GetValue<int64_t>(Value v) { return v.i64; }
template<> uint64_t GetValue<uint64_t>(Value v) { return v.i64; }
template<> uint32_t GetValue<float>(Value v) { return v.f32_bits; }
template<> uint64_t GetValue<double>(Value v) { return v.f64_bits; }

// Differs from the normal CHECK_RESULT because this one is meant to return the
// interp Result type.
#undef CHECK_RESULT
#define CHECK_RESULT(expr)   \
  do {                       \
    if (WABT_FAILED(expr)) { \
      return Result::Error;  \
    }                        \
  } while (0)

// Differs from CHECK_RESULT since it can return different traps, not just
// Error. Also uses __VA_ARGS__ so templates can be passed without surrounding
// parentheses.
#define CHECK_TRAP(...)            \
  do {                             \
    Result result = (__VA_ARGS__); \
    if (result != Result::Ok) {    \
      return result;               \
    }                              \
  } while (0)

#define TRAP(type) return Thread::Result::Trap##type
#define TRAP_UNLESS(cond, type) TRAP_IF(!(cond), type)
#define TRAP_IF(cond, type)    \
  do {                         \
    if (WABT_UNLIKELY(cond)) { \
      TRAP(type);              \
    }                          \
  } while (0)

#define CHECK_STACK() \
  TRAP_IF(_valueStackTop >= _valueStack.size(), ValueStackExhausted)

#define PUSH_NEG_1_AND_BREAK_IF(cond) \
  if (WABT_UNLIKELY(cond)) {          \
    CHECK_TRAP(Push<int32_t>(-1));    \
    break;                            \
  }

#define GOTO(offset) pc = &istream[offset]

template<typename MemType>
Thread::Result Thread::GetAccessAddress(const Func::OpcodeRec * pc, void** out_address) {
	auto memory = _currentFrame->module->memory[pc->value32.v2];
	uint64_t addr = static_cast<uint64_t>(Pop<uint32_t>()) + pc->value32.v1;
	TRAP_IF(addr + sizeof(MemType) > memory->data.size(), MemoryAccessOutOfBounds);
	*out_address = memory->data.data() + addr;
	return Result::Ok;
}

template<typename MemType>
Thread::Result Thread::GetAtomicAccessAddress(const Func::OpcodeRec * pc, void** out_address) {
	auto memory = _currentFrame->module->memory[pc->value32.v2];
	uint64_t addr = static_cast<uint64_t>(Pop<uint32_t>()) + pc->value32.v1;
	TRAP_IF(addr + sizeof(MemType) > memory->data.size(), MemoryAccessOutOfBounds);
	TRAP_IF((addr & (sizeof(MemType) - 1)) != 0, AtomicMemoryAccessUnaligned);
	*out_address = memory->data.data() + addr;
	return Result::Ok;
}

Value& Thread::Top() {
	return Pick(1);
}

Value& Thread::Pick(Index depth) {
	return _valueStack[_valueStackTop - depth];
}

void Thread::Reset() {
	_valueStackTop = 0;
	_callStackTop = 0;
}

Thread::Result Thread::Push(Value value) {
	CHECK_STACK();
	_valueStack[_valueStackTop++] = value;
	return Result::Ok;
}

Value Thread::Pop() {
	return _valueStack[--_valueStackTop];
}

Value Thread::ValueAt(Index at) const {
	assert(at < _valueStackTop);
	return _valueStack[at];
}

template<typename T>
Thread::Result Thread::Push(T value) {
	return PushRep<T>(ToRep(value));
}

template<typename T>
T Thread::Pop() {
	return FromRep<T>(PopRep<T>());
}

template<typename T>
Thread::Result Thread::PushRep(ValueTypeRep<T> value) {
	return Push(MakeValue<T>(value));
}

template<typename T>
ValueTypeRep<T> Thread::PopRep() {
	return GetValue<T>(Pop());
}

void Thread::StoreResult(Value *begin, Index stack, Index results) {
	const auto d = _valueStack.data();
	auto resultsPtr = d + _valueStackTop - results;
	_valueStackTop = (begin - d) + stack + results;
	if (results > 0) {
		memmove(begin + stack, resultsPtr, results * sizeof(Value));
	}
}

Thread::Result Thread::PushCall(const RuntimeModule *module, const Func *func) {
	TRAP_IF(_callStackTop >= _callStack.size(), CallStackExhausted);
	_callStack[_callStackTop] = CallStackFrame{module, func, _valueStack.data() + _valueStackTop - func->types.size(), func->opcodes.data()};
	++ _callStackTop;
	return Result::Ok;
}

Thread::Result Thread::PushCall(const RuntimeModule *module, Index idx, bool import) {
	if (!import) {
		return PushCall(module, module->func[idx].first);
	} else {
		TRAP_IF(_callStackTop >= _callStack.size(), CallStackExhausted);
		auto &fn = module->func[idx];
		if (fn.first) {
			if (auto rtMod = _runtime->getModule(fn.first->module)) {
				return PushCall(rtMod, module->func[idx].first);
			}
		} else if (fn.second) {
			Index newTop = _valueStackTop - fn.second->sig.params.size() + fn.second->sig.results.size();
			TRAP_IF(newTop >= _valueStack.size(), ValueStackExhausted);
			if (fn.second->callback(this, fn.second, _valueStack.data() + _valueStackTop - fn.second->sig.params.size()) == wasm::Result::Ok) {
				_valueStackTop = newTop;
				return Result::Returned;
			}
		}
	}
	return Result::TrapHostTrapped;
}

void Thread::PopCall(Index idx) {
	const Index newTop = _currentFrame->locals - _valueStack.data() + idx;
	if (idx > 0) {
		memmove(_currentFrame->locals, _valueStack.data() + _valueStackTop - idx, idx * sizeof(Value));
	}
	_valueStackTop = newTop;
	-- _callStackTop;
}

template <typename T>
void LoadFromMemory(T* dst, const void* src) {
	memcpy(dst, src, sizeof(T));
}

template <typename T>
void StoreToMemory(void* dst, T value) {
	memcpy(dst, &value, sizeof(T));
}

template <typename MemType, typename ResultType>
Thread::Result Thread::Load(const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	static_assert(std::is_floating_point<MemType>::value == std::is_floating_point<ExtendedType>::value,
			"Extended type should be float iff MemType is float");

	void* src;
	CHECK_TRAP(GetAccessAddress<MemType>(pc, &src));
	MemType value;
	LoadFromMemory<MemType>(&value, src);
	return Push<ResultType>(static_cast<ExtendedType>(value));
}

template <typename MemType, typename ResultType>
Thread::Result Thread::Store(const Func::OpcodeRec * pc) {
	typedef typename WrapMemType<ResultType, MemType>::type WrappedType;
	WrappedType value = PopRep<ResultType>();
	void* dst;
	CHECK_TRAP(GetAccessAddress<MemType>(pc, &dst));
	StoreToMemory<WrappedType>(dst, value);
	return Result::Ok;
}

template <typename MemType, typename ResultType>
Thread::Result Thread::AtomicLoad(const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	static_assert(!std::is_floating_point<MemType>::value,
			"AtomicLoad type can't be float");
	void* src;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &src));
	MemType value;
	LoadFromMemory<MemType>(&value, src);
	return Push<ResultType>(static_cast<ExtendedType>(value));
}

template<typename MemType, typename ResultType>
Thread::Result Thread::AtomicStore(const Func::OpcodeRec * pc) {
	typedef typename WrapMemType<ResultType, MemType>::type WrappedType;
	WrappedType value = PopRep<ResultType>();
	void* dst;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &dst));
	StoreToMemory<WrappedType>(dst, value);
	return Result::Ok;
}

template<typename MemType, typename ResultType>
Thread::Result Thread::AtomicRmw(BinopFunc<ResultType, ResultType> func, const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	MemType rhs = PopRep<ResultType>();
	void* addr;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &addr));
	MemType read;
	LoadFromMemory<MemType>(&read, addr);
	StoreToMemory<MemType>(addr, func(read, rhs));
	return Push<ResultType>(static_cast<ExtendedType>(read));
}

template<typename MemType, typename ResultType>
Thread::Result Thread::AtomicRmwCmpxchg(const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	MemType replace = PopRep<ResultType>();
	MemType expect = PopRep<ResultType>();
	void* addr;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &addr));
	MemType read;
	LoadFromMemory<MemType>(&read, addr);
	if (read == expect) {
		StoreToMemory<MemType>(addr, replace);
	}
	return Push<ResultType>(static_cast<ExtendedType>(read));
}

template<typename R, typename T>
Thread::Result Thread::Unop(UnopFunc<R, T> func) {
	auto value = PopRep<T>();
	return PushRep<R>(func(value));
}

template<typename R, typename T>
Thread::Result Thread::UnopTrap(UnopTrapFunc<R, T> func) {
	auto value = PopRep<T>();
	ValueTypeRep<R> result_value;
	CHECK_TRAP(func(value, &result_value));
	return PushRep<R>(result_value);
}

template<typename R, typename T>
Thread::Result Thread::Binop(BinopFunc<R, T> func) {
	auto rhs_rep = PopRep<T>();
	auto lhs_rep = PopRep<T>();
	return PushRep<R>(func(lhs_rep, rhs_rep));
}

template<typename R, typename T>
Thread::Result Thread::BinopTrap(BinopTrapFunc<R, T> func) {
	auto rhs_rep = PopRep<T>();
	auto lhs_rep = PopRep<T>();
	ValueTypeRep<R> result_value;
	CHECK_TRAP(func(lhs_rep, rhs_rep, &result_value));
	return PushRep<R>(result_value);
}

// {i,f}{32,64}.add
template<typename T>
ValueTypeRep<T> Add(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) + FromRep<T>(rhs_rep));
}

// {i,f}{32,64}.sub
template<typename T>
ValueTypeRep<T> Sub(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) - FromRep<T>(rhs_rep));
}

// {i,f}{32,64}.mul
template<typename T>
ValueTypeRep<T> Mul(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) * FromRep<T>(rhs_rep));
}

// i{32,64}.{div,rem}_s are special-cased because they trap when dividing the
// max signed value by -1. The modulo operation on x86 uses the same
// instruction to generate the quotient and the remainder.
template<typename T>
bool IsNormalDivRemS(T lhs, T rhs) {
	static_assert(std::is_signed<T>::value, "T should be a signed type.");
	return !(lhs == std::numeric_limits<T>::min() && rhs == -1);
}

// i{32,64}.div_s
template<typename T>
Thread::Result IntDivS(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep, ValueTypeRep<T>* out_result) {
	auto lhs = FromRep<T>(lhs_rep);
	auto rhs = FromRep<T>(rhs_rep);
	TRAP_IF(rhs == 0, IntegerDivideByZero);
	TRAP_UNLESS(IsNormalDivRemS(lhs, rhs), IntegerOverflow);
	*out_result = ToRep(lhs / rhs);
	return Thread::Result::Ok;
}

// i{32,64}.rem_s
template<typename T>
Thread::Result IntRemS(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep, ValueTypeRep<T>* out_result) {
	auto lhs = FromRep<T>(lhs_rep);
	auto rhs = FromRep<T>(rhs_rep);
	TRAP_IF(rhs == 0, IntegerDivideByZero);
	if (WABT_LIKELY(IsNormalDivRemS(lhs, rhs))) {
		*out_result = ToRep(lhs % rhs);
	} else {
		*out_result = 0;
	}
	return Thread::Result::Ok;
}

// i{32,64}.div_u
template<typename T>
Thread::Result IntDivU(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep, ValueTypeRep<T>* out_result) {
	auto lhs = FromRep<T>(lhs_rep);
	auto rhs = FromRep<T>(rhs_rep);
	TRAP_IF(rhs == 0, IntegerDivideByZero);
	*out_result = ToRep(lhs / rhs);
	return Thread::Result::Ok;
}

// i{32,64}.rem_u
template<typename T>
Thread::Result IntRemU(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep, ValueTypeRep<T>* out_result) {
	auto lhs = FromRep<T>(lhs_rep);
	auto rhs = FromRep<T>(rhs_rep);
	TRAP_IF(rhs == 0, IntegerDivideByZero);
	*out_result = ToRep(lhs % rhs);
	return Thread::Result::Ok;
}

// f{32,64}.div
template<typename T>
ValueTypeRep<T> FloatDiv(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	typedef FloatTraits<T> Traits;
	ValueTypeRep<T> result;
	if (WABT_UNLIKELY(Traits::IsZero(rhs_rep))) {
		if (Traits::IsNan(lhs_rep)) {
			result = lhs_rep | Traits::kQuietNan;
		} else if (Traits::IsZero(lhs_rep)) {
			result = Traits::kQuietNan;
		} else {
			auto sign = (lhs_rep & Traits::kSignMask) ^ (rhs_rep & Traits::kSignMask);
			result = sign | Traits::kInf;
		}
	} else {
		result = ToRep(FromRep<T>(lhs_rep) / FromRep<T>(rhs_rep));
	}
	return result;
}

// i{32,64}.and
template<typename T>
ValueTypeRep<T> IntAnd(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) & FromRep<T>(rhs_rep));
}

// i{32,64}.or
template<typename T>
ValueTypeRep<T> IntOr(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) | FromRep<T>(rhs_rep));
}

// i{32,64}.xor
template<typename T>
ValueTypeRep<T> IntXor(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) ^ FromRep<T>(rhs_rep));
}

// i{32,64}.shl
template<typename T>
ValueTypeRep<T> IntShl(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	const int mask = sizeof(T) * 8 - 1;
	return ToRep(FromRep<T>(lhs_rep) << (FromRep<T>(rhs_rep) & mask));
}

// i{32,64}.shr_{s,u}
template<typename T>
ValueTypeRep<T> IntShr(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	const int mask = sizeof(T) * 8 - 1;
	return ToRep(FromRep<T>(lhs_rep) >> (FromRep<T>(rhs_rep) & mask));
}

// i{32,64}.rotl
template<typename T>
ValueTypeRep<T> IntRotl(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	const int mask = sizeof(T) * 8 - 1;
	int amount = FromRep<T>(rhs_rep) & mask;
	auto lhs = FromRep<T>(lhs_rep);
	if (amount == 0) {
		return ToRep(lhs);
	} else {
		return ToRep((lhs << amount) | (lhs >> (mask + 1 - amount)));
	}
}

// i{32,64}.rotr
template<typename T>
ValueTypeRep<T> IntRotr(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	const int mask = sizeof(T) * 8 - 1;
	int amount = FromRep<T>(rhs_rep) & mask;
	auto lhs = FromRep<T>(lhs_rep);
	if (amount == 0) {
		return ToRep(lhs);
	} else {
		return ToRep((lhs >> amount) | (lhs << (mask + 1 - amount)));
	}
}

// i{32,64}.eqz
template<typename R, typename T>
ValueTypeRep<R> IntEqz(ValueTypeRep<T> v_rep) {
	return ToRep(v_rep == 0);
}

// f{32,64}.abs
template<typename T>
ValueTypeRep<T> FloatAbs(ValueTypeRep<T> v_rep) {
	return v_rep & ~FloatTraits<T>::kSignMask;
}

// f{32,64}.neg
template<typename T>
ValueTypeRep<T> FloatNeg(ValueTypeRep<T> v_rep) {
	return v_rep ^ FloatTraits<T>::kSignMask;
}

// f{32,64}.ceil
template<typename T>
ValueTypeRep<T> FloatCeil(ValueTypeRep<T> v_rep) {
	auto result = ToRep(std::ceil(FromRep<T>(v_rep)));
	if (WABT_UNLIKELY(FloatTraits<T>::IsNan(result))) {
		result |= FloatTraits<T>::kQuietNanBit;
	}
	return result;
}

// f{32,64}.floor
template<typename T>
ValueTypeRep<T> FloatFloor(ValueTypeRep<T> v_rep) {
	auto result = ToRep(std::floor(FromRep<T>(v_rep)));
	if (WABT_UNLIKELY(FloatTraits<T>::IsNan(result))) {
		result |= FloatTraits<T>::kQuietNanBit;
	}
	return result;
}

// f{32,64}.trunc
template<typename T>
ValueTypeRep<T> FloatTrunc(ValueTypeRep<T> v_rep) {
	auto result = ToRep(std::trunc(FromRep<T>(v_rep)));
	if (WABT_UNLIKELY(FloatTraits<T>::IsNan(result))) {
		result |= FloatTraits<T>::kQuietNanBit;
	}
	return result;
}

// f{32,64}.nearest
template<typename T>
ValueTypeRep<T> FloatNearest(ValueTypeRep<T> v_rep) {
	auto result = ToRep(std::nearbyint(FromRep<T>(v_rep)));
	if (WABT_UNLIKELY(FloatTraits<T>::IsNan(result))) {
		result |= FloatTraits<T>::kQuietNanBit;
	}
	return result;
}

// f{32,64}.sqrt
template<typename T>
ValueTypeRep<T> FloatSqrt(ValueTypeRep<T> v_rep) {
	auto result = ToRep(std::sqrt(FromRep<T>(v_rep)));
	if (WABT_UNLIKELY(FloatTraits<T>::IsNan(result))) {
		result |= FloatTraits<T>::kQuietNanBit;
	}
	return result;
}

// f{32,64}.min
template<typename T>
ValueTypeRep<T> FloatMin(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	typedef FloatTraits<T> Traits;

	if (WABT_UNLIKELY(Traits::IsNan(lhs_rep))) {
		return lhs_rep | Traits::kQuietNanBit;
	} else if (WABT_UNLIKELY(Traits::IsNan(rhs_rep))) {
		return rhs_rep | Traits::kQuietNanBit;
	} else if (WABT_UNLIKELY(
			Traits::IsZero(lhs_rep) && Traits::IsZero(rhs_rep))) {
		// min(0.0, -0.0) == -0.0, but std::min won't produce the correct result.
		// We can instead compare using the unsigned integer representation, but
		// just max instead (since the sign bit makes the value larger).
		return std::max(lhs_rep, rhs_rep);
	} else {
		return ToRep(std::min(FromRep<T>(lhs_rep), FromRep<T>(rhs_rep)));
	}
}

// f{32,64}.max
template<typename T>
ValueTypeRep<T> FloatMax(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	typedef FloatTraits<T> Traits;

	if (WABT_UNLIKELY(Traits::IsNan(lhs_rep))) {
		return lhs_rep | Traits::kQuietNanBit;
	} else if (WABT_UNLIKELY(Traits::IsNan(rhs_rep))) {
		return rhs_rep | Traits::kQuietNanBit;
	} else if (WABT_UNLIKELY(
			Traits::IsZero(lhs_rep) && Traits::IsZero(rhs_rep))) {
		// min(0.0, -0.0) == -0.0, but std::min won't produce the correct result.
		// We can instead compare using the unsigned integer representation, but
		// just max instead (since the sign bit makes the value larger).
		return std::min(lhs_rep, rhs_rep);
	} else {
		return ToRep(std::max(FromRep<T>(lhs_rep), FromRep<T>(rhs_rep)));
	}
}

// f{32,64}.copysign
template<typename T>
ValueTypeRep<T> FloatCopySign(ValueTypeRep<T> lhs_rep,
		ValueTypeRep<T> rhs_rep) {
	typedef FloatTraits<T> Traits;
	return (lhs_rep & ~Traits::kSignMask) | (rhs_rep & Traits::kSignMask);
}

// {i,f}{32,64}.eq
template<typename T>
uint32_t Eq(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) == FromRep<T>(rhs_rep));
}

// {i,f}{32,64}.ne
template<typename T>
uint32_t Ne(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) != FromRep<T>(rhs_rep));
}

// f{32,64}.lt | i{32,64}.lt_{s,u}
template<typename T>
uint32_t Lt(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) < FromRep<T>(rhs_rep));
}

// f{32,64}.le | i{32,64}.le_{s,u}
template<typename T>
uint32_t Le(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) <= FromRep<T>(rhs_rep));
}

// f{32,64}.gt | i{32,64}.gt_{s,u}
template<typename T>
uint32_t Gt(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) > FromRep<T>(rhs_rep));
}

// f{32,64}.ge | i{32,64}.ge_{s,u}
template<typename T>
uint32_t Ge(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return ToRep(FromRep<T>(lhs_rep) >= FromRep<T>(rhs_rep));
}

// i{32,64}.trunc_{s,u}/f{32,64}
template<typename R, typename T>
Thread::Result IntTrunc(ValueTypeRep<T> v_rep, ValueTypeRep<R>* out_result) {
	TRAP_IF(FloatTraits<T>::IsNan(v_rep), InvalidConversionToInteger);
	TRAP_UNLESS((IsConversionInRange<R, T>(v_rep)), IntegerOverflow);
	*out_result = ToRep(static_cast<R>(FromRep<T>(v_rep)));
	return Thread::Result::Ok;
}

// i{32,64}.trunc_{s,u}:sat/f{32,64}
template<typename R, typename T>
ValueTypeRep<R> IntTruncSat(ValueTypeRep<T> v_rep) {
	typedef FloatTraits<T> Traits;
	if (WABT_UNLIKELY(Traits::IsNan(v_rep))) {
		return 0;
	} else if (WABT_UNLIKELY((!IsConversionInRange<R, T>(v_rep)))) {
		if (v_rep & Traits::kSignMask) {
			return ToRep(std::numeric_limits<R>::min());
		} else {
			return ToRep(std::numeric_limits<R>::max());
		}
	} else {
		return ToRep(static_cast<R>(FromRep<T>(v_rep)));
	}
}

// i{32,64}.extend{8,16,32}_s
template<typename T, typename E>
ValueTypeRep<T> IntExtendS(ValueTypeRep<T> v_rep) {
	// To avoid undefined/implementation-defined behavior, convert from unsigned
	// type (T), to an unsigned value of the smaller size (EU), then bitcast from
	// unsigned to signed, then cast from the smaller signed type to the larger
	// signed type (TS) to sign extend. ToRep then will bitcast back from signed
	// to unsigned.
	static_assert(std::is_unsigned<ValueTypeRep<T>>::value, "T must be unsigned");
	static_assert(std::is_signed<E>::value, "E must be signed");
	typedef typename std::make_unsigned<E>::type EU;
	typedef typename std::make_signed<T>::type TS;
	return ToRep(static_cast<TS>(Bitcast<E>(static_cast<EU>(v_rep))));
}

// i{32,64}.atomic.rmw(8,16,32}_u.xchg
template <typename T>
ValueTypeRep<T> Xchg(ValueTypeRep<T> lhs_rep, ValueTypeRep<T> rhs_rep) {
	return rhs_rep;
}


Thread::Thread(const Runtime *runtime, Index tag) : _runtime(runtime), _tag(tag) { }

bool Thread::init(uint32_t valueStackSize, uint32_t callStackSize) {
	_callStackTop = 0;
	_valueStackTop = 0;

	_valueStack.resize(valueStackSize);
	_callStack.resize(callStackSize);

	return true;
}

const uint8_t* GetIstream() {
	return nullptr;
}

Opcode ReadOpcode(const uint8_t **) {
	return Opcode::Nop;
}

inline uint8_t ReadU8At(const uint8_t* pc) {
	return 0;
}

inline uint8_t ReadU8(const uint8_t** pc) {
	return 0;
}

inline uint32_t ReadU32At(const uint8_t* pc) {
	return 0;
}

inline uint32_t ReadU32(const uint8_t** pc) {
	return 0;
}

inline uint64_t ReadU64At(const uint8_t* pc) {
	return 0;
}

inline uint64_t ReadU64(const uint8_t** pc) {
	return 0;
}

inline void read_table_entry_at(const uint8_t* pc, Offset* out_offset, uint32_t* out_drop, uint8_t* out_keep) {
	*out_offset = ReadU32At(pc + WABT_TABLE_ENTRY_OFFSET_OFFSET);
	*out_drop = ReadU32At(pc + WABT_TABLE_ENTRY_DROP_OFFSET);
	*out_keep = ReadU8At(pc + WABT_TABLE_ENTRY_KEEP_OFFSET);
}

Thread::Result Thread::Run(const RuntimeModule *module, const Func *func, Value *buffer) {
	auto origStack = _callStackTop;
	auto origValue = _valueStackTop;
	CHECK_TRAP(PushLocals(func, buffer));
	CHECK_TRAP(PushCall(module, func));
	auto res = Run(origStack);
	if (res == Result::Ok || res == Result::Returned) {
		Index nresults = func->sig->results.size();
		memcpy(buffer, &_valueStack[_valueStackTop - nresults], nresults * sizeof(Value));
		_valueStackTop -= nresults;
	} else {
		_runtime->onThreadError(*this);
		_callStackTop = origStack;
		_valueStackTop = origValue;
	}
	return res;
}

uint8_t *Thread::GetMemory(Index memIndex, Index offset) const {
	if (_currentFrame && _currentFrame->module->memory.size() < memIndex) {
		auto mem = _currentFrame->module->memory[memIndex];
		if (mem->data.size() < offset) {
			return &mem->data[offset];
		}
	}
	return nullptr;
}

Thread::Result Thread::PushLocals(const Func *func, const Value *buffer, Index storeParams) {
	const Index paramsSpace = func->types.size() - storeParams;
	const Index extraStackSpace = std::max(paramsSpace, Index(func->sig->results.size()));
	const Index nParams = func->sig->params.size();
	const Index nLocals = func->types.size();

	if (_valueStackTop + extraStackSpace > _valueStack.size()) {
		return Result::TrapValueStackExhausted;
	}

	if (buffer) {
		auto ptr = _valueStack.data() + _valueStackTop;
		memcpy(ptr, buffer, (func->sig->params.size() - storeParams) * sizeof(Value));
	}
	memset(_valueStack.data() + _valueStackTop + nParams, 0, sizeof(Value) * (nLocals - nParams));

	_valueStackTop += paramsSpace;
	return Result::Ok;
}

Thread::Result Thread::Run(Index stackMax) {
	Result result = Result::Ok;
	while (_callStackTop > stackMax) {
		_currentFrame = &_callStack[_callStackTop - 1];
		const auto func = _currentFrame->func;
		const auto module = _currentFrame->module;
		auto locals = _currentFrame->locals;

		auto data = func->opcodes.data();
		auto &it = _currentFrame->position;
		auto end = data + func->opcodes.size();
		while (it != end) {
			const auto opcode = it->opcode;
			switch (opcode) {
			case Opcode::Select: {
				uint32_t cond = Pop<uint32_t>();
				Value false_ = Pop();
				Value true_ = Pop();
				CHECK_TRAP(Push(cond ? true_ : false_));
				break;
			}

			case Opcode::Br:
			case Opcode::Else: {
				it = data + it->value32.v2;
				continue;
				break;
			}

			case Opcode::BrIf: {
				if (Pop<uint32_t>()) {
					it = data + it->value32.v2;
					continue;
				}
				break;
			}

			case Opcode::BrTable: {
				const Index num_targets = it->value32.v1;
				const uint32_t key = Pop<uint32_t>();
				auto target = it + ((key >= num_targets ? num_targets : key) + 1);
				it = data + target->value32.v2;
				continue;
				break;
			}

			case Opcode::If:
				if (!Pop<uint32_t>()) {
					it = data + it->value32.v2;
					continue;
				}
				break;

			case Opcode::End:
				StoreResult(locals + func->types.size(), it->value32.v1, it->value32.v2);
				break;

			case Opcode::Return:
				if (_callStackTop <= stackMax) {
					result = Result::Returned;
				}
				PopCall(it->value32.v1);
				it = end;
				goto exit_opcode_loop;
				break;

			case Opcode::Unreachable:
				TRAP(Unreachable);
				break;

			case Opcode::I32Const:
				CHECK_TRAP(Push<uint32_t>(it->value32.v1));
				break;

			case Opcode::I64Const:
				CHECK_TRAP(Push<uint64_t>(it->value64));
				break;

			case Opcode::F32Const:
				CHECK_TRAP(PushRep<float>(it->value32.v1));
				break;

			case Opcode::F64Const:
				CHECK_TRAP(PushRep<double>(it->value64));
				break;

			case Opcode::GetGlobal:
				CHECK_TRAP(Push(module->globals[it->value32.v1]->value.value));
				break;

			case Opcode::SetGlobal:
				module->globals[it->value32.v1]->value.value = Pop();
				break;

			case Opcode::GetLocal:
				CHECK_TRAP(Push(locals[it->value32.v1]));
				break;

			case Opcode::SetLocal:
				locals[it->value32.v1] = Pop();
				break;

			case Opcode::TeeLocal:
				locals[it->value32.v1] = Top();
				break;

			case Opcode::Call: {
				auto result = PushCall(module, it->value32.v1, it->value32.v2);
				switch (result) {
				case Result::Ok: ++ it; goto exit_opcode_loop; break;
				case Result::Returned: break;
				default: return result; break;
				}
				break;
			}

			case Opcode::CallIndirect: {
				RuntimeTable* table = module->tables[it->value32.v2];
				auto reqSig = module->module->getSignature(it->value32.v1);
				Index entry_index = Pop<uint32_t>();
				TRAP_IF(entry_index >= table->values.size(), UndefinedTableIndex);
				Index func_index = table->values[entry_index].i32;
				TRAP_IF(func_index == kInvalidIndex, UninitializedTableElement);
				auto sig = module->module->getFuncSignature(func_index);
				TRAP_IF(sig.first == nullptr, IndirectCallSignatureMismatch);
				TRAP_UNLESS(_runtime->isSignatureMatch(*sig.first, *reqSig), IndirectCallSignatureMismatch);
				auto result = PushCall(module, func_index, sig.second);
				switch (result) {
				case Result::Ok: ++ it; goto exit_opcode_loop; break;
				case Result::Returned: break;
				default: return result; break;
				}
				break;
			}

			case Opcode::I32Load8S:
				CHECK_TRAP(Load<int8_t, uint32_t>(it));
				break;

			case Opcode::I32Load8U:
				CHECK_TRAP(Load<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32Load16S:
				CHECK_TRAP(Load<int16_t, uint32_t>(it));
				break;

			case Opcode::I32Load16U:
				CHECK_TRAP(Load<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64Load8S:
				CHECK_TRAP(Load<int8_t, uint64_t>(it));
				break;

			case Opcode::I64Load8U:
				CHECK_TRAP(Load<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64Load16S:
				CHECK_TRAP(Load<int16_t, uint64_t>(it));
				break;

			case Opcode::I64Load16U:
				CHECK_TRAP(Load<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64Load32S:
				CHECK_TRAP(Load<int32_t, uint64_t>(it));
				break;

			case Opcode::I64Load32U:
				CHECK_TRAP(Load<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32Load:
				CHECK_TRAP(Load<uint32_t>(it));
				break;

			case Opcode::I64Load:
				CHECK_TRAP(Load<uint64_t>(it));
				break;

			case Opcode::F32Load:
				CHECK_TRAP(Load<float>(it));
				break;

			case Opcode::F64Load:
				CHECK_TRAP(Load<double>(it));
				break;

			case Opcode::I32Store8:
				CHECK_TRAP(Store<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32Store16:
				CHECK_TRAP(Store<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64Store8:
				CHECK_TRAP(Store<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64Store16:
				CHECK_TRAP(Store<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64Store32:
				CHECK_TRAP(Store<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32Store:
				CHECK_TRAP(Store<uint32_t>(it));
				break;

			case Opcode::I64Store:
				CHECK_TRAP(Store<uint64_t>(it));
				break;

			case Opcode::F32Store:
				CHECK_TRAP(Store<float>(it));
				break;

			case Opcode::F64Store:
				CHECK_TRAP(Store<double>(it));
				break;

			case Opcode::I32AtomicLoad8U:
				CHECK_TRAP(AtomicLoad<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32AtomicLoad16U:
				CHECK_TRAP(AtomicLoad<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicLoad8U:
				CHECK_TRAP(AtomicLoad<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicLoad16U:
				CHECK_TRAP(AtomicLoad<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicLoad32U:
				CHECK_TRAP(AtomicLoad<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32AtomicLoad:
				CHECK_TRAP(AtomicLoad<uint32_t>(it));
				break;

			case Opcode::I64AtomicLoad:
				CHECK_TRAP(AtomicLoad<uint64_t>(it));
				break;

			case Opcode::I32AtomicStore8:
				CHECK_TRAP(AtomicStore<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32AtomicStore16:
				CHECK_TRAP(AtomicStore<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicStore8:
				CHECK_TRAP(AtomicStore<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicStore16:
				CHECK_TRAP(AtomicStore<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicStore32:
				CHECK_TRAP(AtomicStore<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32AtomicStore:
				CHECK_TRAP(AtomicStore<uint32_t>(it));
				break;

			case Opcode::I64AtomicStore:
				CHECK_TRAP(AtomicStore<uint64_t>(it));
				break;

#define ATOMIC_RMW(rmwop, func)                                     \
			case Opcode::I32AtomicRmw##rmwop:                                 \
				CHECK_TRAP(AtomicRmw<uint32_t, uint32_t>(func<uint32_t>, it)); \
				break;                                                          \
			case Opcode::I64AtomicRmw##rmwop:                                 \
				CHECK_TRAP(AtomicRmw<uint64_t, uint64_t>(func<uint64_t>, it)); \
				break;                                                          \
			case Opcode::I32AtomicRmw8U##rmwop:                               \
				CHECK_TRAP(AtomicRmw<uint8_t, uint32_t>(func<uint32_t>, it));  \
				break;                                                          \
			case Opcode::I32AtomicRmw16U##rmwop:                              \
				CHECK_TRAP(AtomicRmw<uint16_t, uint32_t>(func<uint32_t>, it)); \
				break;                                                          \
			case Opcode::I64AtomicRmw8U##rmwop:                               \
				CHECK_TRAP(AtomicRmw<uint8_t, uint64_t>(func<uint64_t>, it));  \
				break;                                                          \
			case Opcode::I64AtomicRmw16U##rmwop:                              \
				CHECK_TRAP(AtomicRmw<uint16_t, uint64_t>(func<uint64_t>, it)); \
				break;                                                          \
			case Opcode::I64AtomicRmw32U##rmwop:                              \
				CHECK_TRAP(AtomicRmw<uint32_t, uint64_t>(func<uint64_t>, it)); \
				break /* no semicolon */

			ATOMIC_RMW(Add, Add);
			ATOMIC_RMW(Sub, Sub);
			ATOMIC_RMW(And, IntAnd);
			ATOMIC_RMW(Or, IntOr);
			ATOMIC_RMW(Xor, IntXor);
			ATOMIC_RMW(Xchg, Xchg);

#undef ATOMIC_RMW

			case Opcode::I32AtomicRmwCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint32_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicRmwCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint64_t, uint64_t>(it));
				break;

			case Opcode::I32AtomicRmw8UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32AtomicRmw16UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicRmw8UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicRmw16UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicRmw32UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint32_t, uint64_t>(it));
				break;

			case Opcode::CurrentMemory:
				CHECK_TRAP(Push<uint32_t>(module->memory[it->value32.v1]->limits.initial));
				break;

			case Opcode::GrowMemory: {
				RuntimeMemory* memory = module->memory[it->value32.v1];
				uint32_t old_page_size = memory->limits.initial;
				uint32_t grow_pages = Pop<uint32_t>();
				uint32_t new_page_size = old_page_size + grow_pages;
				uint32_t max_page_size = memory->limits.has_max ? memory->limits.max : WABT_MAX_PAGES;
				PUSH_NEG_1_AND_BREAK_IF(new_page_size > max_page_size);
				PUSH_NEG_1_AND_BREAK_IF(static_cast<uint64_t>(new_page_size) * WABT_PAGE_SIZE > UINT32_MAX);
				memory->data.resize(new_page_size * WABT_PAGE_SIZE);
				memory->limits.initial = new_page_size;
				CHECK_TRAP(Push<uint32_t>(old_page_size));
				break;
			}

			case Opcode::I32Add:
				CHECK_TRAP(Binop(Add<uint32_t>));
				break;

			case Opcode::I32Sub:
				CHECK_TRAP(Binop(Sub<uint32_t>));
				break;

			case Opcode::I32Mul:
				CHECK_TRAP(Binop(Mul<uint32_t>));
				break;

			case Opcode::I32DivS:
				CHECK_TRAP(BinopTrap(IntDivS<int32_t>));
				break;

			case Opcode::I32DivU:
				CHECK_TRAP(BinopTrap(IntDivU<uint32_t>));
				break;

			case Opcode::I32RemS:
				CHECK_TRAP(BinopTrap(IntRemS<int32_t>));
				break;

			case Opcode::I32RemU:
				CHECK_TRAP(BinopTrap(IntRemU<uint32_t>));
				break;

			case Opcode::I32And:
				CHECK_TRAP(Binop(IntAnd<uint32_t>));
				break;

			case Opcode::I32Or:
				CHECK_TRAP(Binop(IntOr<uint32_t>));
				break;

			case Opcode::I32Xor:
				CHECK_TRAP(Binop(IntXor<uint32_t>));
				break;

			case Opcode::I32Shl:
				CHECK_TRAP(Binop(IntShl<uint32_t>));
				break;

			case Opcode::I32ShrU:
				CHECK_TRAP(Binop(IntShr<uint32_t>));
				break;

			case Opcode::I32ShrS:
				CHECK_TRAP(Binop(IntShr<int32_t>));
				break;

			case Opcode::I32Eq:
				CHECK_TRAP(Binop(Eq<uint32_t>));
				break;

			case Opcode::I32Ne:
				CHECK_TRAP(Binop(Ne<uint32_t>));
				break;

			case Opcode::I32LtS:
				CHECK_TRAP(Binop(Lt<int32_t>));
				break;

			case Opcode::I32LeS:
				CHECK_TRAP(Binop(Le<int32_t>));
				break;

			case Opcode::I32LtU:
				CHECK_TRAP(Binop(Lt<uint32_t>));
				break;

			case Opcode::I32LeU:
				CHECK_TRAP(Binop(Le<uint32_t>));
				break;

			case Opcode::I32GtS:
				CHECK_TRAP(Binop(Gt<int32_t>));
				break;

			case Opcode::I32GeS:
				CHECK_TRAP(Binop(Ge<int32_t>));
				break;

			case Opcode::I32GtU:
				CHECK_TRAP(Binop(Gt<uint32_t>));
				break;

			case Opcode::I32GeU:
				CHECK_TRAP(Binop(Ge<uint32_t>));
				break;

			case Opcode::I32Clz:
				CHECK_TRAP(Push<uint32_t>(Clz(Pop<uint32_t>())));
				break;

			case Opcode::I32Ctz:
				CHECK_TRAP(Push<uint32_t>(Ctz(Pop<uint32_t>())));
				break;

			case Opcode::I32Popcnt:
				CHECK_TRAP(Push<uint32_t>(Popcount(Pop<uint32_t>())));
				break;

			case Opcode::I32Eqz:
				CHECK_TRAP(Unop(IntEqz<uint32_t, uint32_t>));
				break;

			case Opcode::I64Add:
				CHECK_TRAP(Binop(Add<uint64_t>));
				break;

			case Opcode::I64Sub:
				CHECK_TRAP(Binop(Sub<uint64_t>));
				break;

			case Opcode::I64Mul:
				CHECK_TRAP(Binop(Mul<uint64_t>));
				break;

			case Opcode::I64DivS:
				CHECK_TRAP(BinopTrap(IntDivS<int64_t>));
				break;

			case Opcode::I64DivU:
				CHECK_TRAP(BinopTrap(IntDivU<uint64_t>));
				break;

			case Opcode::I64RemS:
				CHECK_TRAP(BinopTrap(IntRemS<int64_t>));
				break;

			case Opcode::I64RemU:
				CHECK_TRAP(BinopTrap(IntRemU<uint64_t>));
				break;

			case Opcode::I64And:
				CHECK_TRAP(Binop(IntAnd<uint64_t>));
				break;

			case Opcode::I64Or:
				CHECK_TRAP(Binop(IntOr<uint64_t>));
				break;

			case Opcode::I64Xor:
				CHECK_TRAP(Binop(IntXor<uint64_t>));
				break;

			case Opcode::I64Shl:
				CHECK_TRAP(Binop(IntShl<uint64_t>));
				break;

			case Opcode::I64ShrU:
				CHECK_TRAP(Binop(IntShr<uint64_t>));
				break;

			case Opcode::I64ShrS:
				CHECK_TRAP(Binop(IntShr<int64_t>));
				break;

			case Opcode::I64Eq:
				CHECK_TRAP(Binop(Eq<uint64_t>));
				break;

			case Opcode::I64Ne:
				CHECK_TRAP(Binop(Ne<uint64_t>));
				break;

			case Opcode::I64LtS:
				CHECK_TRAP(Binop(Lt<int64_t>));
				break;

			case Opcode::I64LeS:
				CHECK_TRAP(Binop(Le<int64_t>));
				break;

			case Opcode::I64LtU:
				CHECK_TRAP(Binop(Lt<uint64_t>));
				break;

			case Opcode::I64LeU:
				CHECK_TRAP(Binop(Le<uint64_t>));
				break;

			case Opcode::I64GtS:
				CHECK_TRAP(Binop(Gt<int64_t>));
				break;

			case Opcode::I64GeS:
				CHECK_TRAP(Binop(Ge<int64_t>));
				break;

			case Opcode::I64GtU:
				CHECK_TRAP(Binop(Gt<uint64_t>));
				break;

			case Opcode::I64GeU:
				CHECK_TRAP(Binop(Ge<uint64_t>));
				break;

			case Opcode::I64Clz:
				CHECK_TRAP(Push<uint64_t>(Clz(Pop<uint64_t>())));
				break;

			case Opcode::I64Ctz:
				CHECK_TRAP(Push<uint64_t>(Ctz(Pop<uint64_t>())));
				break;

			case Opcode::I64Popcnt:
				CHECK_TRAP(Push<uint64_t>(Popcount(Pop<uint64_t>())));
				break;

			case Opcode::F32Add:
				CHECK_TRAP(Binop(Add<float>));
				break;

			case Opcode::F32Sub:
				CHECK_TRAP(Binop(Sub<float>));
				break;

			case Opcode::F32Mul:
				CHECK_TRAP(Binop(Mul<float>));
				break;

			case Opcode::F32Div:
				CHECK_TRAP(Binop(FloatDiv<float>));
				break;

			case Opcode::F32Min:
				CHECK_TRAP(Binop(FloatMin<float>));
				break;

			case Opcode::F32Max:
				CHECK_TRAP(Binop(FloatMax<float>));
				break;

			case Opcode::F32Abs:
				CHECK_TRAP(Unop(FloatAbs<float>));
				break;

			case Opcode::F32Neg:
				CHECK_TRAP(Unop(FloatNeg<float>));
				break;

			case Opcode::F32Copysign:
				CHECK_TRAP(Binop(FloatCopySign<float>));
				break;

			case Opcode::F32Ceil:
				CHECK_TRAP(Unop(FloatCeil<float>));
				break;

			case Opcode::F32Floor:
				CHECK_TRAP(Unop(FloatFloor<float>));
				break;

			case Opcode::F32Trunc:
				CHECK_TRAP(Unop(FloatTrunc<float>));
				break;

			case Opcode::F32Nearest:
				CHECK_TRAP(Unop(FloatNearest<float>));
				break;

			case Opcode::F32Sqrt:
				CHECK_TRAP(Unop(FloatSqrt<float>));
				break;

			case Opcode::F32Eq:
				CHECK_TRAP(Binop(Eq<float>));
				break;

			case Opcode::F32Ne:
				CHECK_TRAP(Binop(Ne<float>));
				break;

			case Opcode::F32Lt:
				CHECK_TRAP(Binop(Lt<float>));
				break;

			case Opcode::F32Le:
				CHECK_TRAP(Binop(Le<float>));
				break;

			case Opcode::F32Gt:
				CHECK_TRAP(Binop(Gt<float>));
				break;

			case Opcode::F32Ge:
				CHECK_TRAP(Binop(Ge<float>));
				break;

			case Opcode::F64Add:
				CHECK_TRAP(Binop(Add<double>));
				break;

			case Opcode::F64Sub:
				CHECK_TRAP(Binop(Sub<double>));
				break;

			case Opcode::F64Mul:
				CHECK_TRAP(Binop(Mul<double>));
				break;

			case Opcode::F64Div:
				CHECK_TRAP(Binop(FloatDiv<double>));
				break;

			case Opcode::F64Min:
				CHECK_TRAP(Binop(FloatMin<double>));
				break;

			case Opcode::F64Max:
				CHECK_TRAP(Binop(FloatMax<double>));
				break;

			case Opcode::F64Abs:
				CHECK_TRAP(Unop(FloatAbs<double>));
				break;

			case Opcode::F64Neg:
				CHECK_TRAP(Unop(FloatNeg<double>));
				break;

			case Opcode::F64Copysign:
				CHECK_TRAP(Binop(FloatCopySign<double>));
				break;

			case Opcode::F64Ceil:
				CHECK_TRAP(Unop(FloatCeil<double>));
				break;

			case Opcode::F64Floor:
				CHECK_TRAP(Unop(FloatFloor<double>));
				break;

			case Opcode::F64Trunc:
				CHECK_TRAP(Unop(FloatTrunc<double>));
				break;

			case Opcode::F64Nearest:
				CHECK_TRAP(Unop(FloatNearest<double>));
				break;

			case Opcode::F64Sqrt:
				CHECK_TRAP(Unop(FloatSqrt<double>));
				break;

			case Opcode::F64Eq:
				CHECK_TRAP(Binop(Eq<double>));
				break;

			case Opcode::F64Ne:
				CHECK_TRAP(Binop(Ne<double>));
				break;

			case Opcode::F64Lt:
				CHECK_TRAP(Binop(Lt<double>));
				break;

			case Opcode::F64Le:
				CHECK_TRAP(Binop(Le<double>));
				break;

			case Opcode::F64Gt:
				CHECK_TRAP(Binop(Gt<double>));
				break;

			case Opcode::F64Ge:
				CHECK_TRAP(Binop(Ge<double>));
				break;

			case Opcode::I32TruncSF32:
				CHECK_TRAP(UnopTrap(IntTrunc<int32_t, float>));
				break;

			case Opcode::I32TruncSSatF32:
				CHECK_TRAP(Unop(IntTruncSat<int32_t, float>));
				break;

			case Opcode::I32TruncSF64:
				CHECK_TRAP(UnopTrap(IntTrunc<int32_t, double>));
				break;

			case Opcode::I32TruncSSatF64:
				CHECK_TRAP(Unop(IntTruncSat<int32_t, double>));
				break;

			case Opcode::I32TruncUF32:
				CHECK_TRAP(UnopTrap(IntTrunc<uint32_t, float>));
				break;

			case Opcode::I32TruncUSatF32:
				CHECK_TRAP(Unop(IntTruncSat<uint32_t, float>));
				break;

			case Opcode::I32TruncUF64:
				CHECK_TRAP(UnopTrap(IntTrunc<uint32_t, double>));
				break;

			case Opcode::I32TruncUSatF64:
				CHECK_TRAP(Unop(IntTruncSat<uint32_t, double>));
				break;

			case Opcode::I32WrapI64:
				CHECK_TRAP(Push<uint32_t>(Pop<uint64_t>()));
				break;

			case Opcode::I64TruncSF32:
				CHECK_TRAP(UnopTrap(IntTrunc<int64_t, float>));
				break;

			case Opcode::I64TruncSSatF32:
				CHECK_TRAP(Unop(IntTruncSat<int64_t, float>));
				break;

			case Opcode::I64TruncSF64:
				CHECK_TRAP(UnopTrap(IntTrunc<int64_t, double>));
				break;

			case Opcode::I64TruncSSatF64:
				CHECK_TRAP(Unop(IntTruncSat<int64_t, double>));
				break;

			case Opcode::I64TruncUF32:
				CHECK_TRAP(UnopTrap(IntTrunc<uint64_t, float>));
				break;

			case Opcode::I64TruncUSatF32:
				CHECK_TRAP(Unop(IntTruncSat<uint64_t, float>));
				break;

			case Opcode::I64TruncUF64:
				CHECK_TRAP(UnopTrap(IntTrunc<uint64_t, double>));
				break;

			case Opcode::I64TruncUSatF64:
				CHECK_TRAP(Unop(IntTruncSat<uint64_t, double>));
				break;

			case Opcode::I64ExtendSI32:
				CHECK_TRAP(Push<uint64_t>(Pop<int32_t>()));
				break;

			case Opcode::I64ExtendUI32:
				CHECK_TRAP(Push<uint64_t>(Pop<uint32_t>()));
				break;

			case Opcode::F32ConvertSI32:
				CHECK_TRAP(Push<float>(Pop<int32_t>()));
				break;

			case Opcode::F32ConvertUI32:
				CHECK_TRAP(Push<float>(Pop<uint32_t>()));
				break;

			case Opcode::F32ConvertSI64:
				CHECK_TRAP(Push<float>(Pop<int64_t>()));
				break;

			case Opcode::F32ConvertUI64:
				CHECK_TRAP(Push<float>(wabt_convert_uint64_to_float(Pop<uint64_t>())));
				break;

			case Opcode::F32DemoteF64: {
				typedef FloatTraits<float> F32Traits;
				typedef FloatTraits<double> F64Traits;

				uint64_t value = PopRep<double>();
				if (WABT_LIKELY((IsConversionInRange<float, double>(value)))) {
					CHECK_TRAP(Push<float>(FromRep<double>(value)));
				} else if (IsInRangeF64DemoteF32RoundToF32Max(value)) {
					CHECK_TRAP(PushRep<float>(F32Traits::kMax));
				} else if (IsInRangeF64DemoteF32RoundToNegF32Max(value)) {
					CHECK_TRAP(PushRep<float>(F32Traits::kNegMax));
				} else {
					uint32_t sign = (value >> 32) & F32Traits::kSignMask;
					uint32_t tag = 0;
					if (F64Traits::IsNan(value)) {
						tag = F32Traits::kQuietNanBit |
						((value >> (F64Traits::kSigBits - F32Traits::kSigBits)) &
								F32Traits::kSigMask);
					}
					CHECK_TRAP(PushRep<float>(sign | F32Traits::kInf | tag));
				}
				break;
			}

			case Opcode::F32ReinterpretI32:
				CHECK_TRAP(PushRep<float>(Pop<uint32_t>()));
				break;

			case Opcode::F64ConvertSI32:
				CHECK_TRAP(Push<double>(Pop<int32_t>()));
				break;

			case Opcode::F64ConvertUI32:
				CHECK_TRAP(Push<double>(Pop<uint32_t>()));
				break;

			case Opcode::F64ConvertSI64:
				CHECK_TRAP(Push<double>(Pop<int64_t>()));
				break;

			case Opcode::F64ConvertUI64:
				CHECK_TRAP(Push<double>(wabt_convert_uint64_to_double(Pop<uint64_t>())));
				break;

			case Opcode::F64PromoteF32:
				CHECK_TRAP(Push<double>(Pop<float>()));
				break;

			case Opcode::F64ReinterpretI64:
				CHECK_TRAP(PushRep<double>(Pop<uint64_t>()));
				break;

			case Opcode::I32ReinterpretF32:
				CHECK_TRAP(Push<uint32_t>(PopRep<float>()));
				break;

			case Opcode::I64ReinterpretF64:
				CHECK_TRAP(Push<uint64_t>(PopRep<double>()));
				break;

			case Opcode::I32Rotr:
				CHECK_TRAP(Binop(IntRotr<uint32_t>));
				break;

			case Opcode::I32Rotl:
				CHECK_TRAP(Binop(IntRotl<uint32_t>));
				break;

			case Opcode::I64Rotr:
				CHECK_TRAP(Binop(IntRotr<uint64_t>));
				break;

			case Opcode::I64Rotl:
				CHECK_TRAP(Binop(IntRotl<uint64_t>));
				break;

			case Opcode::I64Eqz:
				CHECK_TRAP(Unop(IntEqz<uint32_t, uint64_t>));
				break;

			case Opcode::I32Extend8S:
				CHECK_TRAP(Unop(IntExtendS<uint32_t, int8_t>));
				break;

			case Opcode::I32Extend16S:
				CHECK_TRAP(Unop(IntExtendS<uint32_t, int16_t>));
				break;

			case Opcode::I64Extend8S:
				CHECK_TRAP(Unop(IntExtendS<uint64_t, int8_t>));
				break;

			case Opcode::I64Extend16S:
				CHECK_TRAP(Unop(IntExtendS<uint64_t, int16_t>));
				break;

			case Opcode::I64Extend32S:
				CHECK_TRAP(Unop(IntExtendS<uint64_t, int32_t>));
				break;

			case Opcode::Drop:
				(void)Pop();
				break;

			case Opcode::Nop:
				break;

			case Opcode::I32AtomicWait:
			case Opcode::I64AtomicWait:
			case Opcode::AtomicWake:
				// TODO(binji): Implement.
				TRAP(Unreachable);
				break;

				// The following opcodes are either never generated or should never be
				// executed.
			case Opcode::Block:
			case Opcode::Catch:
			case Opcode::CatchAll:
			case Opcode::Invalid:
			case Opcode::Loop:
			case Opcode::Rethrow:
			case Opcode::Throw:
			case Opcode::Try:
				WABT_UNREACHABLE;
				break;
			}
			++ it;
		}

		PopCall(func->sig->results.size());
exit_opcode_loop: ;
	}

exit_loop:
	return result;
}

}
