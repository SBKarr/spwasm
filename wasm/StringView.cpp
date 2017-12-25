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

#include "StringView.h"

#include <algorithm>

namespace wasm {

void StringView::remove_prefix(size_type n) {
	assert(n <= _size);
	_data += n;
	_size -= n;
}

void StringView::remove_suffix(size_type n) {
	assert(n <= _size);
	_size -= n;
}

void StringView::swap(StringView& s) noexcept {
	std::swap(_data, s._data);
	std::swap(_size, s._size);
}

constexpr StringView::size_type StringView::max_size() const noexcept {
	return std::numeric_limits<size_type>::max();
}

StringView::size_type StringView::copy(char* s, size_type n, size_type pos) const {
	assert(pos <= _size);
	size_t count = std::min(n, _size - pos);
	traits_type::copy(s, _data + pos, count);
	return count;
}

StringView StringView::substr(size_type pos, size_type n) const {
	assert(pos <= _size);
	size_t count = std::min(n, _size - pos);
	return StringView(_data + pos, count);
}

int StringView::compare(StringView s) const noexcept {
	size_type rlen = std::min(_size, s._size);
	int result = traits_type::compare(_data, s._data, rlen);
	if (result != 0 || _size == s._size) {
		return result;
	}
	return _size < s._size ? -1 : 1;
}

int StringView::compare(size_type pos1, size_type n1, StringView s) const {
	return substr(pos1, n1).compare(s);
}

int StringView::compare(size_type pos1, size_type n1, StringView s,
		size_type pos2, size_type n2) const {
	return substr(pos1, n1).compare(s.substr(pos2, n2));
}

int StringView::compare(const char* s) const {
	return compare(StringView(s));
}

int StringView::compare(size_type pos1, size_type n1, const char* s) const {
	return substr(pos1, n1).compare(StringView(s));
}

int StringView::compare(size_type pos1, size_type n1, const char* s, size_type n2) const {
	return substr(pos1, n1).compare(StringView(s, n2));
}

StringView::size_type StringView::find(StringView s, size_type pos) const noexcept {
	pos = std::min(pos, _size);
	const_iterator iter = std::search(begin() + pos, end(), s.begin(), s.end());
	return iter == end() ? npos : iter - begin();
}

StringView::size_type StringView::find(char c, size_type pos) const noexcept {
	return find(StringView(&c, 1), pos);
}

StringView::size_type StringView::find(const char* s, size_type pos,
		size_type n) const {
	return find(StringView(s, n), pos);
}

StringView::size_type StringView::find(const char* s, size_type pos) const {
	return find(StringView(s), pos);
}

StringView::size_type StringView::rfind(StringView s, size_type pos) const noexcept {
	pos = std::min(std::min(pos, _size - s._size) + s._size, _size);
	reverse_iterator iter = std::search(reverse_iterator(begin() + pos), rend(), s.rbegin(), s.rend());
	return iter == rend() ? npos : (rend() - iter - s._size);
}

StringView::size_type StringView::rfind(char c, size_type pos) const noexcept {
	return rfind(StringView(&c, 1), pos);
}

StringView::size_type StringView::rfind(const char* s, size_type pos, size_type n) const {
	return rfind(StringView(s, n), pos);
}

StringView::size_type StringView::rfind(const char* s, size_type pos) const {
	return rfind(StringView(s), pos);
}

StringView::size_type StringView::find_first_of(StringView s, size_type pos) const noexcept {
	pos = std::min(pos, _size);
	const_iterator iter = std::find_first_of(begin() + pos, end(), s.begin(), s.end());
	return iter == end() ? npos : iter - begin();
}

StringView::size_type StringView::find_first_of(char c, size_type pos) const noexcept {
	return find_first_of(StringView(&c, 1), pos);
}

StringView::size_type StringView::find_first_of(const char* s, size_type pos, size_type n) const {
	return find_first_of(StringView(s, n), pos);
}

StringView::size_type StringView::find_first_of(const char* s, size_type pos) const {
	return find_first_of(StringView(s), pos);
}

StringView::size_type StringView::find_last_of(StringView s, size_type pos) const noexcept {
	pos = std::min(pos, _size - 1);
	reverse_iterator iter = std::find_first_of(reverse_iterator(begin() + pos + 1), rend(), s.begin(), s.end());
	return iter == rend() ? npos : (rend() - iter - 1);
}

StringView::size_type StringView::find_last_of(char c, size_type pos) const noexcept {
	return find_last_of(StringView(&c, 1), pos);
}

StringView::size_type StringView::find_last_of(const char* s, size_type pos, size_type n) const {
	return find_last_of(StringView(s, n), pos);
}

StringView::size_type StringView::find_last_of(const char* s, size_type pos) const {
	return find_last_of(StringView(s), pos);
}

}  // namespace wasm
