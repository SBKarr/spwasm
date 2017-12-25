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

#ifndef EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMSTD_H_
#define EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMSTD_H_

#include "SPCommon.h"

namespace wasm {

template <typename T>
using Vector = stappler::memory::PoolInterface::VectorType<T>;

template <typename T>
using Function = stappler::memory::function<T>;

template <typename K, typename V>
using Map = stappler::memory::PoolInterface::MapType<K, V, std::less<>>;

using String = stappler::memory::PoolInterface::StringType;
using StringStream = stappler::memory::PoolInterface::StringStreamType;

}

#endif /* EXTENSIONS_SPWASM_STAPPLER_SRC_SPWASMSTD_H_ */
