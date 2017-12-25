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

#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include "Utils.h"
#include "Opcode.h"

namespace wasm {

class Environment;
class Thread;
class Runtime;
class RuntimeModule;
class Module;
class ModuleReader;

struct HostFunc;

using TypeInitList = std::initializer_list<Type>;
using ValueInitList = std::initializer_list<Value>;
using HostFuncCallback = Result (*)(Thread *, const HostFunc * func, Value* buf);

struct Func {
	struct Local {
		Local(Type, Index);

		Type type;
		Index count;
	};

	struct OpcodeRec {
		Opcode::Enum opcode;
		union {
			struct {
				uint32_t v1;
				uint32_t v2;
			} value32;
			uint64_t value64;
		};

		OpcodeRec(Opcode::Enum op, uint32_t v1, uint32_t v2) noexcept
		: opcode(op) {
			value32.v1 = v1;
			value32.v2 = v2;
		}

		OpcodeRec(Opcode::Enum op, uint64_t v1) noexcept
		: opcode(op) {
			value64 = v1;
		}

		OpcodeRec(const OpcodeRec &) = default;
	};

	struct Label {
		Label(Index results, Index stack)
		: results(results), stack(stack) { }

		Label(Index results, Index stack, Index offset, Index origin = kInvalidIndex)
		: results(results), stack(stack), offset(offset), origin(origin) { }

		Label(const Label &) = default;

		Index results = 0;
		Index stack = 0;
		Index offset = 0;
		Index origin = kInvalidIndex;
	};

	struct Signature {
		Signature() = default;
		Signature(Index param_count, Type* param_types, Index result_count, Type* result_types);
		Signature(TypeInitList params, TypeInitList results);

		void printInfo(std::ostream &) const;

		Vector<Type> params;
		Vector<Type> results;
	};

	Func(const Signature *sig, const Module *);

	void printInfo(std::ostream &) const;

	const Signature *sig = nullptr;
	const Module *module = nullptr;
	Vector<Type> types;
	Vector<OpcodeRec> opcodes;
	String name;
};

// Module should store only headers and constant data, not runtime data
class Module {
public:
	using Signature = Func::Signature;

	struct Import {
		explicit Import(ExternalKind kind);

		Import(ExternalKind kind, StringView module, StringView field, const Signature *sig);
		Import(ExternalKind kind, StringView module, StringView field, Type, const Limits &);
		Import(ExternalKind kind, StringView module, StringView field, const Limits &);
		Import(ExternalKind kind, StringView module, StringView field, Type, bool);

		ExternalKind kind;
		String module;
		String field;

		union {
			struct {
				const Signature * sig;
			} func;

			struct {
				Limits limits;
				Type type = Type::Anyfunc;
			} table;

			struct {
				Limits limits;
			} memory;

			struct {
				Type type;
				bool mut;
			} global;
		};
	};

	struct Table {
		explicit Table(Type, const Limits& limits);

		Type type;
		Limits limits;
	};

	struct Memory {
		explicit Memory(const Limits& limits);

		Limits limits;
	};

	struct Global {
		Global() = default;
		Global(const TypedValue& value, bool mut);
		Global(Type value, bool mut);

		TypedValue value;
		bool mut = false;
	};

	struct IndexObject {
		IndexObject() = default;
		IndexObject(Index, bool);

		bool exported = false;
		bool import = false;
		Index index = kInvalidIndex;
	};

	struct Export {
		Export(ExternalKind, Index, IndexObject, StringView);

		ExternalKind kind = ExternalKind::Func;
		Index object;
		IndexObject index;
		String name;
	};

	struct Elements {
		Elements(Index, Index, Index capacity);

		Index table;
		Index offset;
		Vector<Index> values;
	};

	struct Data {
		Data(Index, Address offset, Address size, const uint8_t *data);

		Index memory;
		Address offset;
		Vector<uint8_t> data;
	};

	bool init(const uint8_t *, size_t, const ReadOptions & = ReadOptions());
	bool init(Environment *, const uint8_t *, size_t, const ReadOptions & = ReadOptions());
	bool init(Environment *, ModuleReader &reader, const uint8_t *, size_t, const ReadOptions & = ReadOptions());

	bool hasMemory() const;
	bool hasTable() const;

	Signature *getSignature(Index);
	const Signature *getSignature(Index) const;

	Func * getFunc(Index);
	Table * getTable(Index);
	Memory * getMemory(Index);
	Global * getGlobal(Index);

	const Func * getFunc(Index) const;
	const Table * getTable(Index) const;
	const Memory * getMemory(Index) const;
	const Global * getGlobal(Index) const;

	const Import * getImportFunc(Index) const;
	const Import * getImportGlobal(Index) const;
	const Import * getImportMemory(Index) const;
	const Import * getImportTable(Index) const;

	const IndexObject *getFunctionIndex(Index) const;
	const IndexObject *getGlobalIndex(Index) const;
	const IndexObject *getMemoryIndex(Index) const;
	const IndexObject *getTableIndex(Index) const;

	const Vector<IndexObject> & getFuncIndexVec() const;
	const Vector<IndexObject> & getGlobalIndexVec() const;
	const Vector<IndexObject> & getMemoryIndexVec() const;
	const Vector<IndexObject> & getTableIndexVec() const;

	const Vector<Import> & getImports() const;
	const Vector<Export> & getExports() const;

	std::pair<const Signature *, bool> getFuncSignature(Index) const;
	const Signature * getFuncSignature(const IndexObject &) const;

	std::pair<Type, bool> getGlobalType(Index) const; // Type, mutable
	std::pair<Type, bool> getGlobalType(const IndexObject &) const; // Type, mutable

	const Vector<Elements> &getTableElements() const;
	const Vector<Data> &getMemoryData() const;

	Offset getLinkingOffset() const;

	void printInfo(std::ostream &) const;

protected:
	friend class ModuleReader;

	Vector<Signature> _types;
	Vector<Import> _imports;

	Vector<Func> _funcs;
	Vector<Table> _tables;
	Vector<Memory> _memory;
	Vector<Global> _globals;

	Vector<IndexObject> _funcIndex;
	Vector<IndexObject> _globalIndex;
	Vector<IndexObject> _memoryIndex;
	Vector<IndexObject> _tableIndex;

	Vector<Export> _exports;
	Vector<Elements> _elements;
	Vector<Data> _data;

	IndexObject _startFunction;
	Index _stackPointer = kInvalidIndex;
	Offset _dataSize = kInvalidOffset;
};

}

#endif /* SRC_MODULE_H_ */
