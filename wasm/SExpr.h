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

#ifndef EXEC_SEXPR_H_
#define EXEC_SEXPR_H_

#include "wasm/Utils.h"

namespace sexpr {

using StringView = wasm::StringView;

template <typename T>
using Vector = wasm::Vector<T>;

struct Token {
	enum Kind {
		Word,
		List
	};

	Token() = default;

	explicit Token(StringView t) : kind(Word), token(t) { }
	explicit Token(Kind k) : kind(k) { }

	Kind kind = Word;
	StringView token;
	Vector<Token> vec;
};

Vector<Token> parse(StringView);
void print(std::ostream &, const Token &);
void print(std::ostream &, const Vector<Token> &);

}

#endif /* EXEC_SEXPR_H_ */
