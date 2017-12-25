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

#ifndef SRC_STD_H_
#define SRC_STD_H_

#if STAPPLER

#include "SPWasmStd.h"

#else

#include <type_traits>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <functional>

namespace wasm {

template <typename T>
using Vector = std::vector<T>;

template <typename T>
using Function = std::function<T>;

template <typename K, typename V>
using Map = std::map<K, V, std::less<>>;

using String = std::string;
using StringStream = std::ostringstream;

}

#endif

#endif /* SRC_STD_H_ */
