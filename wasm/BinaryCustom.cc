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
Result ModuleReader::BeginNamesSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnFunctionNameSubsection(Index index, uint32_t name_type, Offset subsection_size) { return Result::Ok; }
Result ModuleReader::OnFunctionNamesCount(Index num_functions) {
	return Result::Ok;
}
Result ModuleReader::OnFunctionName(Index function_index, StringView function_name) {
	if (auto fnIndex = _targetModule->getFunctionIndex(function_index)) {
		if (!fnIndex->import) {
			if (auto fn = _targetModule->getFunc(fnIndex->index)) {
				fn->name.assign(function_name.data(), function_name.size());
			}
		}
	}
	return Result::Ok;
}
Result ModuleReader::OnLocalNameSubsection(Index index, uint32_t name_type, Offset subsection_size) { return Result::Ok; }
Result ModuleReader::OnLocalNameFunctionCount(Index num_functions) { return Result::Ok; }
Result ModuleReader::OnLocalNameLocalCount(Index function_index, Index num_locals) { return Result::Ok; }
Result ModuleReader::OnLocalName(Index function_index, Index local_index, StringView local_name) { return Result::Ok; }
Result ModuleReader::EndNamesSection() { return Result::Ok; }

/* Reloc section */
Result ModuleReader::BeginRelocSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnRelocCount(Index count, BinarySection section_code, StringView section_name) { return Result::Ok; }
Result ModuleReader::OnReloc(RelocType type, Offset offset, Index index, uint32_t addend) { return Result::Ok; }
Result ModuleReader::EndRelocSection() { return Result::Ok; }

/* Linking section */
Result ModuleReader::BeginLinkingSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnStackGlobal(Index stack_global) { return Result::Ok; }
Result ModuleReader::OnSymbolInfoCount(Index count) { return Result::Ok; }
Result ModuleReader::OnSymbolInfo(StringView name, uint32_t flags) { return Result::Ok; }
Result ModuleReader::OnDataSize(uint32_t data_size) {
	_targetModule->_dataSize = data_size;
	return Result::Ok;
}
Result ModuleReader::OnDataAlignment(uint32_t data_alignment) { return Result::Ok; }
Result ModuleReader::OnSegmentInfoCount(Index count) { return Result::Ok; }
Result ModuleReader::OnSegmentInfo(Index index, StringView name, uint32_t alignment, uint32_t flags) { return Result::Ok; }
Result ModuleReader::EndLinkingSection() { return Result::Ok; }

/* Exception section */
Result ModuleReader::BeginExceptionSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnExceptionCount(Index count) { return Result::Ok; }
Result ModuleReader::OnExceptionType(Index index, TypeVector& sig) { return Result::Ok; }
Result ModuleReader::EndExceptionSection() { return Result::Ok; }

}
