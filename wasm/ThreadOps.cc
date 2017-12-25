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

#include "Thread.h"
#include "Environment.h"

namespace wasm {

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

template <typename T>
void LoadFromMemory(T* dst, const void* src) {
	memcpy(dst, src, sizeof(T));
}

template <typename T>
void StoreToMemory(void* dst, T value) {
	memcpy(dst, &value, sizeof(T));
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

}
