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

#include "Binary.h"
#include "Module.h"

namespace wasm {

/* Names section */
Result ModuleReader::BeginNamesSection(Offset size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnFunctionNameSubsection(Index index, uint32_t name_type, Offset subsection_size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnFunctionNamesCount(Index num_functions) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnFunctionName(Index function_index, StringView function_name) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnLocalNameSubsection(Index index, uint32_t name_type, Offset subsection_size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnLocalNameFunctionCount(Index num_functions) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnLocalNameLocalCount(Index function_index, Index num_locals) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnLocalName(Index function_index, Index local_index, StringView local_name) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::EndNamesSection() {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}

/* Reloc section */
Result ModuleReader::BeginRelocSection(Offset size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnRelocCount(Index count, BinarySection section_code, StringView section_name) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnReloc(RelocType type, Offset offset, Index index, uint32_t addend) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::EndRelocSection() {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}

/* Linking section */
Result ModuleReader::BeginLinkingSection(Offset size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnStackGlobal(Index stack_global) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnSymbolInfoCount(Index count) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnSymbolInfo(StringView name, uint32_t flags) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnDataSize(uint32_t data_size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnDataAlignment(uint32_t data_alignment) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnSegmentInfoCount(Index count) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnSegmentInfo(Index index, StringView name, uint32_t alignment, uint32_t flags) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::EndLinkingSection() {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}

/* Exception section */
Result ModuleReader::BeginExceptionSection(Offset size) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnExceptionCount(Index count) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::OnExceptionType(Index index, TypeVector& sig) {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::EndExceptionSection() {
	printf("%s\n", __FUNCTION__);
	return Result::Ok;
}


}
