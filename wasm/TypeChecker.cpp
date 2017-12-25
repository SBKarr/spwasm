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

#include "TypeChecker.h"

namespace wasm {

#define WABT_USE(x) static_cast<void>(x)
#define WABT_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define WABT_STATIC_ASSERT(x) static_assert((x), #x)

#define CHECK_RESULT(expr) do { if (expr == ::wasm::Result::Error) { return ::wasm::Result::Error; } } while (0)

template <typename E>
constexpr typename std::underlying_type<E>::type toInt(const E &e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename Enum>
inline constexpr int GetEnumCount() {
	return toInt(Enum::Last) - toInt(Enum::First) + 1;
}

static const int kLabelTypeCount = GetEnumCount<LabelType>();


template <typename Callback>
inline void TypeChecker::PushErrorStream(const Callback &cb) {
	if (error_callback_) {
		StringStream stream;
		cb(stream);
		error_callback_(stream);
	}
}

static const char* GetTypeName(Type type) {
	switch (type) {
	case Type::I32:
		return "i32";
	case Type::I64:
		return "i64";
	case Type::F32:
		return "f32";
	case Type::F64:
		return "f64";
	case Type::Anyfunc:
		return "anyfunc";
	case Type::Func:
		return "func";
	case Type::Void:
		return "void";
	case Type::Any:
		return "any";
	}
	return "";
}

TypeChecker::Label::Label(LabelType label_type, const TypeVector& sig, size_t limit)
: label_type(label_type), sig(sig), type_stack_limit(limit), unreachable(false) { }

TypeChecker::TypeChecker(const ErrorCallback& error_callback)
: error_callback_(error_callback) {
	label_stack_.reserve(8);
}

Result TypeChecker::GetLabel(Index depth, Label** out_label) {
	if (depth >= label_stack_.size()) {
		PushErrorStream([&] (StringStream &stream) {
			stream << "invalid depth: " << depth << " (max " << (label_stack_.size() - 1) << ")";
		});

		*out_label = nullptr;
		return Result::Error;
	}
	*out_label = &label_stack_[label_stack_.size() - depth - 1];
	return Result::Ok;
}

Result TypeChecker::TopLabel(Label** out_label) {
	return GetLabel(0, out_label);
}

bool TypeChecker::IsUnreachable() {
	Label* label;
	if (Failed(TopLabel(&label))) {
		return true;
	}
	return label->unreachable;
}

void TypeChecker::ResetTypeStackToLabel(Label* label) {
	type_stack_.resize(label->type_stack_limit);
}

Result TypeChecker::SetUnreachable() {
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	label->unreachable = true;
	ResetTypeStackToLabel(label);
	return Result::Ok;
}

void TypeChecker::PushLabel(LabelType label_type, const TypeVector& sig) {
	label_stack_.emplace_back(label_type, sig, type_stack_.size());
}

Result TypeChecker::PopLabel() {
	label_stack_.pop_back();
	return Result::Ok;
}

Result TypeChecker::CheckLabelType(Label* label, LabelType label_type) {
	return label->label_type == label_type ? Result::Ok : Result::Error;
}

Result TypeChecker::PeekType(Index depth, Type* out_type) {
	Label* label;
	CHECK_RESULT(TopLabel(&label));

	if (label->type_stack_limit + depth >= type_stack_.size()) {
		*out_type = Type::Any;
		return label->unreachable ? Result::Ok : Result::Error;
	}
	*out_type = type_stack_[type_stack_.size() - depth - 1];
	return Result::Ok;
}

Result TypeChecker::PeekAndCheckType(Index depth, Type expected) {
	Type actual = Type::Any;
	Result result = PeekType(depth, &actual);
	return result | CheckType(actual, expected);
}

Result TypeChecker::DropTypes(size_t drop_count) {
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	if (label->type_stack_limit + drop_count > type_stack_.size()) {
		if (label->unreachable) {
			ResetTypeStackToLabel(label);
			return Result::Ok;
		}
		return Result::Error;
	}
	type_stack_.erase(type_stack_.end() - drop_count, type_stack_.end());
	return Result::Ok;
}

void TypeChecker::PushType(Type type) {
	if (type != Type::Void) {
		type_stack_.push_back(type);
	}
}

void TypeChecker::PushTypes(const TypeVector& types) {
	for (Type type : types)
		PushType(type);
}

Result TypeChecker::CheckTypeStackEnd(const char* desc) {
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	Result result =
			(type_stack_.size() == label->type_stack_limit) ?
					Result::Ok : Result::Error;
	PrintStackIfFailed(result, desc);
	return result;
}

Result TypeChecker::CheckType(Type actual, Type expected) {
	return (expected == actual || expected == Type::Any || actual == Type::Any) ?
			Result::Ok : Result::Error;
}

Result TypeChecker::CheckSignature(const TypeVector& sig) {
	Result result = Result::Ok;
	for (size_t i = 0; i < sig.size(); ++i)
		result = result | PeekAndCheckType(sig.size() - i - 1, sig[i]);
	return result;
}

Result TypeChecker::PopAndCheckSignature(const TypeVector& sig,
		const char* desc) {
	Result result = CheckSignature(sig);
	PrintStackIfFailed(result, desc, sig);
	result = result | DropTypes(sig.size());
	return result;
}

Result TypeChecker::PopAndCheckCall(const TypeVector& param_types,
		const TypeVector& result_types, const char* desc) {
	Result result = CheckSignature(param_types);
	PrintStackIfFailed(result, desc, param_types);
	result = result | DropTypes(param_types.size());
	PushTypes(result_types);
	return result;
}

Result TypeChecker::PopAndCheck1Type(Type expected, const char* desc) {
	Result result = Result::Ok;
	result = result | PeekAndCheckType(0, expected);
	PrintStackIfFailed(result, desc, expected);
	result = result | DropTypes(1);
	return result;
}

Result TypeChecker::PopAndCheck2Types(Type expected1, Type expected2,
		const char* desc) {
	Result result = Result::Ok;
	result = result | PeekAndCheckType(0, expected2);
	result = result | PeekAndCheckType(1, expected1);
	PrintStackIfFailed(result, desc, expected1, expected2);
	result = result | DropTypes(2);
	return result;
}

Result TypeChecker::PopAndCheck3Types(Type expected1, Type expected2,
		Type expected3, const char* desc) {
	Result result = Result::Ok;
	result = result | PeekAndCheckType(0, expected3);
	result = result | PeekAndCheckType(1, expected2);
	result = result | PeekAndCheckType(2, expected1);
	PrintStackIfFailed(result, desc, expected1, expected2, expected3);
	result = result | DropTypes(3);
	return result;
}

Result TypeChecker::CheckOpcode1(Opcode opcode) {
	Result result = PopAndCheck1Type(opcode.GetParamType1(), opcode.GetName());
	PushType(opcode.GetResultType());
	return result;
}

Result TypeChecker::CheckOpcode2(Opcode opcode) {
	Result result = PopAndCheck2Types(opcode.GetParamType1(),
			opcode.GetParamType2(), opcode.GetName());
	PushType(opcode.GetResultType());
	return result;
}

Result TypeChecker::CheckOpcode3(Opcode opcode) {
	Result result = PopAndCheck3Types(opcode.GetParamType1(),
			opcode.GetParamType2(), opcode.GetParamType3(), opcode.GetName());
	PushType(opcode.GetResultType());
	return result;
}

static void TypesToString(StringStream &stream, const TypeVector& types, const char* prefix = nullptr) {
	stream << "[";
	if (prefix) {
		stream << prefix;
	}

	for (size_t i = 0; i < types.size(); ++i) {
		stream << GetTypeName(types[i]);
		if (i < types.size() - 1) {
			stream << ", ";
		}
	}
	stream << "]";
}

void TypeChecker::PrintStackIfFailed(Result result, const char* desc,
		const TypeVector& expected) {
	if (Failed(result)) {
		size_t limit = 0;
		Label* label;
		if (Succeeded(TopLabel(&label))) {
			limit = label->type_stack_limit;
		}

		TypeVector actual;
		size_t max_depth = type_stack_.size() - limit;

		// In general we want to print as many values of the actual stack as were
		// expected. However, if the stack was expected to be empty, we should
		// print some amount of the actual stack.
		size_t actual_size;
		if (expected.size() == 0) {
			// Don't print too many elements if the stack is really deep.
			const size_t kMaxActualStackToPrint = 4;
			actual_size = std::min(kMaxActualStackToPrint, max_depth);
		} else {
			actual_size = std::min(expected.size(), max_depth);
		}

		bool incomplete_actual_stack = actual_size != max_depth;

		for (size_t i = 0; i < actual_size; ++i) {
			Type type;
			Result result = PeekType(actual_size - i - 1, &type);
			WABT_USE(result);
			assert(Succeeded(result));
			actual.push_back(type);
		}

		PushErrorStream([&] (StringStream &message) {
			message << "type mismatch in " << desc << ", expected ";
			TypesToString(message, expected);
			message << " but got ";
			TypesToString(message, actual, incomplete_actual_stack ? "... " : nullptr);
		});
	}
}

Result TypeChecker::BeginFunction(const TypeVector* sig) {
	type_stack_.clear();
	label_stack_.clear();
	PushLabel(LabelType::Func, *sig);
	return Result::Ok;
}

Result TypeChecker::OnAtomicLoad(Opcode opcode) {
	return CheckOpcode1(opcode);
}

Result TypeChecker::OnAtomicStore(Opcode opcode) {
	return CheckOpcode2(opcode);
}

Result TypeChecker::OnAtomicRmw(Opcode opcode) {
	return CheckOpcode2(opcode);
}

Result TypeChecker::OnAtomicRmwCmpxchg(Opcode opcode) {
	return CheckOpcode3(opcode);
}

Result TypeChecker::OnAtomicWait(Opcode opcode) {
	return CheckOpcode3(opcode);
}

Result TypeChecker::OnAtomicWake(Opcode opcode) {
	return CheckOpcode2(opcode);
}

Result TypeChecker::OnBinary(Opcode opcode) {
	return CheckOpcode2(opcode);
}

Result TypeChecker::OnBlock(const TypeVector* sig) {
	PushLabel(LabelType::Block, *sig);
	return Result::Ok;
}

Result TypeChecker::OnBr(Index depth) {
	Result result = Result::Ok;
	Label* label;
	CHECK_RESULT(GetLabel(depth, &label));
	if (label->label_type != LabelType::Loop) {
		result = result | CheckSignature(label->sig);
	}
	PrintStackIfFailed(result, "br", label->sig);
	CHECK_RESULT(SetUnreachable());
	return result;
}

Result TypeChecker::OnBrIf(Index depth) {
	Result result = PopAndCheck1Type(Type::I32, "br_if");
	Label* label;
	CHECK_RESULT(GetLabel(depth, &label));
	if (label->label_type != LabelType::Loop) {
		result = result | PopAndCheckSignature(label->sig, "br_if");
		PushTypes(label->sig);
	}
	return result;
}

Result TypeChecker::BeginBrTable() {
	br_table_sig_ = Type::Any;
	return PopAndCheck1Type(Type::I32, "br_table");
}

Result TypeChecker::OnBrTableTarget(Index depth) {
	Result result = Result::Ok;
	Label* label;
	CHECK_RESULT(GetLabel(depth, &label));
	Type label_sig;
	if (label->label_type == LabelType::Loop) {
		label_sig = Type::Void;
	} else {
		assert(label->sig.size() <= 1);
		label_sig = label->sig.size() == 0 ? Type::Void : label->sig[0];

		result = result | CheckSignature(label->sig);
		PrintStackIfFailed(result, "br_table", label_sig);
	}

	// Make sure this label's signature is consistent with the previous labels'
	// signatures.
	if (Failed(CheckType(br_table_sig_, label_sig))) {
		result = result | Result::Error;
		PushErrorStream([&] (StringStream &stream) {
			stream << "br_table labels have inconsistent types: expected " << GetTypeName(br_table_sig_)
				<< ", got " << GetTypeName(label_sig);
		});
	}
	br_table_sig_ = label_sig;

	return result;
}

Result TypeChecker::EndBrTable() {
	return SetUnreachable();
}

Result TypeChecker::OnCall(const TypeVector* param_types,
		const TypeVector* result_types) {
	return PopAndCheckCall(*param_types, *result_types, "call");
}

Result TypeChecker::OnCallIndirect(const TypeVector* param_types,
		const TypeVector* result_types) {
	Result result = PopAndCheck1Type(Type::I32, "call_indirect");
	result = result | PopAndCheckCall(*param_types, *result_types, "call_indirect");
	return result;
}

Result TypeChecker::OnCompare(Opcode opcode) {
	return CheckOpcode2(opcode);
}

Result TypeChecker::OnCatch(const TypeVector* sig) {
	PushTypes(*sig);
	return Result::Ok;
}

Result TypeChecker::OnCatchBlock(const TypeVector* sig) {
	Result result = Result::Ok;
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	result = result | CheckLabelType(label, LabelType::Try);
	result = result | PopAndCheckSignature(label->sig, "try block");
	result = result | CheckTypeStackEnd("try block");
	ResetTypeStackToLabel(label);
	label->label_type = LabelType::Catch;
	label->unreachable = false;
	return result;
}

Result TypeChecker::OnConst(Type type) {
	PushType(type);
	return Result::Ok;
}

Result TypeChecker::OnConvert(Opcode opcode) {
	return CheckOpcode1(opcode);
}

Result TypeChecker::OnCurrentMemory() {
	PushType(Type::I32);
	return Result::Ok;
}

Result TypeChecker::OnDrop() {
	Result result = Result::Ok;
	result = result | DropTypes(1);
	PrintStackIfFailed(result, "drop", Type::Any);
	return result;
}

Result TypeChecker::OnElse() {
	Result result = Result::Ok;
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	result = result | CheckLabelType(label, LabelType::If);
	result = result | PopAndCheckSignature(label->sig, "if true branch");
	result = result | CheckTypeStackEnd("if true branch");
	ResetTypeStackToLabel(label);
	label->label_type = LabelType::Else;
	label->unreachable = false;
	return result;
}

Result TypeChecker::OnEnd(Label* label, const char* sig_desc,
		const char* end_desc) {
	Result result = Result::Ok;
	result = result | PopAndCheckSignature(label->sig, sig_desc);
	result = result | CheckTypeStackEnd(end_desc);
	ResetTypeStackToLabel(label);
	PushTypes(label->sig);
	PopLabel();
	return result;
}

Result TypeChecker::OnEnd() {
	Result result = Result::Ok;
	static const char* s_label_type_name[] = { "function", "block", "loop",
			"if", "if false branch", "try", "try catch" };
	WABT_STATIC_ASSERT(WABT_ARRAY_SIZE(s_label_type_name) == kLabelTypeCount);
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	assert(static_cast<int>(label->label_type) < kLabelTypeCount);
	if (label->label_type == LabelType::If) {
		if (label->sig.size() != 0) {
			PushErrorStream([&] (StringStream &stream) {
				stream << "if without else cannot have type signature.";
			});
			result = Result::Error;
		}
	}
	const char* desc = s_label_type_name[static_cast<int>(label->label_type)];
	result = result | OnEnd(label, desc, desc);
	return result;
}

Result TypeChecker::OnGrowMemory() {
	return CheckOpcode1(Opcode::GrowMemory);
}

Result TypeChecker::OnIf(const TypeVector* sig) {
	Result result = PopAndCheck1Type(Type::I32, "if");
	PushLabel(LabelType::If, *sig);
	return result;
}

Result TypeChecker::OnGetGlobal(Type type) {
	PushType(type);
	return Result::Ok;
}

Result TypeChecker::OnGetLocal(Type type) {
	PushType(type);
	return Result::Ok;
}

Result TypeChecker::OnLoad(Opcode opcode) {
	return CheckOpcode1(opcode);
}

Result TypeChecker::OnLoop(const TypeVector* sig) {
	PushLabel(LabelType::Loop, *sig);
	return Result::Ok;
}

Result TypeChecker::OnRethrow(Index depth) {
	Result result = Result::Ok;
	Label* label;
	CHECK_RESULT(GetLabel(depth, &label));
	if (label->label_type != LabelType::Catch) {
		PushErrorStream([&] (StringStream &stream) {
			bool empty = true;
			StringStream candidates;
			size_t last = label_stack_.size() - 1;
			for (size_t i = 0; i < label_stack_.size(); ++i) {
				if (label_stack_[last - i].label_type == LabelType::Catch) {
					if (!empty) {
						candidates << ", ";
					} else {
						empty = false;
					}
					candidates << i;
				}
			}
			if (empty) {
				stream << "Rethrow not in try catch block";
			} else {
				stream << "invalid rethrow depth: " << depth << " (catches: " << candidates.str() << ")";
			}
		});
		result = Result::Error;
	}
	CHECK_RESULT(SetUnreachable());
	return result;
}

Result TypeChecker::OnThrow(const TypeVector* sig) {
	Result result = Result::Ok;
	result = result | PopAndCheckSignature(*sig, "throw");
	CHECK_RESULT(SetUnreachable());
	return result;
}

Result TypeChecker::OnReturn() {
	Result result = Result::Ok;
	Label* func_label;
	CHECK_RESULT(GetLabel(label_stack_.size() - 1, &func_label));
	result = result | PopAndCheckSignature(func_label->sig, "return");
	CHECK_RESULT(SetUnreachable());
	return result;
}

Result TypeChecker::OnSelect() {
	Result result = Result::Ok;
	Type type = Type::Any;
	result = result | PeekAndCheckType(0, Type::I32);
	result = result | PeekType(1, &type);
	result = result | PeekAndCheckType(2, type);
	PrintStackIfFailed(result, "select", Type::I32, type, type);
	result = result | DropTypes(3);
	PushType(type);
	return result;
}

Result TypeChecker::OnSetGlobal(Type type) {
	return PopAndCheck1Type(type, "set_global");
}

Result TypeChecker::OnSetLocal(Type type) {
	return PopAndCheck1Type(type, "set_local");
}

Result TypeChecker::OnStore(Opcode opcode) {
	return CheckOpcode2(opcode);
}

Result TypeChecker::OnTryBlock(const TypeVector* sig) {
	PushLabel(LabelType::Try, *sig);
	return Result::Ok;
}

Result TypeChecker::OnTeeLocal(Type type) {
	Result result = Result::Ok;
	result = result | PopAndCheck1Type(type, "tee_local");
	PushType(type);
	return result;
}

Result TypeChecker::OnUnary(Opcode opcode) {
	return CheckOpcode1(opcode);
}

Result TypeChecker::OnUnreachable() {
	return SetUnreachable();
}

Result TypeChecker::EndFunction() {
	Result result = Result::Ok;
	Label* label;
	CHECK_RESULT(TopLabel(&label));
	result = result | CheckLabelType(label, LabelType::Func);
	result = result | OnEnd(label, "implicit return", "function");
	return result;
}

}  // namespace wasm
