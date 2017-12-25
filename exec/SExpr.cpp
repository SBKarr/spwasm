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

#include "SExpr.h"

namespace sexpr {

template <typename E>
constexpr typename std::underlying_type<E>::type toInt(const E &e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

#define SP_DEFINE_ENUM_AS_MASK(Type) \
	constexpr inline Type operator | (const Type &l, const Type &r) { return Type(toInt(l) | toInt(r)); } \
	constexpr inline Type operator & (const Type &l, const Type &r) { return Type(toInt(l) & toInt(r)); } \
	constexpr inline Type & operator |= (Type &l, const Type &r) { l = Type(toInt(l) | toInt(r)); return l; } \
	constexpr inline Type & operator &= (Type &l, const Type &r) { l = Type(toInt(l) & toInt(r)); return l; } \
	constexpr inline bool operator == (const Type &l, const std::underlying_type<Type>::type &r) { return toInt(l) == r; } \
	constexpr inline bool operator == (const std::underlying_type<Type>::type &l, const Type &r) { return l == toInt(r); } \
	constexpr inline bool operator != (const Type &l, const std::underlying_type<Type>::type &r) { return toInt(l) != r; } \
	constexpr inline bool operator != (const std::underlying_type<Type>::type &l, const Type &r) { return l != toInt(r); } \
	constexpr inline Type operator~(const Type &t) { return Type(~toInt(t)); }

/// Define character types
enum class Type : uint8_t {
	Whitespace	= 1 << 0,	//!< ' ','\t'
	Punctuation	= 1 << 1,	//!< .!?,;:"'`~(){}[]#$%+-=<>&@\/^*_|
	Alpha		= 1 << 2,	//!< a-zA-Z
	Digit		= 1 << 3,	//!< 0-9
	LineEnding	= 1 << 4,	//!< \n,\r,\0
};

SP_DEFINE_ENUM_AS_MASK(Type);

static uint8_t smart_char_type[256] = {
	16,  0,  0,  0,  0,  0,  0,  0,  0,  1, 16,  0,  0, 16,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	1,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
	8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  2,  2,  2,  2,  2,  2,
	2,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
	4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,
	2,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
	4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

bool isType(char c, Type t) {
	return smart_char_type[(unsigned char) c] & toInt(t);
}

static void skipWhitespace(StringView & r) {
	auto d = r.data();
	auto s = r.size();

	while (s > 0 && isType(d[0], Type::Whitespace | Type::LineEnding)) {
		++ d; -- s;
	}

	r = StringView(d, s);
}

static void skipUntilNewLine(StringView & r) {
	auto d = r.data();
	auto s = r.size();

	while (s > 0 && !isType(d[0], Type::LineEnding)) {
		++ d; -- s;
	}

	while (s > 0 && isType(d[0], Type::LineEnding)) {
		++ d; -- s;
	}

	r = StringView(d, s);
}

static void skipWhitespaceAndComments(StringView & r) {
	skipWhitespace(r);
	while (r.size() > 1 && r[0] == ';' && r[1] == ';') {
		skipUntilNewLine(r);
		skipWhitespace(r);
	}
}

static StringView readNormalToken(StringView & r) {
	auto origin = r.data();
	auto d = origin;
	auto s = r.size();

	while (s > 0 && !isType(d[0], Type::Whitespace | Type::LineEnding) && d[0] != ')' && d[0] != '(') {
		++ d; -- s;
	}

	r = StringView(d, s);
	return StringView(origin, d - origin);
}

static StringView readQuotedToken(StringView & r) {
	auto origin = r.data();
	auto d = origin;
	auto s = r.size();

	char q = 0;
	if (d[0] == '"') {
		q = '"';
		++ d; -- s; ++ origin;
	} else if (d[0] == '\'') {
		q = '\'';
		++ d; -- s; ++ origin;
	} else {
		return StringView();
	}

	while (s > 0 && d[0] != q) {
		++ d; -- s;
	}

	if (s > 0 && d[0] == q) {
		++ d; -- s;

		r = StringView(d, s);
		return StringView(origin, d - origin - 1);
	} else {
		r = StringView(d, s);
		return StringView(origin, d - origin);
	}
}

static void pushToken(Token &ret, StringView token) {
	if (ret.vec.empty()) {
		ret.token = token;
	}
	ret.vec.push_back(Token(token));
}

static void readBracedExpr(StringView &r, Token &target) {
	if (r[0] == '(') {
		r = StringView(r.data() + 1, r.size() - 1);
	}

	while (!r.empty()) {
		skipWhitespaceAndComments(r);
		if (r[0] == '(') {
			target.vec.emplace_back(Token::List);
			readBracedExpr(r, target.vec.back());
			// read token list
		} else if (r[0] == '"') {
			// read normal token
			auto t = readQuotedToken(r);
			pushToken(target, t);
			// read quoted token
		} else if (r[0] == ')') {
			r = StringView(r.data() + 1, r.size() - 1);
			return;
		} else {
			// read normal token
			auto t = readNormalToken(r);
			if (!t.empty()) {
				pushToken(target, t);
			}
		}
	}
}

Vector<Token> parse(StringView source) {
	Vector<Token> ret;
	StringView r(source);

	skipWhitespaceAndComments(r);
	while (!r.empty() && r[0] == '(') {
		ret.emplace_back(Token::List);
		readBracedExpr(r, ret.back());
		skipWhitespaceAndComments(r);
	}

	return ret;
}

void print(std::ostream &stream, const Token &token) {
	switch (token.kind) {
	case Token::Word:
		stream << token.token;
		break;
	case Token::List:
		stream << "( ";
		for (auto &it : token.vec) {
			print(stream, it);
			stream << " ";
		}
		stream << ")";
		break;
	};
}

void print(std::ostream &stream, const Vector<Token> &tokenList) {
	for (auto &it : tokenList) {
		print(stream, it);
		stream << "\n";
	}
}

}
