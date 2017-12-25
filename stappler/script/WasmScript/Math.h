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

#ifndef WASMSCRIPT_MATH_H_
#define WASMSCRIPT_MATH_H_

#include <WasmScript.h>

#define WASM_MATH_FUNC(name) \
		WASM_IMPORT double _ws_ ## name ## d(double); \
		WASM_IMPORT float _ws_ ## name ## f(float);

WASM_MATH_FUNC(cos)
WASM_MATH_FUNC(sin)
WASM_MATH_FUNC(tan)
WASM_MATH_FUNC(acos)
WASM_MATH_FUNC(asin)
WASM_MATH_FUNC(atan)
WASM_MATH_FUNC(cosh)
WASM_MATH_FUNC(sinh)
WASM_MATH_FUNC(tanh)
WASM_MATH_FUNC(acosh)
WASM_MATH_FUNC(asinh)
WASM_MATH_FUNC(atanh)
WASM_MATH_FUNC(exp)
WASM_MATH_FUNC(log)
WASM_MATH_FUNC(log10)
WASM_MATH_FUNC(exp2)
WASM_MATH_FUNC(sqrt)
WASM_MATH_FUNC(ceil)
WASM_MATH_FUNC(floor)
WASM_MATH_FUNC(trunc)
WASM_MATH_FUNC(round)
WASM_MATH_FUNC(fabs)

WASM_IMPORT double _ws_atan2d(double, double);
WASM_IMPORT float _ws_atan2f(float, float);

WASM_IMPORT int32_t _ws_lroundd(double);
WASM_IMPORT int32_t _ws_lroundf(float);

WASM_IMPORT double _ws_fmodd(double, double);
WASM_IMPORT float _ws_fmodf(float, float);

WASM_IMPORT double _ws_powd(double, double);
WASM_IMPORT float _ws_powf(float, float);

WASM_IMPORT double _ws_ldexpd(double, int32_t);
WASM_IMPORT float _ws_ldexpf(float, int32_t);

WASM_IMPORT double _ws_modfd(double, double *);
WASM_IMPORT float _ws_modff(float, float *);

WASM_IMPORT double _ws_frexpd(double, int32_t *);
WASM_IMPORT float _ws_frexpf(float, int32_t *);

WASM_IMPORT double _ws_nand(const char *ptr);
WASM_IMPORT float _ws_nanf(const char *ptr);

#define WASM_NAND (__builtin_nan (""))
#define WASM_NANF (__builtin_nanf (""))

#ifdef __cplusplus
namespace script {

#define WASM_CPP_MATH_FUNC(name) \
		WASM_INLINE double name (double value) { return _ws_ ## name ## d(value); } \
		WASM_INLINE float name (float value) { return _ws_ ## name ## f(value); }

WASM_CPP_MATH_FUNC(cos)
WASM_CPP_MATH_FUNC(sin)
WASM_CPP_MATH_FUNC(tan)
WASM_CPP_MATH_FUNC(acos)
WASM_CPP_MATH_FUNC(asin)
WASM_CPP_MATH_FUNC(atan)
WASM_CPP_MATH_FUNC(cosh)
WASM_CPP_MATH_FUNC(sinh)
WASM_CPP_MATH_FUNC(tanh)
WASM_CPP_MATH_FUNC(acosh)
WASM_CPP_MATH_FUNC(asinh)
WASM_CPP_MATH_FUNC(atanh)
WASM_CPP_MATH_FUNC(exp)
WASM_CPP_MATH_FUNC(log)
WASM_CPP_MATH_FUNC(log10)
WASM_CPP_MATH_FUNC(exp2)
WASM_CPP_MATH_FUNC(sqrt)
WASM_CPP_MATH_FUNC(ceil)
WASM_CPP_MATH_FUNC(floor)
WASM_CPP_MATH_FUNC(trunc)
WASM_CPP_MATH_FUNC(round)
WASM_CPP_MATH_FUNC(fabs)

WASM_INLINE double atan2(double v1, double v2) { return _ws_atan2d(v1, v2); }
WASM_INLINE float atan2(float v1, float v2) { return _ws_atan2d(v1, v2); }

WASM_INLINE int32_t atan2(double v) { return _ws_lroundd(v); }
WASM_INLINE int32_t atan2(float v) { return _ws_lroundf(v); }

WASM_INLINE double fmod(double v1, double v2) { return _ws_fmodd(v1, v2); }
WASM_INLINE float fmod(float v1, float v2) { return _ws_fmodf(v1, v2); }

WASM_INLINE double pow(double v1, double v2) { return _ws_powd(v1, v2); }
WASM_INLINE float pow(float v1, float v2) { return _ws_powf(v1, v2); }

WASM_INLINE double ldexp(double v1, int32_t v2) { return _ws_ldexpd(v1, v2); }
WASM_INLINE float ldexp(float v1, int32_t v2) { return _ws_ldexpf(v1, v2); }

WASM_INLINE double modf(double v1, double *v2) { return _ws_modfd(v1, v2); }
WASM_INLINE float modf(float v1, float *v2) { return _ws_modff(v1, v2); }

WASM_INLINE double frexp(double v1, int32_t *v2) { return _ws_frexpd(v1, v2); }
WASM_INLINE float frexp(float v1, int32_t *v2) { return _ws_frexpf(v1, v2); }

template <typename T = float> auto nan() -> T;

template <> WASM_INLINE auto nan() -> float { return __builtin_nanf (""); }
template <> WASM_INLINE auto nan() -> double { return __builtin_nan (""); }

}
#endif

#endif /* WASMSCRIPT_MATH_H_ */
