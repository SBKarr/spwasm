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

#define PRINT_CONTENT 0

#if (PRINT_CONTENT)
#define BINARY_PRINTF(...) printf(__VA_ARGS__)
#else
#define BINARY_PRINTF
#endif

namespace wasm {

static void printType(std::ostream &stream, Type t) {
	switch (t) {
	case Type::I32: stream << "i32"; break;
	case Type::I64: stream << "i64"; break;
	case Type::F32: stream << "f32"; break;
	case Type::F64: stream << "f64"; break;
	case Type::Anyfunc: stream << "anyfunc"; break;
	case Type::Func: stream << "func"; break;
	case Type::Void: stream << "void"; break;
	default: stream << "unknown"; break;
	}
}


/* Type section */
Result ModuleReader::BeginTypeSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnTypeCount(Index count) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, count);
	_targetModule->_types.reserve(count);

	return Result::Ok;
}
Result ModuleReader::OnType(Index index, Index nparam, Type* param, Index nresult, Type* result) {
	if (index == _targetModule->_types.size()) {
#if (PRINT_CONTENT)
		StringStream stream; stream << __FUNCTION__ << " [" << index << "]";

		for (Index i = 0; i < nparam; ++ i) {
			stream << " (param ";
			printType(stream, param[i]);
			stream << ")";
		}

		for (Index i = 0; i < nresult; ++ i) {
			stream << " (return ";
			printType(stream, result[i]);
			stream << ")";
		}

		stream << "\n";

		BINARY_PRINTF("%s", stream.str().data());
#endif

		_targetModule->_types.emplace_back(nparam, param, nresult, result);
		return Result::Ok;
	}
	return Result::Error;
}
Result ModuleReader::EndTypeSection() { return Result::Ok; }


/* Import section */
Result ModuleReader::BeginImportSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnImportCount(Index count) {
	_targetModule->_imports.reserve(count);
	return Result::Ok;
}
Result ModuleReader::OnImport(Index index, StringView module, StringView field) { return Result::Ok; }
Result ModuleReader::OnImportFunc(Index import, StringView module, StringView field, Index func, Index sig) {
	BINARY_PRINTF("%s %u %.*s %.*s %u %u\n", __FUNCTION__, import, (int)module.size(), module.data(), (int)field.size(), field.data(), func, sig);
	if (auto sigObj = _targetModule->getSignature(sig)) {
		_targetModule->_imports.emplace_back(ExternalKind::Func, module, field, sigObj);
		_targetModule->_funcIndex.emplace_back(_targetModule->_imports.size() - 1, true);
		return Result::Ok;
	}
	PushErrorStream([&] (StringStream &stream) {
		stream << "Function signature with index " << sig << " not found";
	});
	return Result::Error;
}
Result ModuleReader::OnImportTable(Index import, StringView module, StringView field, Index table, Type elem, const Limits* elemLimits) {
	BINARY_PRINTF("%s %u %.*s %.*s %u %d (%lu %lu)\n", __FUNCTION__, import, (int)module.size(), module.data(), (int)field.size(), field.data(), table, elem
			, elemLimits->initial, elemLimits->max);

	_targetModule->_imports.emplace_back(ExternalKind::Table, module, field, elem, *elemLimits);
	_targetModule->_tableIndex.emplace_back(_targetModule->_imports.size() - 1, true);
	return Result::Ok;
}
Result ModuleReader::OnImportMemory(Index import, StringView module, StringView field, Index memory, const Limits* pageLimits) {
	BINARY_PRINTF("%s %u %.*s %.*s %u (%lu %lu)\n", __FUNCTION__, import, (int)module.size(), module.data(), (int)field.size(), field.data(), memory
				, pageLimits->initial, pageLimits->max);

	_targetModule->_imports.emplace_back(ExternalKind::Memory, module, field, *pageLimits);
	_targetModule->_memoryIndex.emplace_back(_targetModule->_imports.size() - 1, true);
	return Result::Ok;
}
Result ModuleReader::OnImportGlobal(Index import, StringView module, StringView field, Index global, Type type, bool mut) {
	BINARY_PRINTF("%s %u %.*s %.*s %u %d (%d)\n", __FUNCTION__, import, (int)module.size(), module.data(), (int)field.size(), field.data(), global, type, mut);

	_targetModule->_imports.emplace_back(ExternalKind::Global, module, field, type, mut);
	_targetModule->_globalIndex.emplace_back(_targetModule->_imports.size() - 1, true);
	return Result::Ok;
}
Result ModuleReader::OnImportException(Index import, StringView module, StringView field, Index except, TypeVector& sig) {
	BINARY_PRINTF("%s %u %.*s %.*s %u\n", __FUNCTION__, import, (int)module.size(), module.data(), (int)field.size(), field.data(), except);
	return Result::Ok;
}
Result ModuleReader::EndImportSection() { return Result::Ok; }


/* Function section */
Result ModuleReader::BeginFunctionSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnFunctionCount(Index count) {
	_targetModule->_funcs.reserve(count);
	return Result::Ok;
}
Result ModuleReader::OnFunction(Index index, Index sig) {
	BINARY_PRINTF("%s %u %u\n", __FUNCTION__, index, sig);
	if (auto sigObj = _targetModule->getSignature(sig)) {
		_targetModule->_funcs.emplace_back(sigObj, _targetModule);
		_targetModule->_funcIndex.emplace_back(_targetModule->_funcs.size() - 1, false);
		return Result::Ok;
	}
	PushErrorStream([&] (StringStream &stream) {
		stream << "Function signature with index " << sig << " not found";
	});
	return Result::Error;
}
Result ModuleReader::EndFunctionSection() { return Result::Ok; }


/* Table section */
Result ModuleReader::BeginTableSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnTableCount(Index count) {
	_targetModule->_tables.reserve(count);
	return Result::Ok;
}
Result ModuleReader::OnTable(Index index, Type type, const Limits* limits) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	_targetModule->_tables.emplace_back(type, *limits);
	_targetModule->_tableIndex.emplace_back(_targetModule->_tables.size() - 1, false);
	return Result::Ok;
}
Result ModuleReader::EndTableSection() { return Result::Ok; }


/* Memory section */
Result ModuleReader::BeginMemorySection(Offset size) { return Result::Ok; }
Result ModuleReader::OnMemoryCount(Index count) {
	_targetModule->_memory.reserve(count);
	return Result::Ok;
}
Result ModuleReader::OnMemory(Index index, const Limits* limits) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, index);
	_targetModule->_memory.emplace_back(*limits);
	_targetModule->_memoryIndex.emplace_back(_targetModule->_memory.size() - 1, false);
	return Result::Ok;
}
Result ModuleReader::EndMemorySection() { return Result::Ok; }


/* Global section */
Result ModuleReader::BeginGlobalSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnGlobalCount(Index count) {
	_targetModule->_globals.reserve(count);
	return Result::Ok;
}
Result ModuleReader::BeginGlobal(Index index, Type type, bool mut) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	_targetModule->_globals.emplace_back(type, mut);
	_targetModule->_globalIndex.emplace_back(_targetModule->_globals.size() - 1, false);
	return Result::Ok;
}
Result ModuleReader::BeginGlobalInitExpr(Index index) {
	_initExprValue.type = Type::Void;
	return Result::Ok;
}
Result ModuleReader::EndGlobalInitExpr(Index index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	Module::Global* global = _targetModule->getGlobal(index);
	if (_initExprValue.type != global->value.type) {
		PushErrorStream([&] (std::ostream &stream) {
			stream << "type mismatch in global, expected ";
			printType(stream, global->value.type);
			stream << " but got ";
			printType(stream, _initExprValue.type);
			stream << ".";
		});
		return Result::Error;
	}
	global->value = _initExprValue;
	return Result::Ok;
}
Result ModuleReader::EndGlobal(Index index) { return Result::Ok; }
Result ModuleReader::EndGlobalSection() { return Result::Ok; }


/* Exports section */
Result ModuleReader::BeginExportSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnExportCount(Index count) {
	_targetModule->_exports.reserve(count);
	return Result::Ok;
}
Result ModuleReader::OnExport(Index index, ExternalKind kind, Index item_index, StringView name) {
	switch (kind) {
	case ExternalKind::Func:
		BINARY_PRINTF("%s func %u %u %.*s\n", __FUNCTION__, index, item_index, (int)name.size(), name.data());
		if (item_index < _targetModule->_funcIndex.size()) {
			_targetModule->_funcIndex[item_index].exported = true;
			_targetModule->_exports.emplace_back(kind, item_index, _targetModule->_funcIndex[item_index], name);
		} else {
			PushErrorStream([&] (StringStream &stream) {
				stream << "No func object for export found";
			});
			return Result::Error;
		}
		break;

	case ExternalKind::Table:
		BINARY_PRINTF("%s table %u %u %.*s\n", __FUNCTION__, index, item_index, (int)name.size(), name.data());
		if (item_index < _targetModule->_tableIndex.size()) {
			_targetModule->_tableIndex[item_index].exported = true;
			_targetModule->_exports.emplace_back(kind, item_index, _targetModule->_tableIndex[item_index], name);
		} else {
			PushErrorStream([&] (StringStream &stream) {
				stream << "No table object for export found";
			});
			return Result::Error;
		}
		break;

	case ExternalKind::Memory:
		BINARY_PRINTF("%s memory %u %u %.*s\n", __FUNCTION__, index, item_index, (int)name.size(), name.data());
		if (item_index < _targetModule->_memoryIndex.size()) {
			_targetModule->_memoryIndex[item_index].exported = true;
			_targetModule->_exports.emplace_back(kind, item_index, _targetModule->_memoryIndex[item_index], name);
		} else {
			PushErrorStream([&] (StringStream &stream) {
				stream << "No memory object for export found";
			});
			return Result::Error;
		}
		break;

	case ExternalKind::Global:
		BINARY_PRINTF("%s global %u %u %.*s\n", __FUNCTION__, index, item_index, (int)name.size(), name.data());
		if (item_index < _targetModule->_globalIndex.size()) {
			_targetModule->_globalIndex[item_index].exported = true;
			_targetModule->_exports.emplace_back(kind, item_index, _targetModule->_globalIndex[item_index], name);
		} else {
			PushErrorStream([&] (StringStream &stream) {
				stream << "No global object for export found";
			});
			return Result::Error;
		}
		break;

	case ExternalKind::Except:
		// TODO(karlschimpf) Define
		PushErrorStream([&] (StringStream &stream) {
			stream << "BinaryReaderInterp::OnExport(except) not implemented";
		});
		return Result::Error;
		break;

	default:
		break;
	}
	return Result::Ok;
}
Result ModuleReader::EndExportSection() { return Result::Ok; }


/* Start section */
Result ModuleReader::BeginStartSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnStartFunction(Index func_index) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, func_index);
	if (func_index < _targetModule->_funcIndex.size()) {
		_targetModule->_startFunction = _targetModule->_funcIndex[func_index];
	} else {
		PushErrorStream([&] (StringStream &stream) { stream << "No func object for start function found"; });
		return Result::Error;
	}
	return Result::Ok;
}
Result ModuleReader::EndStartSection() { return Result::Ok; }


}
