/*
 * Copyright 2016 WebAssembly Community Group participants
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

#ifndef WABT_BINARY_H_
#define WABT_BINARY_H_

#include "TypeChecker.h"
#include "Module.h"

namespace wasm {

struct ReaderState {
	ReaderState() = default;
	ReaderState(const uint8_t* data, Offset size) : data(data), size(size) { }

	const uint8_t *data = nullptr;
	Offset size = 0;
	Offset offset = 0;
};

class ModuleReader {
public:
	bool init(Module *, Environment *env, const uint8_t *data, size_t size, const ReadOptions & = ReadOptions());

	bool OnError(StringStream & message);

	template <typename Callback>
	void PushErrorStream(const Callback &);

	/* Module */
	Result BeginModule(uint32_t version);
	Result EndModule();

	/* Custom section */
	Result BeginCustomSection(Offset size, const StringView &section_name);
	Result EndCustomSection();

	/* Type section */
	Result BeginTypeSection(Offset size);
	Result OnTypeCount(Index count);
	Result OnType(Index index, Index nparam, Type* param, Index nresult, Type* result);
	Result EndTypeSection();

	/* Import section */
	Result BeginImportSection(Offset size);
	Result OnImportCount(Index count);
	Result OnImport(Index index, StringView module, StringView field_name);
	Result OnImportFunc(Index import, StringView module, StringView field, Index func, Index sig);
	Result OnImportTable(Index import, StringView module, StringView field, Index table, Type elem, const Limits* elemLimits);
	Result OnImportMemory(Index import, StringView module, StringView field, Index memory, const Limits* pageLimits);
	Result OnImportGlobal(Index import, StringView module, StringView field, Index global, Type type, bool mut);
	Result OnImportException(Index import, StringView module, StringView field, Index except, TypeVector& sig);
	Result EndImportSection();

	/* Function section */
	Result BeginFunctionSection(Offset size);
	Result OnFunctionCount(Index count);
	Result OnFunction(Index index, Index sig);
	Result EndFunctionSection();

	/* Table section */
	Result BeginTableSection(Offset size);
	Result OnTableCount(Index count);
	Result OnTable(Index index, Type elem_type, const Limits* elem_limits);
	Result EndTableSection();

	/* Memory section */
	Result BeginMemorySection(Offset size);
	Result OnMemoryCount(Index count);
	Result OnMemory(Index index, const Limits* limits);
	Result EndMemorySection();

	/* Global section */
	Result BeginGlobalSection(Offset size);
	Result OnGlobalCount(Index count);
	Result BeginGlobal(Index index, Type type, bool mut);
	Result BeginGlobalInitExpr(Index index);
	Result EndGlobalInitExpr(Index index);
	Result EndGlobal(Index index);
	Result EndGlobalSection();

	/* Exports section */
	Result BeginExportSection(Offset size);
	Result OnExportCount(Index count);
	Result OnExport(Index index, ExternalKind kind, Index item_index, StringView name);
	Result EndExportSection();

	/* Start section */
	Result BeginStartSection(Offset size);
	Result OnStartFunction(Index func_index);
	Result EndStartSection();

	/* Code section */
	Result BeginCodeSection(Offset size);
	Result OnFunctionBodyCount(Index count);
	Result BeginFunctionBody(Index index);
	Result OnLocalDeclCount(Index count);
	Result OnLocalDecl(Index decl_index, Index count, Type type);

	/* Function expressions; called between BeginFunctionBody and
	 EndFunctionBody */
	Result OnAtomicLoadExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnAtomicStoreExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnAtomicRmwExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnAtomicRmwCmpxchgExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnAtomicWaitExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnAtomicWakeExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnBinaryExpr(Opcode opcode);
	Result OnBlockExpr(Index num_types, Type* sig_types);
	Result OnBrExpr(Index depth);
	Result OnBrIfExpr(Index depth);
	Result OnBrTableExpr(Index num_targets, Index* target_depths, Index default_target_depth);
	Result OnCallExpr(Index func_index);
	Result OnCallIndirectExpr(Index sig_index);
	Result OnCatchExpr(Index except_index);
	Result OnCatchAllExpr();
	Result OnCompareExpr(Opcode opcode);
	Result OnConvertExpr(Opcode opcode);
	Result OnCurrentMemoryExpr();
	Result OnDropExpr();
	Result OnElseExpr();
	Result OnEndExpr();
	Result OnEndFunc();
	Result OnF32ConstExpr(uint32_t value_bits);
	Result OnF64ConstExpr(uint64_t value_bits);
	Result OnGetGlobalExpr(Index global_index);
	Result OnGetLocalExpr(Index local_index);
	Result OnGrowMemoryExpr();
	Result OnI32ConstExpr(uint32_t value);
	Result OnI64ConstExpr(uint64_t value);
	Result OnIfExpr(Index num_types, Type* sig_types);
	Result OnLoadExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnLoopExpr(Index num_types, Type* sig_types);
	Result OnRethrowExpr(Index depth);
	Result OnReturnExpr();
	Result OnSelectExpr();
	Result OnSetGlobalExpr(Index global_index);
	Result OnSetLocalExpr(Index local_index);
	Result OnStoreExpr(Opcode opcode, uint32_t alignment_log2, Address offset);
	Result OnTeeLocalExpr(Index local_index);
	Result OnThrowExpr(Index except_index);
	Result OnTryExpr(Index num_types, Type* sig_types);

	Result OnUnaryExpr(Opcode opcode);
	Result OnUnreachableExpr();
	Result EndFunctionBody(Index index);
	Result EndCodeSection();

	/* Elem section */
	Result BeginElemSection(Offset size);
	Result OnElemSegmentCount(Index count);
	Result BeginElemSegment(Index index, Index table_index);
	Result BeginElemSegmentInitExpr(Index index);
	Result EndElemSegmentInitExpr(Index index);
	Result OnElemSegmentFunctionIndexCount(Index index, Index count);
	Result OnElemSegmentFunctionIndex(Index segment_index, Index func_index);
	Result EndElemSegment(Index index);
	Result EndElemSection();

	/* Data section */
	Result BeginDataSection(Offset size);
	Result OnDataSegmentCount(Index count);
	Result BeginDataSegment(Index index, Index memory_index);
	Result BeginDataSegmentInitExpr(Index index);
	Result EndDataSegmentInitExpr(Index index);
	Result OnDataSegmentData(Index index, const void* data, Address size);
	Result EndDataSegment(Index index);
	Result EndDataSection();

	/* Names section */
	Result BeginNamesSection(Offset size);
	Result OnFunctionNameSubsection(Index index, uint32_t name_type, Offset subsection_size);
	Result OnFunctionNamesCount(Index num_functions);
	Result OnFunctionName(Index function_index, StringView function_name);
	Result OnLocalNameSubsection(Index index, uint32_t name_type, Offset subsection_size);
	Result OnLocalNameFunctionCount(Index num_functions);
	Result OnLocalNameLocalCount(Index function_index, Index num_locals);
	Result OnLocalName(Index function_index, Index local_index, StringView local_name);
	Result EndNamesSection();

	/* Reloc section */
	Result BeginRelocSection(Offset size);
	Result OnRelocCount(Index count, BinarySection section_code, StringView section_name);
	Result OnReloc(RelocType type, Offset offset, Index index, uint32_t addend);
	Result EndRelocSection();

	/* Linking section */
	Result BeginLinkingSection(Offset size);
	Result OnStackGlobal(Index stack_global);
	Result OnSymbolInfoCount(Index count);
	Result OnSymbolInfo(StringView name, uint32_t flags);
	Result OnDataSize(uint32_t data_size);
	Result OnDataAlignment(uint32_t data_alignment);
	Result OnSegmentInfoCount(Index count);
	Result OnSegmentInfo(Index index, StringView name, uint32_t alignment, uint32_t flags);
	Result EndLinkingSection();

	/* Exception section */
	Result BeginExceptionSection(Offset size);
	Result OnExceptionCount(Index count);
	Result OnExceptionType(Index index, TypeVector& sig);
	Result EndExceptionSection();

	/* InitExpr - used by elem, data and global sections; these functions are
	 * only called between calls to Begin*InitExpr and End*InitExpr */
	Result OnInitExprF32ConstExpr(Index index, uint32_t value);
	Result OnInitExprF64ConstExpr(Index index, uint64_t value);
	Result OnInitExprGetGlobalExpr(Index index, Index global_index);
	Result OnInitExprI32ConstExpr(Index index, uint32_t value);
	Result OnInitExprI64ConstExpr(Index index, uint64_t value);

protected:
	class BinaryReader;

	void EmitOpcodeValue(Opcode opcode, uint32_t, uint32_t = 0);
	void EmitOpcodeValue(Opcode opcode, uint64_t);

	void PushLabel(Index results, Index stack, Index position, Index origin = kInvalidIndex);
	void PopLabel(Index position);

	Environment *_env = nullptr;
	Module *_targetModule = nullptr;
	const ReadOptions *_options = nullptr;
	ReaderState _state;
	TypedValue _initExprValue;

	Index _currentIndex = kInvalidIndex;
	Func *_currentFunc = nullptr;
	TypeChecker _typechecker;
	Vector<Func::OpcodeRec> _opcodes;
	Vector<Func::Label> _labels;
	Vector<Index> _labelStack;
};

template <typename Callback>
inline void ModuleReader::PushErrorStream(const Callback &cb) {
	StringStream stream;
	cb(stream);
	OnError(stream);
}

}

#endif /* WABT_BINARY_H_ */
