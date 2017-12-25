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

#ifndef WABT_STRING_VIEW_H_
#define WABT_STRING_VIEW_H_

#include <cassert>
#include <iterator>
#include <limits>

#include "Std.h"

namespace wasm {

// This is a simplified implemention of C++17's basic_string_view template.
//
// Missing features:
// * Not a template
// * No allocator support
// * Some functions are not implemented
// * Asserts instead of exceptions
// * Some functions are not constexpr because we don't compile in C++17 mode

class StringView {
public:
	using traits_type = std::char_traits<char> ;
	using value_type = char;
	using pointer = char *;
	using const_pointer = const char *;
	using reference = char &;
	using const_reference = const char &;
	using const_iterator = const char *;
	using iterator = const_iterator;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using reverse_iterator = const_reverse_iterator;
	using size_type = std::size_t ;
	using difference_type = std::ptrdiff_t;

	static const size_type npos = std::numeric_limits<size_t>::max();

	// construction and assignment
	constexpr StringView() noexcept;
	constexpr StringView(const StringView&) noexcept = default;
	StringView& operator=(const StringView&) noexcept = default;
	StringView(const char* str);
	StringView(const String &);
	constexpr StringView(const char* str, size_type len);

	// iterator support
	constexpr const_iterator begin() const noexcept;
	constexpr const_iterator end() const noexcept;
	constexpr const_iterator cbegin() const noexcept;
	constexpr const_iterator cend() const noexcept;
	const_reverse_iterator rbegin() const noexcept;
	const_reverse_iterator rend() const noexcept;
	const_reverse_iterator crbegin() const noexcept;
	const_reverse_iterator crend() const noexcept;

	// capacity
	constexpr size_type size() const noexcept;
	constexpr size_type length() const noexcept;
	constexpr size_type max_size() const noexcept;
	constexpr bool empty() const noexcept;

	// element access
	constexpr const_reference operator[](size_type pos) const;

	const_reference at(size_type pos) const;
	const_reference front() const;
	const_reference back() const;
	constexpr const_pointer data() const noexcept;

	// modifiers
	void remove_prefix(size_type n);
	void remove_suffix(size_type n);
	void swap(StringView& s) noexcept;

	// string operations
	size_type copy(char* s, size_type n, size_type pos = 0) const;
	StringView substr(size_type pos = 0, size_type n = npos) const;

	int compare(StringView s) const noexcept;
	int compare(size_type pos1, size_type n1, StringView s) const;
	int compare(size_type pos1, size_type n1, StringView s, size_type pos2, size_type n2) const;
	int compare(const char* s) const;
	int compare(size_type pos1, size_type n1, const char* s) const;
	int compare(size_type pos1, size_type n1, const char* s, size_type n2) const;

	size_type find(StringView s, size_type pos = 0) const noexcept;
	size_type find(char c, size_type pos = 0) const noexcept;
	size_type find(const char* s, size_type pos, size_type n) const;
	size_type find(const char* s, size_type pos = 0) const;

	size_type rfind(StringView s, size_type pos = npos) const noexcept;
	size_type rfind(char c, size_type pos = npos) const noexcept;
	size_type rfind(const char* s, size_type pos, size_type n) const;
	size_type rfind(const char* s, size_type pos = npos) const;

	size_type find_first_of(StringView s, size_type pos = 0) const noexcept;
	size_type find_first_of(char c, size_type pos = 0) const noexcept;
	size_type find_first_of(const char* s, size_type pos, size_type n) const;
	size_type find_first_of(const char* s, size_type pos = 0) const;
	size_type find_last_of(StringView s, size_type pos = npos) const noexcept;
	size_type find_last_of(char c, size_type pos = npos) const noexcept;
	size_type find_last_of(const char* s, size_type pos, size_type n) const;
	size_type find_last_of(const char* s, size_type pos = npos) const;

private:
	const char* _data;
	size_type _size;
};

// non-member comparison functions
inline bool operator==(StringView x, StringView y) noexcept {
	return x.compare(y) == 0;
}

inline bool operator!=(StringView x, StringView y) noexcept {
	return x.compare(y) != 0;
}

inline bool operator<(StringView x, StringView y) noexcept {
	return x.compare(y) < 0;
}

inline bool operator>(StringView x, StringView y) noexcept {
	return x.compare(y) > 0;
}

inline bool operator<=(StringView x, StringView y) noexcept {
	return x.compare(y) <= 0;
}

inline bool operator>=(StringView x, StringView y) noexcept {
	return x.compare(y) >= 0;
}

inline constexpr StringView::StringView() noexcept : _data(nullptr), _size(0) { }

inline StringView::StringView(const char* str) : _data(str), _size(traits_type::length(str)) { }

inline StringView::StringView(const String &str) : _data(str.data()), _size(str.size()) { }

inline constexpr StringView::StringView(const char* str, size_type len) : _data(str), _size(len) { }

inline constexpr StringView::const_iterator StringView::begin() const noexcept {
	return _data;
}

inline constexpr StringView::const_iterator StringView::end() const noexcept {
	return _data + _size;
}

inline constexpr StringView::const_iterator StringView::cbegin() const noexcept {
	return _data;
}

inline constexpr StringView::const_iterator StringView::cend() const noexcept {
	return _data + _size;
}

inline StringView::const_reverse_iterator StringView::rbegin() const noexcept {
	return const_reverse_iterator(end());
}

inline StringView::const_reverse_iterator StringView::rend() const noexcept {
	return const_reverse_iterator(begin());
}

inline StringView::const_reverse_iterator StringView::crbegin() const noexcept {
	return const_reverse_iterator(cend());
}

inline StringView::const_reverse_iterator StringView::crend() const noexcept {
	return const_reverse_iterator(cbegin());
}

constexpr inline StringView::size_type StringView::size() const noexcept {
	return _size;
}

constexpr inline StringView::size_type StringView::length() const noexcept {
	return _size;
}

constexpr inline bool StringView::empty() const noexcept {
	return _size == 0;
}

constexpr inline StringView::const_reference StringView::operator[](size_type pos) const {
	return _data[pos];
}

inline StringView::const_reference StringView::at(size_type pos) const {
	assert(pos < _size);
	return _data[pos];
}

inline StringView::const_reference StringView::front() const {
	assert(!empty());
	return *_data;
}

inline StringView::const_reference StringView::back() const {
	assert(!empty());
	return _data[_size - 1];
}

constexpr inline StringView::const_pointer StringView::data() const noexcept {
	return _data;
}

inline std::basic_ostream<char> &
operator << (std::basic_ostream<char> & os, const StringView & str) {
	return os.write(str.data(), str.size());
}

} // namespace wasm

#endif  // WABT_STRING_VIEW_H_
