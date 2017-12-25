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

#ifndef SRC_TYPECHECKER_H_
#define SRC_TYPECHECKER_H_

#include "Opcode.h"

namespace wasm {

class TypeChecker {
public:
	using ErrorCallback = Function<void (StringStream &msg)>;

	struct Label {
		Label(LabelType, const TypeVector& sig, size_t limit);

		LabelType label_type;
		TypeVector sig;
		size_t type_stack_limit;
		bool unreachable;
	};

	TypeChecker() = default;
	explicit TypeChecker(const ErrorCallback &);

	void set_error_callback(const ErrorCallback& error_callback) {
		error_callback_ = error_callback;
	}

	size_t type_stack_size() const {
		return type_stack_.size();
	}

	bool IsUnreachable();
	Result GetLabel(Index depth, Label** out_label);

	Result BeginFunction(const TypeVector* sig);
	Result OnAtomicLoad(Opcode);
	Result OnAtomicStore(Opcode);
	Result OnAtomicRmw(Opcode);
	Result OnAtomicRmwCmpxchg(Opcode);
	Result OnAtomicWait(Opcode);
	Result OnAtomicWake(Opcode);
	Result OnBinary(Opcode);
	Result OnBlock(const TypeVector* sig);
	Result OnBr(Index depth);
	Result OnBrIf(Index depth);
	Result BeginBrTable();
	Result OnBrTableTarget(Index depth);
	Result EndBrTable();
	Result OnCall(const TypeVector* param_types, const TypeVector* result_types);
	Result OnCallIndirect(const TypeVector* param_types, const TypeVector* result_types);
	Result OnCatch(const TypeVector* sig);
	Result OnCatchBlock(const TypeVector* sig);
	Result OnCompare(Opcode);
	Result OnConst(Type);
	Result OnConvert(Opcode);
	Result OnCurrentMemory();
	Result OnDrop();
	Result OnElse();
	Result OnEnd();
	Result OnGetGlobal(Type);
	Result OnGetLocal(Type);
	Result OnGrowMemory();
	Result OnIf(const TypeVector* sig);
	Result OnLoad(Opcode);
	Result OnLoop(const TypeVector* sig);
	Result OnRethrow(Index depth);
	Result OnReturn();
	Result OnSelect();
	Result OnSetGlobal(Type);
	Result OnSetLocal(Type);
	Result OnStore(Opcode);
	Result OnTeeLocal(Type);
	Result OnThrow(const TypeVector* sig);
	Result OnTryBlock(const TypeVector* sig);
	Result OnUnary(Opcode);
	Result OnUnreachable();
	Result EndFunction();

private:
	template <typename Callback>
	void PushErrorStream(const Callback &);

	Result TopLabel(Label** out_label);
	void ResetTypeStackToLabel(Label* label);
	Result SetUnreachable();
	void PushLabel(LabelType label_type, const TypeVector& sig);
	Result PopLabel();
	Result CheckLabelType(Label* label, LabelType label_type);
	Result PeekType(Index depth, Type* out_type);
	Result PeekAndCheckType(Index depth, Type expected);
	Result DropTypes(size_t drop_count);
	void PushType(Type type);
	void PushTypes(const TypeVector& types);
	Result CheckTypeStackEnd(const char* desc);
	Result CheckType(Type actual, Type expected);
	Result CheckSignature(const TypeVector& sig);
	Result PopAndCheckSignature(const TypeVector& sig, const char* desc);
	Result PopAndCheckCall(const TypeVector& param_types, const TypeVector& result_types, const char* desc);
	Result PopAndCheck1Type(Type expected, const char* desc);
	Result PopAndCheck2Types(Type expected1, Type expected2, const char* desc);
	Result PopAndCheck3Types(Type expected1, Type expected2, Type expected3, const char* desc);
	Result CheckOpcode1(Opcode opcode);
	Result CheckOpcode2(Opcode opcode);
	Result CheckOpcode3(Opcode opcode);
	Result OnEnd(Label* label, const char* sig_desc, const char* end_desc);

	template<typename ... Args>
	void PrintStackIfFailed(Result result, const char* desc, Args ... args) {
		// Minor optimzation, check result before constructing the vector to pass
		// to the other overload of PrintStackIfFailed.
		if (Failed(result)) {
			PrintStackIfFailed(result, desc, { args... });
		}
	}

	void PrintStackIfFailed(Result, const char* desc, const TypeVector&);

	ErrorCallback error_callback_;
	TypeVector type_stack_;
	Vector<Label> label_stack_;
	Type br_table_sig_ = Type::Void;
};

}

#endif /* SRC_TYPECHECKER_H_ */
