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

#include "Module.h"
#include "Binary.h"

#include <iomanip>

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
	case Type::Any: stream << "any"; break;
	}
}

static void printSignature(std::ostream &stream, const Module::Signature &sig) {
	stream << "(";
	bool first = true;
	for (auto &it : sig.params) {
		if (!first) { stream << ", "; } else { first = false; }
		printType(stream, it);
	}

	if (!sig.results.empty()) {
		if (sig.params.empty()) {
			stream << "()";
		}
		stream << " -> ";
	}

	first = true;
	for (auto &it : sig.results) {
		if (!first) { stream << ", "; } else { first = false; }
		printType(stream, it);
	}
	stream << ")";
}

static void printIndent(std::ostream &stream, Index indent) {
	for (Index i = 0; i < indent; ++ i) {
		stream << "\t";
	}
}

static void printFunctionData(std::ostream &stream, const Func &it, Index indent) {
	size_t j = 0;
	printIndent(stream, indent);
	if (!it.name.empty()) {
		stream << it.name << ": ";
	}
	stream << "Code (" << it.opcodes.size() << ")\n";
	for (auto &opcodeIt : it.opcodes) {
		printIndent(stream, indent);
		stream << "\t(" << j << ") " << Opcode(opcodeIt.opcode).GetName() << " ";
		switch (opcodeIt.opcode) {
		case Opcode::I32Const:
			stream << opcodeIt.value32.v1;
			break;
		case Opcode::I64Const:
			stream << opcodeIt.value64;
			break;
		case Opcode::F32Const:
			stream << Value(opcodeIt.value32.v1).asFloat();
			break;
		case Opcode::F64Const:
			stream << Value(opcodeIt.value64).asDouble();
			break;
		default:
			stream << opcodeIt.value32.v1 << " " << opcodeIt.value32.v2;
			break;
		}
		stream << "\n";
		++ j;
	}
}

Func::Local::Local(Type t, Index count) : type(t), count(count) { }


Func::Func(const Signature *sig, const Module *module) : sig(sig), module(module) { }


Module::Signature::Signature(Index param_count, Type* param_types, Index result_count, Type* result_types) {
	params.reserve(param_count);
	for (Index i = 0; i < param_count; ++ i) {
		params.emplace_back(param_types[i]);
	}

	results.reserve(result_count);
	for (Index i = 0; i < result_count; ++ i) {
		results.emplace_back(result_types[i]);
	}
}

Module::Signature::Signature(TypeInitList params, TypeInitList results)
: params(params), results(results) { }

void Module::Signature::printInfo(std::ostream &stream) const {
	printSignature(stream, *this);
}

Module::Import::Import(ExternalKind kind) : kind(kind) { }

Module::Import::Import(ExternalKind kind, StringView module, StringView field, const Signature *sig)
: kind(kind), module(module.data(), module.size()), field(field.data(), field.size()) { func.sig = sig; }

Module::Import::Import(ExternalKind kind, StringView module, StringView field, Type t, const Limits &l)
: kind(kind), module(module.data(), module.size()), field(field.data(), field.size()) { table.type = t; table.limits = l; }

Module::Import::Import(ExternalKind kind, StringView module, StringView field, const Limits &l)
: kind(kind), module(module.data(), module.size()), field(field.data(), field.size()) { memory.limits = l; }

Module::Import::Import(ExternalKind kind, StringView module, StringView field, Type t, bool mut)
: kind(kind), module(module.data(), module.size()), field(field.data(), field.size()) { global.type = t; global.mut = mut; }


Module::Table::Table(Type t, const Limits& limits) : type(t), limits(limits) { }

Module::Memory::Memory(const Limits& limits) : limits(limits) { }


Module::Global::Global(const TypedValue& value, bool mut)
: value(value), mut(mut) { }

Module::Global::Global(Type type, bool mut)
: value(type), mut(mut)  { }


Module::IndexObject::IndexObject(Index idx, bool import)
: import(import), index(idx) { }


Module::Export::Export(ExternalKind kind, Index obj, IndexObject index, StringView name)
: kind(kind), object(obj), index(index), name(name.data(), name.size()) { }


Module::Elements::Elements(Index t, Index off, Index capacity)
: table(t), offset(off) {
	values.reserve(capacity);
}


Module::Data::Data(Index m, Address offset, Address size, const uint8_t *bytes)
: memory(m), offset(offset) {
	data.assign(bytes, bytes + size);
}

bool Module::init(const uint8_t *data, size_t size, const ReadOptions &opts) {
	return init(nullptr, data, size, opts);
}

bool Module::init(Environment *env, const uint8_t *data, size_t size, const ReadOptions &opts) {
	wasm::ModuleReader reader;
	return init(env, reader, data, size, opts);
}

bool Module::init(Environment *env, ModuleReader &reader, const uint8_t *data, size_t size, const ReadOptions &opts) {
	return reader.init(this, env, data, size, opts);
}

bool Module::hasMemory() const {
	return !_memoryIndex.empty();
}

bool Module::hasTable() const {
	return !_tableIndex.empty();
}

Module::Signature *Module::getSignature(Index idx) {
	if (idx < _types.size()) {
		return &_types[idx];
	}
	return nullptr;
}

const Module::Signature *Module::getSignature(Index idx) const {
	if (idx < _types.size()) {
		return &_types[idx];
	}
	return nullptr;
}

Func * Module::getFunc(Index idx) {
	if (idx < _funcs.size()) {
		return &_funcs[idx];
	}
	return nullptr;
}
Module::Table * Module::getTable(Index idx) {
	if (idx < _tables.size()) {
		return &_tables[idx];
	}
	return nullptr;
}
Module::Memory * Module::getMemory(Index idx) {
	if (idx < _memory.size()) {
		return &_memory[idx];
	}
	return nullptr;
}
Module::Global * Module::getGlobal(Index idx) {
	if (idx < _globals.size()) {
		return &_globals[idx];
	}
	return nullptr;
}

const Func * Module::getFunc(Index idx) const {
	if (idx < _funcs.size()) {
		return &_funcs[idx];
	}
	return nullptr;
}
const Module::Table * Module::getTable(Index idx) const {
	if (idx < _tables.size()) {
		return &_tables[idx];
	}
	return nullptr;
}
const Module::Memory * Module::getMemory(Index idx) const {
	if (idx < _memory.size()) {
		return &_memory[idx];
	}
	return nullptr;
}
const Module::Global * Module::getGlobal(Index idx) const {
	if (idx < _globals.size()) {
		return &_globals[idx];
	}
	return nullptr;
}

const Module::Import * Module::getImportFunc(Index idx) const {
	if (idx < _imports.size() && _imports[idx].kind == ExternalKind::Func) {
		return &_imports[idx];
	}
	return nullptr;
}
const Module::Import * Module::getImportGlobal(Index idx) const {
	if (idx < _imports.size() && _imports[idx].kind == ExternalKind::Global) {
		return &_imports[idx];
	}
	return nullptr;
}
const Module::Import * Module::getImportMemory(Index idx) const {
	if (idx < _imports.size() && _imports[idx].kind == ExternalKind::Memory) {
		return &_imports[idx];
	}
	return nullptr;
}
const Module::Import * Module::getImportTable(Index idx) const {
	if (idx < _imports.size() && _imports[idx].kind == ExternalKind::Table) {
		return &_imports[idx];
	}
	return nullptr;
}

const Module::IndexObject *Module::getFunctionIndex(Index idx) const {
	if (idx < _funcIndex.size()) {
		return &_funcIndex[idx];
	}
	return nullptr;
}
const Module::IndexObject *Module::getGlobalIndex(Index idx) const {
	if (idx < _globalIndex.size()) {
		return &_globalIndex[idx];
	}
	return nullptr;
}
const Module::IndexObject *Module::getMemoryIndex(Index idx) const {
	if (idx < _memoryIndex.size()) {
		return &_memoryIndex[idx];
	}
	return nullptr;
}
const Module::IndexObject *Module::getTableIndex(Index idx) const {
	if (idx < _tableIndex.size()) {
		return &_tableIndex[idx];
	}
	return nullptr;
}

const Vector<Module::IndexObject> & Module::getFuncIndexVec() const {
	return _funcIndex;
}
const Vector<Module::IndexObject> & Module::getGlobalIndexVec() const {
	return _globalIndex;
}
const Vector<Module::IndexObject> & Module::getMemoryIndexVec() const {
	return _memoryIndex;
}
const Vector<Module::IndexObject> & Module::getTableIndexVec() const {
	return _tableIndex;
}

const Vector<Module::Import> & Module::getImports() const {
	return _imports;
}

const Vector<Module::Export> & Module::getExports() const {
	return _exports;
}

std::pair<const Module::Signature *, bool> Module::getFuncSignature(Index idx) const {
	if (auto obj = getFunctionIndex(idx)) {
		if (obj->import) {
			if (auto func = getImportFunc(obj->index)) {
				return std::pair<const Module::Signature *, bool>(func->func.sig, true);
			}
		} else {
			if (auto func = getFunc(obj->index)) {
				return std::pair<const Module::Signature *, bool>(func->sig, false);
			}
		}
	}
	return std::pair<const Module::Signature *, bool>(nullptr, false);
}

const Module::Signature * Module::getFuncSignature(const IndexObject &idx) const {
	if (idx.import) {
		if (auto func = getImportFunc(idx.index)) {
			return func->func.sig;
		}
	} else {
		if (auto func = getFunc(idx.index)) {
			return func->sig;
		}
	}
	return nullptr;
}

std::pair<Type, bool> Module::getGlobalType(Index idx) const {
	if (auto obj = getGlobalIndex(idx)) {
		if (obj->import) {
			if (auto g = getImportGlobal(obj->index)) {
				return std::pair<Type, bool>(g->global.type, g->global.mut);
			}
		} else {
			if (auto g = getGlobal(obj->index)) {
				return std::pair<Type, bool>(g->value.type, g->mut);
			}
		}
	}
	return std::pair<Type, bool>(Type::Void, false);
}

std::pair<Type, bool> Module::getGlobalType(const IndexObject &idx) const {
	if (idx.import) {
		if (auto g = getImportGlobal(idx.index)) {
			return std::pair<Type, bool>(g->global.type, g->global.mut);
		}
	} else {
		if (auto g = getGlobal(idx.index)) {
			return std::pair<Type, bool>(g->value.type, g->mut);
		}
	}
	return std::pair<Type, bool>(Type::Void, false);
}

const Vector<Module::Elements> &Module::getTableElements() const {
	return _elements;
}
const Vector<Module::Data> &Module::getMemoryData() const {
	return _data;
}

Offset Module::getLinkingOffset() const {
	return _dataSize;
}

void Func::printInfo(std::ostream &stream) const {
	printSignature(stream, *sig);
	stream << "\n";
	printFunctionData(stream, *this, 1);
}

void Module::printInfo(std::ostream &stream) const {
	stream << "Types: (" << _types.size() << ")\n";

	size_t i = 0;
	for (auto &it : _types) {
		stream << "\t(" << i << ") ";
		printSignature(stream, it);
		stream << "\n";
		++ i;
	}

	stream << "Imports:\n";
	if (!_imports.empty()) {
		for (auto &it : _imports) {
			stream << "\t";
			switch (it.kind) {
			case ExternalKind::Func:
				stream << "function \"" << it.module << "\".\"" << it.field << "\" ";
				printSignature(stream, *it.func.sig);
				break;
			case ExternalKind::Global:
				stream << "global \"" << it.module << "\".\"" << it.field << "\" ";
				printType(stream, it.global.type);
				if (it.global.mut) {
					stream << " mut";
				}
				break;
			case ExternalKind::Memory:
				stream << "memory \"" << it.module << "\".\"" << it.field << "\" initial:" << (it.memory.limits.initial * 64 * 1024) << "bytes";
				if (it.memory.limits.has_max) {
					stream << " max:" << (it.memory.limits.max * 64 * 1024) << "bytes";
				}
				if (it.memory.limits.is_shared) {
					stream << " shared";
				}
				break;
			case ExternalKind::Table:
				stream << "table \"" << it.module << "\".\"" << it.field << "\" initial:" << it.table.limits.initial;
				if (it.table.limits.has_max) {
					stream << " max:" << it.table.limits.max;
				}
				if (it.table.limits.is_shared) {
					stream << " shared";
				}
				break;
			default:
				break;
			}
			stream << "\n";
		}
	}

	stream << "Index spaces:\n";
	if (!_funcIndex.empty()) {
		i = 0;
		stream << "\tFunctions: (" << _funcIndex.size() << ")\n";
		for (auto &it : _funcIndex) {
			stream << "\t\t(" << i << ") -> (" << it.index << ") ";
			auto sig = getFuncSignature(i);
			if (sig.first) {
				printSignature(stream, *sig.first);
			}

			if (it.import) { stream << " imported"; }
			if (it.exported) { stream << " exported"; }
			stream << "\n";
			++ i;
		}
	}

	if (!_globalIndex.empty()) {
		i = 0;
		stream << "\tGlobals: (" << _globalIndex.size() << ")\n";
		for (auto &it : _globalIndex) {
			stream << "\t\t(" << i << ") -> (" << it.index << ") (";
			printType(stream, getGlobalType(i).first);
			if (it.import) { stream << " imported"; }
			if (it.exported) { stream << " exported"; }
			stream << "\n";
			++ i;
		}
	}

	if (!_memoryIndex.empty()) {
		i = 0;
		stream << "\tMemory: (" << _memoryIndex.size() << ")\n";
		for (auto &it : _memoryIndex) {
			stream << "\t\t(" << i << ") -> (" << it.index << ")";
			if (it.import) { stream << " imported"; }
			if (it.exported) { stream << " exported"; }

			if (it.index < _memory.size()) {
				auto &mem = _memory[it.index];
				stream << " ( " << mem.limits.initial;
				if (mem.limits.has_max) {
					stream << " max:" << mem.limits.max;
				}
				stream << " )";
			}
			stream << "\n";
			++ i;
		}
	}

	if (!_tableIndex.empty()) {
		i = 0;
		stream << "\tTables: (" << _tableIndex.size() << ")\n";
		for (auto &it : _tableIndex) {
			stream << "\t\t(" << i << ") -> (" << it.index << ")";
			if (it.import) { stream << " imported"; }
			if (it.exported) { stream << " exported"; }
			stream << "\n";
			++ i;
		}
	}

	if (!_exports.empty()) {
		stream << "Exports: (" << _data.size() << ")\n";
		for (auto &it : _exports) {
			stream << "\t";
			if (it.index.import) {
				stream << "imported ";
			} else {
				stream << "defined ";
			}
			switch (it.kind) {
			case ExternalKind::Func:
				stream << "function:" << it.index.index << " ";
				if (auto sig = getFuncSignature(it.index)) {
					printSignature(stream, *sig);
				}
				break;

			case ExternalKind::Table:
				stream << "table:" << it.index.index;
				break;
			case ExternalKind::Memory:
				stream << "memory:" << it.index.index;
				break;
			case ExternalKind::Global:
				stream << "global:" << it.index.index << " ";
				printType(stream, getGlobalType(it.index).first);
				break;
			case ExternalKind::Except:
				break;
			}

			stream << " as \"" << it.name << "\"\n";
		}
	}

	i = 0;
	stream << "Functions: (" << _funcs.size() << ")\n";
	for (auto &it : _funcs) {
		stream << "\t(" << i << ") ";
		printSignature(stream, *it.sig);
		stream << "\n";
		printFunctionData(stream, it, 2);
		++ i;
	}

	if (!_globals.empty()) {
		i = 0;
		stream << "Globals: (" << _globals.size() << ")\n";

		for (auto &it : _globals) {
			stream << "\t(" << i << ") ";
			printType(stream, it.value.type);

			switch (it.value.type) {
			case Type::I32: stream << ":" << it.value.value.i32; break;
			case Type::I64: stream << ":" << it.value.value.i64; break;
			case Type::F32: stream << ":" << it.value.value.f32_bits; break;
			case Type::F64: stream << ":" << it.value.value.f64_bits; break;
			case Type::Anyfunc: stream << ":" << it.value.value.i32; break;
			case Type::Func: stream << ":" << it.value.value.i32; break;
			case Type::Any: stream << ":" << it.value.value.i32; break;
			case Type::Void: break;
			}

			if (it.mut) {
				stream << " mutable";
			}

			++ i;
		}
	}

	if (!_data.empty()) {
		stream << "Data: (" << _data.size() << ")\n";
		i = 0;
		for (auto &it : _data) {
			stream << "\t(" << i << ") (" << it.offset << ":" << it.data.size() << ":\"";
			stream << std::hex;
			for (auto &b : it.data) {
				if (b < 127 && b >= 32) {
					stream << char(b);
				} else {
					stream << "\\" << std::setw(2) << std::setfill('0') << unsigned(b);
				}
			}
			stream << std::dec << std::setw(1);
			stream << "\") -> memory:" << it.memory << "\n";
			++ i;
		}
	}

	if (!_elements.empty()) {
		stream << "Elements: (" << _elements.size() << ")\n";
		i = 0;
		for (auto &it : _elements) {
			stream << "\t(" << i << ") (" << it.offset << ":" << it.values.size() << ":";
			bool first = true;
			for (auto &b : it.values) {
				if (!first) { stream << ", "; } else { first = false; }
				stream << "(" << b << ")";
			}
			stream << ") -> table:" << it.table << "\n";
			++ i;
		}
	}
}

}
