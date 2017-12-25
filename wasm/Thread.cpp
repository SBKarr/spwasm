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

#include "Thread.h"
#include "Opcode.h"
#include "Environment.h"
#include <cmath>
#include <type_traits>
#include <string.h>

#include "ThreadOps.cc"
#include "ThreadUtils.cc"

namespace wasm {

Thread::Result Thread::Run(Index stackMax) {
	Result result = Result::Ok;
	while (_callStackTop > stackMax) {
		TrySync();
		_currentFrame = &_callStack[_callStackTop - 1];
		const auto func = _currentFrame->func;
		const auto module = _currentFrame->module;
		auto locals = _currentFrame->locals;

		auto data = func->opcodes.data();
		auto &it = _currentFrame->position;
		auto end = data + func->opcodes.size();
		while (it != end) {
			const auto opcode = it->opcode;
			switch (opcode) {
			case Opcode::Select: {
				uint32_t cond = Pop<uint32_t>();
				Value false_ = Pop();
				Value true_ = Pop();
				CHECK_TRAP(Push(cond ? true_ : false_));
				break;
			}

			case Opcode::Br:
			case Opcode::Else: {
				it = data + it->value32.v2;
				continue;
				break;
			}

			case Opcode::BrIf: {
				if (Pop<uint32_t>()) {
					it = data + it->value32.v2;
					continue;
				}
				break;
			}

			case Opcode::BrTable: {
				const Index num_targets = it->value32.v1;
				const uint32_t key = Pop<uint32_t>();
				auto target = it + ((key >= num_targets ? num_targets : key) + 1);
				it = data + target->value32.v2;
				continue;
				break;
			}

			case Opcode::If:
				if (!Pop<uint32_t>()) {
					it = data + it->value32.v2;
					continue;
				}
				break;

			case Opcode::End:
				StoreResult(locals + func->types.size(), it->value32.v1, it->value32.v2);
				break;

			case Opcode::Return:
				if (_callStackTop <= stackMax) {
					result = Result::Returned;
				}
				PopCall(it->value32.v1);
				it = end;
				goto exit_opcode_loop;
				break;

			case Opcode::Unreachable:
				TRAP(Unreachable);
				break;

			case Opcode::I32Const:
				CHECK_TRAP(Push<uint32_t>(it->value32.v1));
				break;

			case Opcode::I64Const:
				CHECK_TRAP(Push<uint64_t>(it->value64));
				break;

			case Opcode::F32Const:
				CHECK_TRAP(PushRep<float>(it->value32.v1));
				break;

			case Opcode::F64Const:
				CHECK_TRAP(PushRep<double>(it->value64));
				break;

			case Opcode::InterpGetStack:
				CHECK_TRAP(Push(_userStackPointer));
				break;

			case Opcode::InterpSetStack: {
				auto value = Pop();
				if (value.i32 < _userStackGuard) {
					TRAP(UserStackExhausted);
				} else {
					_userStackPointer = value.i32;
				}
				break;
			}

			case Opcode::GetGlobal:
				CHECK_TRAP(Push(module->globals[it->value32.v1]->value.value));
				break;

			case Opcode::SetGlobal:
				module->globals[it->value32.v1]->value.value = Pop();
				break;

			case Opcode::GetLocal:
				CHECK_TRAP(Push(locals[it->value32.v1]));
				break;

			case Opcode::SetLocal:
				locals[it->value32.v1] = Pop();
				break;

			case Opcode::TeeLocal:
				locals[it->value32.v1] = Top();
				break;

			case Opcode::Call: {
				auto result = PushCall(*module, it->value32.v1, it->value32.v2);
				switch (result) {
				case Result::Ok: ++ it; goto exit_opcode_loop; break;
				case Result::Returned: break;
				default: return result; break;
				}
				break;
			}

			case Opcode::CallIndirect: {
				RuntimeTable* table = module->tables[it->value32.v2];
				auto reqSig = module->module->getSignature(it->value32.v1);
				Index entry_index = Pop<uint32_t>();
				TRAP_IF(entry_index >= table->values.size(), UndefinedTableIndex);
				Index func_index = table->values[entry_index].i32;
				TRAP_IF(func_index == kInvalidIndex, UninitializedTableElement);
				auto sig = module->module->getFuncSignature(func_index);
				TRAP_IF(sig.first == nullptr, IndirectCallSignatureMismatch);
				TRAP_UNLESS(_runtime->isSignatureMatch(*sig.first, *reqSig), IndirectCallSignatureMismatch);
				auto result = PushCall(*module, func_index, sig.second);
				switch (result) {
				case Result::Ok: ++ it; goto exit_opcode_loop; break;
				case Result::Returned: break;
				default: return result; break;
				}
				break;
			}

			case Opcode::I32Load8S:
				CHECK_TRAP(Load<int8_t, uint32_t>(it));
				break;

			case Opcode::I32Load8U:
				CHECK_TRAP(Load<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32Load16S:
				CHECK_TRAP(Load<int16_t, uint32_t>(it));
				break;

			case Opcode::I32Load16U:
				CHECK_TRAP(Load<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64Load8S:
				CHECK_TRAP(Load<int8_t, uint64_t>(it));
				break;

			case Opcode::I64Load8U:
				CHECK_TRAP(Load<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64Load16S:
				CHECK_TRAP(Load<int16_t, uint64_t>(it));
				break;

			case Opcode::I64Load16U:
				CHECK_TRAP(Load<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64Load32S:
				CHECK_TRAP(Load<int32_t, uint64_t>(it));
				break;

			case Opcode::I64Load32U:
				CHECK_TRAP(Load<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32Load:
				CHECK_TRAP(Load<uint32_t>(it));
				break;

			case Opcode::I64Load:
				CHECK_TRAP(Load<uint64_t>(it));
				break;

			case Opcode::F32Load:
				CHECK_TRAP(Load<float>(it));
				break;

			case Opcode::F64Load:
				CHECK_TRAP(Load<double>(it));
				break;

			case Opcode::I32Store8:
				CHECK_TRAP(Store<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32Store16:
				CHECK_TRAP(Store<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64Store8:
				CHECK_TRAP(Store<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64Store16:
				CHECK_TRAP(Store<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64Store32:
				CHECK_TRAP(Store<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32Store:
				CHECK_TRAP(Store<uint32_t>(it));
				break;

			case Opcode::I64Store:
				CHECK_TRAP(Store<uint64_t>(it));
				break;

			case Opcode::F32Store:
				CHECK_TRAP(Store<float>(it));
				break;

			case Opcode::F64Store:
				CHECK_TRAP(Store<double>(it));
				break;

			case Opcode::I32AtomicLoad8U:
				CHECK_TRAP(AtomicLoad<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32AtomicLoad16U:
				CHECK_TRAP(AtomicLoad<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicLoad8U:
				CHECK_TRAP(AtomicLoad<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicLoad16U:
				CHECK_TRAP(AtomicLoad<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicLoad32U:
				CHECK_TRAP(AtomicLoad<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32AtomicLoad:
				CHECK_TRAP(AtomicLoad<uint32_t>(it));
				break;

			case Opcode::I64AtomicLoad:
				CHECK_TRAP(AtomicLoad<uint64_t>(it));
				break;

			case Opcode::I32AtomicStore8:
				CHECK_TRAP(AtomicStore<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32AtomicStore16:
				CHECK_TRAP(AtomicStore<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicStore8:
				CHECK_TRAP(AtomicStore<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicStore16:
				CHECK_TRAP(AtomicStore<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicStore32:
				CHECK_TRAP(AtomicStore<uint32_t, uint64_t>(it));
				break;

			case Opcode::I32AtomicStore:
				CHECK_TRAP(AtomicStore<uint32_t>(it));
				break;

			case Opcode::I64AtomicStore:
				CHECK_TRAP(AtomicStore<uint64_t>(it));
				break;

#define ATOMIC_RMW(rmwop, func)                                     \
			case Opcode::I32AtomicRmw##rmwop:                                 \
				CHECK_TRAP(AtomicRmw<uint32_t, uint32_t>(func<uint32_t>, it)); \
				break;                                                          \
			case Opcode::I64AtomicRmw##rmwop:                                 \
				CHECK_TRAP(AtomicRmw<uint64_t, uint64_t>(func<uint64_t>, it)); \
				break;                                                          \
			case Opcode::I32AtomicRmw8U##rmwop:                               \
				CHECK_TRAP(AtomicRmw<uint8_t, uint32_t>(func<uint32_t>, it));  \
				break;                                                          \
			case Opcode::I32AtomicRmw16U##rmwop:                              \
				CHECK_TRAP(AtomicRmw<uint16_t, uint32_t>(func<uint32_t>, it)); \
				break;                                                          \
			case Opcode::I64AtomicRmw8U##rmwop:                               \
				CHECK_TRAP(AtomicRmw<uint8_t, uint64_t>(func<uint64_t>, it));  \
				break;                                                          \
			case Opcode::I64AtomicRmw16U##rmwop:                              \
				CHECK_TRAP(AtomicRmw<uint16_t, uint64_t>(func<uint64_t>, it)); \
				break;                                                          \
			case Opcode::I64AtomicRmw32U##rmwop:                              \
				CHECK_TRAP(AtomicRmw<uint32_t, uint64_t>(func<uint64_t>, it)); \
				break /* no semicolon */

			ATOMIC_RMW(Add, Add);
			ATOMIC_RMW(Sub, Sub);
			ATOMIC_RMW(And, IntAnd);
			ATOMIC_RMW(Or, IntOr);
			ATOMIC_RMW(Xor, IntXor);
			ATOMIC_RMW(Xchg, Xchg);

#undef ATOMIC_RMW

			case Opcode::I32AtomicRmwCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint32_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicRmwCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint64_t, uint64_t>(it));
				break;

			case Opcode::I32AtomicRmw8UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint8_t, uint32_t>(it));
				break;

			case Opcode::I32AtomicRmw16UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint16_t, uint32_t>(it));
				break;

			case Opcode::I64AtomicRmw8UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint8_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicRmw16UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint16_t, uint64_t>(it));
				break;

			case Opcode::I64AtomicRmw32UCmpxchg:
				CHECK_TRAP(AtomicRmwCmpxchg<uint32_t, uint64_t>(it));
				break;

			case Opcode::CurrentMemory:
				CHECK_TRAP(Push<uint32_t>(module->memory[it->value32.v1]->limits.initial));
				break;

			case Opcode::GrowMemory: {
				auto mem = module->memory[it->value32.v1];
				uint32_t old_page_size = mem->limits.initial;
				if (!GrowMemory(mem, Pop<uint32_t>())) {
					CHECK_TRAP(Push<int32_t>(-1));
					break;
				}
				CHECK_TRAP(Push<uint32_t>(old_page_size));
				break;
			}

			case Opcode::I32Add:
				CHECK_TRAP(Binop(Add<uint32_t>));
				break;

			case Opcode::I32Sub:
				CHECK_TRAP(Binop(Sub<uint32_t>));
				break;

			case Opcode::I32Mul:
				CHECK_TRAP(Binop(Mul<uint32_t>));
				break;

			case Opcode::I32DivS:
				CHECK_TRAP(BinopTrap(IntDivS<int32_t>));
				break;

			case Opcode::I32DivU:
				CHECK_TRAP(BinopTrap(IntDivU<uint32_t>));
				break;

			case Opcode::I32RemS:
				CHECK_TRAP(BinopTrap(IntRemS<int32_t>));
				break;

			case Opcode::I32RemU:
				CHECK_TRAP(BinopTrap(IntRemU<uint32_t>));
				break;

			case Opcode::I32And:
				CHECK_TRAP(Binop(IntAnd<uint32_t>));
				break;

			case Opcode::I32Or:
				CHECK_TRAP(Binop(IntOr<uint32_t>));
				break;

			case Opcode::I32Xor:
				CHECK_TRAP(Binop(IntXor<uint32_t>));
				break;

			case Opcode::I32Shl:
				CHECK_TRAP(Binop(IntShl<uint32_t>));
				break;

			case Opcode::I32ShrU:
				CHECK_TRAP(Binop(IntShr<uint32_t>));
				break;

			case Opcode::I32ShrS:
				CHECK_TRAP(Binop(IntShr<int32_t>));
				break;

			case Opcode::I32Eq:
				CHECK_TRAP(Binop(Eq<uint32_t>));
				break;

			case Opcode::I32Ne:
				CHECK_TRAP(Binop(Ne<uint32_t>));
				break;

			case Opcode::I32LtS:
				CHECK_TRAP(Binop(Lt<int32_t>));
				break;

			case Opcode::I32LeS:
				CHECK_TRAP(Binop(Le<int32_t>));
				break;

			case Opcode::I32LtU:
				CHECK_TRAP(Binop(Lt<uint32_t>));
				break;

			case Opcode::I32LeU:
				CHECK_TRAP(Binop(Le<uint32_t>));
				break;

			case Opcode::I32GtS:
				CHECK_TRAP(Binop(Gt<int32_t>));
				break;

			case Opcode::I32GeS:
				CHECK_TRAP(Binop(Ge<int32_t>));
				break;

			case Opcode::I32GtU:
				CHECK_TRAP(Binop(Gt<uint32_t>));
				break;

			case Opcode::I32GeU:
				CHECK_TRAP(Binop(Ge<uint32_t>));
				break;

			case Opcode::I32Clz:
				CHECK_TRAP(Push<uint32_t>(Clz(Pop<uint32_t>())));
				break;

			case Opcode::I32Ctz:
				CHECK_TRAP(Push<uint32_t>(Ctz(Pop<uint32_t>())));
				break;

			case Opcode::I32Popcnt:
				CHECK_TRAP(Push<uint32_t>(Popcount(Pop<uint32_t>())));
				break;

			case Opcode::I32Eqz:
				CHECK_TRAP(Unop(IntEqz<uint32_t, uint32_t>));
				break;

			case Opcode::I64Add:
				CHECK_TRAP(Binop(Add<uint64_t>));
				break;

			case Opcode::I64Sub:
				CHECK_TRAP(Binop(Sub<uint64_t>));
				break;

			case Opcode::I64Mul:
				CHECK_TRAP(Binop(Mul<uint64_t>));
				break;

			case Opcode::I64DivS:
				CHECK_TRAP(BinopTrap(IntDivS<int64_t>));
				break;

			case Opcode::I64DivU:
				CHECK_TRAP(BinopTrap(IntDivU<uint64_t>));
				break;

			case Opcode::I64RemS:
				CHECK_TRAP(BinopTrap(IntRemS<int64_t>));
				break;

			case Opcode::I64RemU:
				CHECK_TRAP(BinopTrap(IntRemU<uint64_t>));
				break;

			case Opcode::I64And:
				CHECK_TRAP(Binop(IntAnd<uint64_t>));
				break;

			case Opcode::I64Or:
				CHECK_TRAP(Binop(IntOr<uint64_t>));
				break;

			case Opcode::I64Xor:
				CHECK_TRAP(Binop(IntXor<uint64_t>));
				break;

			case Opcode::I64Shl:
				CHECK_TRAP(Binop(IntShl<uint64_t>));
				break;

			case Opcode::I64ShrU:
				CHECK_TRAP(Binop(IntShr<uint64_t>));
				break;

			case Opcode::I64ShrS:
				CHECK_TRAP(Binop(IntShr<int64_t>));
				break;

			case Opcode::I64Eq:
				CHECK_TRAP(Binop(Eq<uint64_t>));
				break;

			case Opcode::I64Ne:
				CHECK_TRAP(Binop(Ne<uint64_t>));
				break;

			case Opcode::I64LtS:
				CHECK_TRAP(Binop(Lt<int64_t>));
				break;

			case Opcode::I64LeS:
				CHECK_TRAP(Binop(Le<int64_t>));
				break;

			case Opcode::I64LtU:
				CHECK_TRAP(Binop(Lt<uint64_t>));
				break;

			case Opcode::I64LeU:
				CHECK_TRAP(Binop(Le<uint64_t>));
				break;

			case Opcode::I64GtS:
				CHECK_TRAP(Binop(Gt<int64_t>));
				break;

			case Opcode::I64GeS:
				CHECK_TRAP(Binop(Ge<int64_t>));
				break;

			case Opcode::I64GtU:
				CHECK_TRAP(Binop(Gt<uint64_t>));
				break;

			case Opcode::I64GeU:
				CHECK_TRAP(Binop(Ge<uint64_t>));
				break;

			case Opcode::I64Clz:
				CHECK_TRAP(Push<uint64_t>(Clz(Pop<uint64_t>())));
				break;

			case Opcode::I64Ctz:
				CHECK_TRAP(Push<uint64_t>(Ctz(Pop<uint64_t>())));
				break;

			case Opcode::I64Popcnt:
				CHECK_TRAP(Push<uint64_t>(Popcount(Pop<uint64_t>())));
				break;

			case Opcode::F32Add:
				CHECK_TRAP(Binop(Add<float>));
				break;

			case Opcode::F32Sub:
				CHECK_TRAP(Binop(Sub<float>));
				break;

			case Opcode::F32Mul:
				CHECK_TRAP(Binop(Mul<float>));
				break;

			case Opcode::F32Div:
				CHECK_TRAP(Binop(FloatDiv<float>));
				break;

			case Opcode::F32Min:
				CHECK_TRAP(Binop(FloatMin<float>));
				break;

			case Opcode::F32Max:
				CHECK_TRAP(Binop(FloatMax<float>));
				break;

			case Opcode::F32Abs:
				CHECK_TRAP(Unop(FloatAbs<float>));
				break;

			case Opcode::F32Neg:
				CHECK_TRAP(Unop(FloatNeg<float>));
				break;

			case Opcode::F32Copysign:
				CHECK_TRAP(Binop(FloatCopySign<float>));
				break;

			case Opcode::F32Ceil:
				CHECK_TRAP(Unop(FloatCeil<float>));
				break;

			case Opcode::F32Floor:
				CHECK_TRAP(Unop(FloatFloor<float>));
				break;

			case Opcode::F32Trunc:
				CHECK_TRAP(Unop(FloatTrunc<float>));
				break;

			case Opcode::F32Nearest:
				CHECK_TRAP(Unop(FloatNearest<float>));
				break;

			case Opcode::F32Sqrt:
				CHECK_TRAP(Unop(FloatSqrt<float>));
				break;

			case Opcode::F32Eq:
				CHECK_TRAP(Binop(Eq<float>));
				break;

			case Opcode::F32Ne:
				CHECK_TRAP(Binop(Ne<float>));
				break;

			case Opcode::F32Lt:
				CHECK_TRAP(Binop(Lt<float>));
				break;

			case Opcode::F32Le:
				CHECK_TRAP(Binop(Le<float>));
				break;

			case Opcode::F32Gt:
				CHECK_TRAP(Binop(Gt<float>));
				break;

			case Opcode::F32Ge:
				CHECK_TRAP(Binop(Ge<float>));
				break;

			case Opcode::F64Add:
				CHECK_TRAP(Binop(Add<double>));
				break;

			case Opcode::F64Sub:
				CHECK_TRAP(Binop(Sub<double>));
				break;

			case Opcode::F64Mul:
				CHECK_TRAP(Binop(Mul<double>));
				break;

			case Opcode::F64Div:
				CHECK_TRAP(Binop(FloatDiv<double>));
				break;

			case Opcode::F64Min:
				CHECK_TRAP(Binop(FloatMin<double>));
				break;

			case Opcode::F64Max:
				CHECK_TRAP(Binop(FloatMax<double>));
				break;

			case Opcode::F64Abs:
				CHECK_TRAP(Unop(FloatAbs<double>));
				break;

			case Opcode::F64Neg:
				CHECK_TRAP(Unop(FloatNeg<double>));
				break;

			case Opcode::F64Copysign:
				CHECK_TRAP(Binop(FloatCopySign<double>));
				break;

			case Opcode::F64Ceil:
				CHECK_TRAP(Unop(FloatCeil<double>));
				break;

			case Opcode::F64Floor:
				CHECK_TRAP(Unop(FloatFloor<double>));
				break;

			case Opcode::F64Trunc:
				CHECK_TRAP(Unop(FloatTrunc<double>));
				break;

			case Opcode::F64Nearest:
				CHECK_TRAP(Unop(FloatNearest<double>));
				break;

			case Opcode::F64Sqrt:
				CHECK_TRAP(Unop(FloatSqrt<double>));
				break;

			case Opcode::F64Eq:
				CHECK_TRAP(Binop(Eq<double>));
				break;

			case Opcode::F64Ne:
				CHECK_TRAP(Binop(Ne<double>));
				break;

			case Opcode::F64Lt:
				CHECK_TRAP(Binop(Lt<double>));
				break;

			case Opcode::F64Le:
				CHECK_TRAP(Binop(Le<double>));
				break;

			case Opcode::F64Gt:
				CHECK_TRAP(Binop(Gt<double>));
				break;

			case Opcode::F64Ge:
				CHECK_TRAP(Binop(Ge<double>));
				break;

			case Opcode::I32TruncSF32:
				CHECK_TRAP(UnopTrap(IntTrunc<int32_t, float>));
				break;

			case Opcode::I32TruncSSatF32:
				CHECK_TRAP(Unop(IntTruncSat<int32_t, float>));
				break;

			case Opcode::I32TruncSF64:
				CHECK_TRAP(UnopTrap(IntTrunc<int32_t, double>));
				break;

			case Opcode::I32TruncSSatF64:
				CHECK_TRAP(Unop(IntTruncSat<int32_t, double>));
				break;

			case Opcode::I32TruncUF32:
				CHECK_TRAP(UnopTrap(IntTrunc<uint32_t, float>));
				break;

			case Opcode::I32TruncUSatF32:
				CHECK_TRAP(Unop(IntTruncSat<uint32_t, float>));
				break;

			case Opcode::I32TruncUF64:
				CHECK_TRAP(UnopTrap(IntTrunc<uint32_t, double>));
				break;

			case Opcode::I32TruncUSatF64:
				CHECK_TRAP(Unop(IntTruncSat<uint32_t, double>));
				break;

			case Opcode::I32WrapI64:
				CHECK_TRAP(Push<uint32_t>(Pop<uint64_t>()));
				break;

			case Opcode::I64TruncSF32:
				CHECK_TRAP(UnopTrap(IntTrunc<int64_t, float>));
				break;

			case Opcode::I64TruncSSatF32:
				CHECK_TRAP(Unop(IntTruncSat<int64_t, float>));
				break;

			case Opcode::I64TruncSF64:
				CHECK_TRAP(UnopTrap(IntTrunc<int64_t, double>));
				break;

			case Opcode::I64TruncSSatF64:
				CHECK_TRAP(Unop(IntTruncSat<int64_t, double>));
				break;

			case Opcode::I64TruncUF32:
				CHECK_TRAP(UnopTrap(IntTrunc<uint64_t, float>));
				break;

			case Opcode::I64TruncUSatF32:
				CHECK_TRAP(Unop(IntTruncSat<uint64_t, float>));
				break;

			case Opcode::I64TruncUF64:
				CHECK_TRAP(UnopTrap(IntTrunc<uint64_t, double>));
				break;

			case Opcode::I64TruncUSatF64:
				CHECK_TRAP(Unop(IntTruncSat<uint64_t, double>));
				break;

			case Opcode::I64ExtendSI32:
				CHECK_TRAP(Push<uint64_t>(Pop<int32_t>()));
				break;

			case Opcode::I64ExtendUI32:
				CHECK_TRAP(Push<uint64_t>(Pop<uint32_t>()));
				break;

			case Opcode::F32ConvertSI32:
				CHECK_TRAP(Push<float>(Pop<int32_t>()));
				break;

			case Opcode::F32ConvertUI32:
				CHECK_TRAP(Push<float>(Pop<uint32_t>()));
				break;

			case Opcode::F32ConvertSI64:
				CHECK_TRAP(Push<float>(Pop<int64_t>()));
				break;

			case Opcode::F32ConvertUI64:
				CHECK_TRAP(Push<float>(wabt_convert_uint64_to_float(Pop<uint64_t>())));
				break;

			case Opcode::F32DemoteF64: {
				typedef FloatTraits<float> F32Traits;
				typedef FloatTraits<double> F64Traits;

				uint64_t value = PopRep<double>();
				if (WABT_LIKELY((IsConversionInRange<float, double>(value)))) {
					CHECK_TRAP(Push<float>(FromRep<double>(value)));
				} else if (IsInRangeF64DemoteF32RoundToF32Max(value)) {
					CHECK_TRAP(PushRep<float>(F32Traits::kMax));
				} else if (IsInRangeF64DemoteF32RoundToNegF32Max(value)) {
					CHECK_TRAP(PushRep<float>(F32Traits::kNegMax));
				} else {
					uint32_t sign = (value >> 32) & F32Traits::kSignMask;
					uint32_t tag = 0;
					if (F64Traits::IsNan(value)) {
						tag = F32Traits::kQuietNanBit |
						((value >> (F64Traits::kSigBits - F32Traits::kSigBits)) &
								F32Traits::kSigMask);
					}
					CHECK_TRAP(PushRep<float>(sign | F32Traits::kInf | tag));
				}
				break;
			}

			case Opcode::F32ReinterpretI32:
				CHECK_TRAP(PushRep<float>(Pop<uint32_t>()));
				break;

			case Opcode::F64ConvertSI32:
				CHECK_TRAP(Push<double>(Pop<int32_t>()));
				break;

			case Opcode::F64ConvertUI32:
				CHECK_TRAP(Push<double>(Pop<uint32_t>()));
				break;

			case Opcode::F64ConvertSI64:
				CHECK_TRAP(Push<double>(Pop<int64_t>()));
				break;

			case Opcode::F64ConvertUI64:
				CHECK_TRAP(Push<double>(wabt_convert_uint64_to_double(Pop<uint64_t>())));
				break;

			case Opcode::F64PromoteF32:
				CHECK_TRAP(Push<double>(Pop<float>()));
				break;

			case Opcode::F64ReinterpretI64:
				CHECK_TRAP(PushRep<double>(Pop<uint64_t>()));
				break;

			case Opcode::I32ReinterpretF32:
				CHECK_TRAP(Push<uint32_t>(PopRep<float>()));
				break;

			case Opcode::I64ReinterpretF64:
				CHECK_TRAP(Push<uint64_t>(PopRep<double>()));
				break;

			case Opcode::I32Rotr:
				CHECK_TRAP(Binop(IntRotr<uint32_t>));
				break;

			case Opcode::I32Rotl:
				CHECK_TRAP(Binop(IntRotl<uint32_t>));
				break;

			case Opcode::I64Rotr:
				CHECK_TRAP(Binop(IntRotr<uint64_t>));
				break;

			case Opcode::I64Rotl:
				CHECK_TRAP(Binop(IntRotl<uint64_t>));
				break;

			case Opcode::I64Eqz:
				CHECK_TRAP(Unop(IntEqz<uint32_t, uint64_t>));
				break;

			case Opcode::I32Extend8S:
				CHECK_TRAP(Unop(IntExtendS<uint32_t, int8_t>));
				break;

			case Opcode::I32Extend16S:
				CHECK_TRAP(Unop(IntExtendS<uint32_t, int16_t>));
				break;

			case Opcode::I64Extend8S:
				CHECK_TRAP(Unop(IntExtendS<uint64_t, int8_t>));
				break;

			case Opcode::I64Extend16S:
				CHECK_TRAP(Unop(IntExtendS<uint64_t, int16_t>));
				break;

			case Opcode::I64Extend32S:
				CHECK_TRAP(Unop(IntExtendS<uint64_t, int32_t>));
				break;

			case Opcode::Drop:
				(void)Pop();
				break;

			case Opcode::Nop:
				break;

			case Opcode::I32AtomicWait:
			case Opcode::I64AtomicWait:
			case Opcode::AtomicWake:
				// TODO(binji): Implement.
				TRAP(Unreachable);
				break;

				// The following opcodes are either never generated or should never be
				// executed.
			case Opcode::Block:
			case Opcode::Catch:
			case Opcode::CatchAll:
			case Opcode::Invalid:
			case Opcode::Loop:
			case Opcode::Rethrow:
			case Opcode::Throw:
			case Opcode::Try:
				WABT_UNREACHABLE;
				break;
			}
			++ it;
		}

		PopCall(func->sig->results.size());
exit_opcode_loop: ;
	}

	return result;
}

}
