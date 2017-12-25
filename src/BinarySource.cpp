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

#include "Binary.h"
#include "Module.h"
#include "Environment.h"

#define PRINT_CONTENT 0

#if (PRINT_CONTENT)
#define BINARY_PRINTF(...) printf(__VA_ARGS__)
#else
#define BINARY_PRINTF
#endif

#define CHECK_RESULT(expr) do { if (expr == ::wasm::Result::Error) { return ::wasm::Result::Error; } } while (0)

namespace wasm {

static Result CheckHasMemory(ModuleReader *reader, Module *module, Opcode opcode) {
	if (!module->hasMemory()) {
		reader->PushErrorStream([&] (StringStream &stream) {
			stream << opcode.GetName() << " requires an imported or defined memory.";
		});
		return Result::Error;
	}
	return Result::Ok;
}

static Result CheckAlign(ModuleReader *reader, uint32_t alignment_log2, Address natural_alignment) {
	if (alignment_log2 >= 32 || (1U << alignment_log2) > natural_alignment) {
		reader->PushErrorStream([&] (StringStream &stream) {
			stream << "Alignment must not be larger than natural alignment (" << natural_alignment << ")";
		});
		return Result::Error;
	}
	return Result::Ok;
}

static Result CheckAtomicAlign(ModuleReader *reader, uint32_t alignment_log2, Address natural_alignment) {
	if (alignment_log2 >= 32 || (1U << alignment_log2) != natural_alignment) {
		reader->PushErrorStream([&] (StringStream &stream) {
			stream << "Alignment must be equal to natural alignment (" << natural_alignment << ")";
		});
		return Result::Error;
	}
	return Result::Ok;
}

static Result CheckLocal(ModuleReader *reader, Func *func, Index local_index) {
	Index locals_count = func->types.size();
	if (local_index >= locals_count) {
		reader->PushErrorStream([&] (StringStream &stream) {
			stream << "invalid local_index: " << local_index << " (max " << locals_count << ")";
		});
		return Result::Error;
	}
	return Result::Ok;
}

static Result CheckGlobal(ModuleReader *reader, Module *module, Index global_index) {
	auto & idx = module->getGlobalIndexVec();
	if (global_index >= idx.size()) {
		reader->PushErrorStream([&] (StringStream &stream) {
			stream << "invalid global_index: " << global_index << " (max " << idx.size() << ")";
		});
		return Result::Error;
	}
	return Result::Ok;
}


bool ModuleReader::OnError(StringStream & message) {
	if (!_env) {
		BINARY_PRINTF("%s\n", message.str().data());
	} else {
		_env->onError("Reader", message);
	}
	return true;
}

/* Module */
Result ModuleReader::BeginModule(uint32_t version) { return Result::Ok; }
Result ModuleReader::EndModule() { return Result::Ok; }

/* Custom section */
Result ModuleReader::BeginCustomSection(Offset size, const StringView &section_name) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::EndCustomSection() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	return Result::Ok;
}

/* Code section */
Result ModuleReader::BeginCodeSection(Offset size) { return Result::Ok; }
Result ModuleReader::OnFunctionBodyCount(Index count) { return Result::Ok; }
Result ModuleReader::BeginFunctionBody(Index index) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, index);
	if (index >= _targetModule->_funcs.size()) {
		PushErrorStream([&] (StringStream &stream) { stream << "Invalid function code index: " << index; });
		return Result::Error;
	}
	_currentFunc = &_targetModule->_funcs[index];
	for (auto &it : _currentFunc->sig->params) {
		_currentFunc->types.emplace_back(it);
	}

	CHECK_RESULT(_typechecker.BeginFunction(&_currentFunc->sig->results));
	PushLabel(_currentFunc->sig->results.size(), 0, kInvalidIndex);

	return Result::Ok;
}
Result ModuleReader::EndFunctionBody(Index index) {
	CHECK_RESULT(_typechecker.EndFunction());
	PopLabel(_opcodes.size());

	_currentFunc->opcodes = _opcodes;
	for (auto &it : _currentFunc->opcodes) {
		switch (it.opcode) {
		case Opcode::Br:
		case Opcode::BrIf:
		case Opcode::BrTable:
		case Opcode::If:
		case Opcode::Else:
			if (it.value32.v2 == kInvalidIndex) {
				it.value32.v2 = _labels[it.value32.v1].offset;
			}
			break;
		}
	}

	_opcodes.clear();
	_labels.clear();
	_jumpTable.clear();

	BINARY_PRINTF("%s\n", __FUNCTION__);
	return Result::Ok;
}
Result ModuleReader::EndCodeSection() { return Result::Ok; }

Result ModuleReader::OnLocalDeclCount(Index count) { return Result::Ok; }
Result ModuleReader::OnLocalDecl(Index decl_index, Index count, Type type) {
	BINARY_PRINTF("%s %u %u\n", __FUNCTION__, decl_index, count);

	for (Index i = 0; i < count; ++i) {
		_currentFunc->types.emplace_back(type);
	}

	return Result::Ok;
}


/* Function expressions; called between BeginFunctionBody and
 EndFunctionBody */

Result ModuleReader::OnAtomicLoadExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAtomicAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnAtomicLoad(opcode));
	EmitOpcodeValue(opcode, 0, offset); // memory_index offset
	return Result::Ok;
}
Result ModuleReader::OnAtomicStoreExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAtomicAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnAtomicStore(opcode));
	EmitOpcodeValue(opcode, 0, offset); // memory_index offset
	return Result::Ok;
}
Result ModuleReader::OnAtomicRmwExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAtomicAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnAtomicRmw(opcode));
	EmitOpcodeValue(opcode, 0, offset); // memory_index offset
	return Result::Ok;
}
Result ModuleReader::OnAtomicRmwCmpxchgExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAtomicAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnAtomicRmwCmpxchg(opcode));
	EmitOpcodeValue(opcode, 0, offset);
	return Result::Ok;
}
Result ModuleReader::OnAtomicWaitExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAtomicAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnAtomicWait(opcode));
	EmitOpcodeValue(opcode, 0, offset);
	return Result::Error;
}
Result ModuleReader::OnAtomicWakeExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAtomicAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnAtomicWake(opcode));
	EmitOpcodeValue(opcode, 0, offset);
	return Result::Error;
}

Result ModuleReader::OnUnaryExpr(Opcode opcode) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnUnary(opcode));
	EmitOpcodeValue(opcode, 0, 0);
	return Result::Ok;
}
Result ModuleReader::OnBinaryExpr(Opcode opcode) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnBinary(opcode));
	EmitOpcodeValue(opcode, 0, 0);
	return Result::Ok;
}

Result ModuleReader::OnBlockExpr(Index num_types, Type* sig_types) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, num_types);
	TypeVector sig(sig_types, sig_types + num_types);
	CHECK_RESULT(_typechecker.OnBlock(&sig));
	PushLabel(num_types, _typechecker.type_stack_size(), kInvalidIndex);
	return Result::Ok;
}
Result ModuleReader::OnBrExpr(Index depth) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, depth);
	CHECK_RESULT(_typechecker.OnBr(depth));
	EmitOpcodeValue(Opcode::Br, _labelStack.at(_labelStack.size() - 1 - depth), kInvalidIndex);
	return Result::Ok;
}
Result ModuleReader::OnBrIfExpr(Index depth) {
	BINARY_PRINTF("%s %u\n", __FUNCTION__, depth);
	CHECK_RESULT(_typechecker.OnBrIf(depth));
	EmitOpcodeValue(Opcode::BrIf, _labelStack.at(_labelStack.size() - 1 - depth), kInvalidIndex);
	return Result::Ok;
}
Result ModuleReader::OnBrTableExpr(Index num_targets, Index* target_depths, Index default_target_depth) {
	BINARY_PRINTF("%s\n", __FUNCTION__);

	auto size = _labelStack.size();
	//auto target = _jumpTable.size();
	//_jumpTable.push_back(num_targets);

	EmitOpcodeValue(Opcode::BrTable, num_targets, 0);

	CHECK_RESULT(_typechecker.BeginBrTable());
	for (Index i = 0; i < num_targets; ++i) {
		EmitOpcodeValue(Opcode::BrTable, _labelStack[size - 1 - target_depths[i]], kInvalidIndex);
		CHECK_RESULT(_typechecker.OnBrTableTarget(target_depths[i]));
	}
	EmitOpcodeValue(Opcode::BrTable, _labelStack[size - 1 - default_target_depth], kInvalidIndex);
	CHECK_RESULT(_typechecker.OnBrTableTarget(default_target_depth));
	CHECK_RESULT(_typechecker.EndBrTable());

	return Result::Ok;
}
Result ModuleReader::OnLoopExpr(Index num_types, Type* sig_types) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	TypeVector sig(sig_types, sig_types + num_types);
	CHECK_RESULT(_typechecker.OnLoop(&sig));
	PushLabel(num_types, _typechecker.type_stack_size(), _opcodes.size());
	return Result::Ok;
}
Result ModuleReader::OnIfExpr(Index num_types, Type* sig_types) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	TypeVector sig(sig_types, sig_types + num_types);
	CHECK_RESULT(_typechecker.OnIf(&sig));
	PushLabel(num_types, _typechecker.type_stack_size(), kInvalidIndex, _opcodes.size());
	EmitOpcodeValue(Opcode::If, _labelStack.back(), kInvalidIndex);
	return Result::Ok;
}
Result ModuleReader::OnElseExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnElse());
	auto backLabel = _labels.at(_labelStack.back());
	EmitOpcodeValue(Opcode::Else, _labelStack.back(), kInvalidIndex);
	_opcodes[backLabel.origin].value32.v2 = _opcodes.size();
	//PopLabel(_opcodes.size() + 1);
	//PushLabel(backLabel.results, backLabel.stack, kInvalidIndex);
	return Result::Ok;
}
Result ModuleReader::OnEndExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnEnd());
	auto &backLabel = _labels[_labelStack.back()];
	PopLabel(_opcodes.size());
	EmitOpcodeValue(Opcode::End, backLabel.stack, backLabel.results);
	return Result::Ok;
}
Result ModuleReader::OnDropExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnDrop());
	EmitOpcodeValue(Opcode::Drop, 0, 0);
	return Result::Ok;
}

Result ModuleReader::OnCallExpr(Index func_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);

	auto sig = _targetModule->getFuncSignature(func_index);
	if (!sig.first) {
		return Result::Error;
	}

	CHECK_RESULT(_typechecker.OnCall(&sig.first->params, &sig.first->results));
	EmitOpcodeValue(Opcode::Call, func_index, uint32_t(sig.second));
	return Result::Ok;
}
Result ModuleReader::OnCallIndirectExpr(Index sig_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	if (!_targetModule->hasTable()) {
		PushErrorStream([&] (StringStream &stream) { stream << "found call_indirect operator, but no table"; });
		return Result::Error;
	}

	Module::Signature* sig = _targetModule->getSignature(sig_index);
	CHECK_RESULT(_typechecker.OnCallIndirect(&sig->params, &sig->results));
	EmitOpcodeValue(Opcode::CallIndirect, sig_index, 0); // sig index, table index
	return Result::Ok;
}

Result ModuleReader::OnCompareExpr(Opcode opcode) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	return OnBinaryExpr(opcode);
}
Result ModuleReader::OnConvertExpr(Opcode opcode) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	return OnUnaryExpr(opcode);
}

Result ModuleReader::OnCurrentMemoryExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, Opcode::CurrentMemory));
	CHECK_RESULT(_typechecker.OnCurrentMemory());
	EmitOpcodeValue(Opcode::CurrentMemory, 0, 0); // memory_index
	return Result::Ok;
}

Result ModuleReader::OnI32ConstExpr(uint32_t value) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnConst(Type::I32));
	EmitOpcodeValue(Opcode::I32Const, value);
	return Result::Ok;
}
Result ModuleReader::OnI64ConstExpr(uint64_t value) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnConst(Type::I64));
	EmitOpcodeValue(Opcode::I64Const, value);
	return Result::Ok;
}
Result ModuleReader::OnF32ConstExpr(uint32_t value_bits) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnConst(Type::F32));
	EmitOpcodeValue(Opcode::F32Const, value_bits);
	return Result::Ok;
}
Result ModuleReader::OnF64ConstExpr(uint64_t value_bits) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnConst(Type::F64));
	EmitOpcodeValue(Opcode::F64Const, value_bits);
	return Result::Ok;
}

Result ModuleReader::OnGetGlobalExpr(Index global_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckGlobal(this, _targetModule, global_index));
	auto type = _targetModule->getGlobalType(global_index);
	if (type.first == Type::Void) {
		return Result::Error;
	}

	CHECK_RESULT(_typechecker.OnGetGlobal(type.first));
	EmitOpcodeValue(Opcode::GetGlobal, global_index);
	return Result::Ok;
}
Result ModuleReader::OnSetGlobalExpr(Index global_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckGlobal(this, _targetModule, global_index));
	auto type = _targetModule->getGlobalType(global_index);
	if (type.first == Type::Void) {
		return Result::Error;
	}

	if (!type.second) {
		PushErrorStream([&] (StringStream &stream) {
			stream << "can't set_global on immutable global at index " << global_index;
		});
		return Result::Error;
	}
	CHECK_RESULT(_typechecker.OnSetGlobal(type.first));
	EmitOpcodeValue(Opcode::SetGlobal, global_index);
	return Result::Ok;
}

Result ModuleReader::OnGetLocalExpr(Index local_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckLocal(this, _currentFunc, local_index));
	Type type = _currentFunc->types[local_index];
	CHECK_RESULT(_typechecker.OnGetLocal(type));
	EmitOpcodeValue(Opcode::GetLocal, local_index);
	return Result::Ok;
}
Result ModuleReader::OnSetLocalExpr(Index local_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckLocal(this, _currentFunc, local_index));
	Type type = _currentFunc->types[local_index];
	CHECK_RESULT(_typechecker.OnSetLocal(type));
	EmitOpcodeValue(Opcode::SetLocal, local_index);
	return Result::Ok;
}

Result ModuleReader::OnGrowMemoryExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, Opcode::GrowMemory));
	CHECK_RESULT(_typechecker.OnGrowMemory());
	EmitOpcodeValue(Opcode::GrowMemory, 0, 0); // memory index
	return Result::Ok;
}

Result ModuleReader::OnLoadExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnLoad(opcode));
	EmitOpcodeValue(opcode, offset);
	return Result::Ok;
}
Result ModuleReader::OnStoreExpr(Opcode opcode, uint32_t alignment_log2, Address offset) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckHasMemory(this, _targetModule, opcode));
	CHECK_RESULT(CheckAlign(this, alignment_log2, opcode.GetMemorySize()));
	CHECK_RESULT(_typechecker.OnStore(opcode));
	EmitOpcodeValue(opcode, offset);
	return Result::Ok;
}


Result ModuleReader::OnTeeLocalExpr(Index local_index) {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(CheckLocal(this, _currentFunc, local_index));
	Type type = _currentFunc->types[local_index];
	CHECK_RESULT(_typechecker.OnTeeLocal(type));
	EmitOpcodeValue(Opcode::TeeLocal, local_index);
	return Result::Ok;
}

Result ModuleReader::OnReturnExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	Index drop_count, keep_count;
	CHECK_RESULT(_typechecker.OnReturn());
	EmitOpcodeValue(Opcode::Return, _currentFunc->sig->results.size()); // return count
	return Result::Ok;
}

Result ModuleReader::OnSelectExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnSelect());
	EmitOpcodeValue(Opcode::Select, 0, 0);
	return Result::Ok;
}

Result ModuleReader::OnUnreachableExpr() {
	BINARY_PRINTF("%s\n", __FUNCTION__);
	CHECK_RESULT(_typechecker.OnUnreachable());
	EmitOpcodeValue(Opcode::Unreachable, 0, 0);
	return Result::Ok;
}

Result ModuleReader::OnCatchExpr(Index except_index) { return Result::Ok; }
Result ModuleReader::OnCatchAllExpr() { return Result::Ok; }
Result ModuleReader::OnRethrowExpr(Index depth) { return Result::Ok; }
Result ModuleReader::OnThrowExpr(Index except_index) { return Result::Ok; }
Result ModuleReader::OnTryExpr(Index num_types, Type* sig_types) { return Result::Ok; }
Result ModuleReader::OnEndFunc() { return Result::Ok; }

void ModuleReader::EmitOpcodeValue(Opcode opcode, uint32_t value1, uint32_t value2) {
	_opcodes.emplace_back(opcode, value1, value2);
}
void ModuleReader::EmitOpcodeValue(Opcode opcode, uint64_t value) {
	_opcodes.emplace_back(opcode, value);
}


void ModuleReader::PushLabel(Index results, Index stack, Index position, Index origin) {
	_labels.emplace_back(Func::Label{results, stack, position, origin});
	_labelStack.push_back(_labels.size() - 1);
}

void ModuleReader::PopLabel(Index position) {
	auto stackId = _labelStack.back();
	if (_labels[stackId].offset == kInvalidIndex) {
		_labels[stackId].offset = position;
	}
	_labelStack.pop_back();
}

}
