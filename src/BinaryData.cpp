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
#include "Environment.h"

#define PRINT_CONTENT 0

#if (PRINT_CONTENT)
#define BINARY_PRINTF(...) printf(VA_ARGS)
#else
#define BINARY_PRINTF
#endif

namespace wasm {

/* Elem section */
Result ModuleReader::BeginElemSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnElemSegmentCount(Index count) {
	_targetModule->_elements.reserve(count);
	return Result::Ok;
}
Result ModuleReader::BeginElemSegment(Index index, Index table_index) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, table_index);

	_currentIndex = table_index;
	return Result::Ok;
}
Result ModuleReader::BeginElemSegmentInitExpr(Index index) { return Result::Ok; }
Result ModuleReader::EndElemSegmentInitExpr(Index index) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, _initExprValue.value.i32);
	if (_initExprValue.type != Type::I32) {
		PushErrorStream([&] (StringStream &stream) { stream << "Invalid initializer type for element_segment"; });
		return Result::Error;
	}
	return Result::Ok;
}
Result ModuleReader::OnElemSegmentFunctionIndexCount(Index index, Index count) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, count);
	if (index != _targetModule->_elements.size()) {
		PushErrorStream([&] (StringStream &stream) { stream << "Invalid elements block indext"; });
		return Result::Error;
	}
	_targetModule->_elements.emplace_back(_currentIndex, _initExprValue.value.i32, count);
	return Result::Ok;
}
Result ModuleReader::OnElemSegmentFunctionIndex(Index segment_index, Index func_index) {
	BINARY_PRINTF("%s %u %u\n", __FUNCTION__, segment_index, func_index);
	_targetModule->_elements.at(segment_index).values.push_back(func_index);
	return Result::Ok;
}
Result ModuleReader::EndElemSegment(Index index) { return Result::Ok; }
Result ModuleReader::EndElemSection() { return Result::Ok; }


/* Data section */
Result ModuleReader::BeginDataSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnDataSegmentCount(Index count) {
	_targetModule->_data.reserve(count);
	return Result::Ok;
}
Result ModuleReader::BeginDataSegment(Index index, Index memory_index) {
	_currentIndex = memory_index;
	return Result::Ok;
}
Result ModuleReader::BeginDataSegmentInitExpr(Index index) {
	return Result::Ok;
}
Result ModuleReader::EndDataSegmentInitExpr(Index index) {
	if (_initExprValue.type != Type::I32) {
		PushErrorStream([&] (StringStream &stream) { stream << "Invalid initializer type for data_segment"; });
		return Result::Error;
	}
	return Result::Ok;
}
Result ModuleReader::OnDataSegmentData(Index index, const void* data, Address size) {
	BINARY_PRINTF("%s %u %u %u %u\n", __FUNCTION__, index, _currentIndex, _initExprValue.value.i32, size);
	if (index != _targetModule->_data.size()) {
		PushErrorStream([&] (StringStream &stream) { stream << "Invalid elements block index"; });
		return Result::Error;
	}
	_targetModule->_data.emplace_back(_currentIndex, _initExprValue.value.i32, size, (const uint8_t *)data);
	return Result::Ok;
}
Result ModuleReader::EndDataSegment(Index index) { return Result::Ok; }
Result ModuleReader::EndDataSection() { return Result::Ok; }


/* InitExpr - used by elem, data and global sections; these functions are
 * only called between calls to Begin*InitExpr and End*InitExpr */
Result ModuleReader::OnInitExprF32ConstExpr(Index index, uint32_t value) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, value);
	_initExprValue.type = Type::F32;
	_initExprValue.value.f32_bits = value;
	return Result::Ok;
}
Result ModuleReader::OnInitExprF64ConstExpr(Index index, uint64_t value) {
	BINARY_PRINTF("%s %lu\n", __FUNCTION__, value);
	_initExprValue.type = Type::F64;
	_initExprValue.value.f64_bits = value;
	return Result::Ok;
}
Result ModuleReader::OnInitExprGetGlobalExpr(Index index, Index global_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);

	if (auto idx = _targetModule->getGlobalIndex(global_index)) {
		if (idx->import) {
			if (!_env) {
				PushErrorStream([&] (StringStream &stream) { stream << "Environment for init with imported globals was not set"; });
				return Result::Error;
			}
			if (auto g = _targetModule->getImportGlobal(idx->index)) {
				if (_env->getGlobalValue(_initExprValue, g->module, g->field)) {
					return Result::Ok;
				}
			}
		} else {
			if (auto g = _targetModule->getGlobal(idx->index)) {
				_initExprValue = g->value;
				return Result::Ok;
			}
		}
	}

	PushErrorStream([&] (StringStream &stream) { stream << "Global for index " << global_index << " was not found"; });
	return Result::Error;
}
Result ModuleReader::OnInitExprI32ConstExpr(Index index, uint32_t value) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, value);
	_initExprValue.type = Type::I32;
	_initExprValue.value.i32 = value;
	return Result::Ok;
}
Result ModuleReader::OnInitExprI64ConstExpr(Index index, uint64_t value) {
	BINARY_PRINTF("%s %lu\n", __FUNCTION__, value);
	_initExprValue.type = Type::I64;
	_initExprValue.value.i64 = value;
	return Result::Ok;
}

}
