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

#include <iostream>
#include <iomanip>
#include "Thread.h"
#include "Environment.h"

#include <string.h>

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

static void printMemoryBlock(std::ostream &stream, const uint8_t *ptr, Index n) {
	for (Index i = 0; i < n; ++ i) {
		stream << std::hex << std::setw(2) << std::setfill('0') << int(ptr[i]) << std::dec << std::setw(1);
	}
}

template<typename MemType>
Thread::Result Thread::GetAccessAddress(const Func::OpcodeRec * pc, void** out_address) {
	auto memory = _currentFrame->module->memory[pc->value32.v2];
	uint64_t addr = static_cast<uint64_t>(Pop<uint32_t>()) + pc->value32.v1;
	TRAP_IF(addr + sizeof(MemType) > memory->size, MemoryAccessOutOfBounds);
	*out_address = memory->data + addr;
	return Result::Ok;
}

template<typename MemType>
Thread::Result Thread::GetAtomicAccessAddress(const Func::OpcodeRec * pc, void** out_address) {
	auto memory = _currentFrame->module->memory[pc->value32.v2];
	uint64_t addr = static_cast<uint64_t>(Pop<uint32_t>()) + pc->value32.v1;
	TRAP_IF(addr + sizeof(MemType) > memory->size, MemoryAccessOutOfBounds);
	TRAP_IF((addr & (sizeof(MemType) - 1)) != 0, AtomicMemoryAccessUnaligned);
	*out_address = memory->data + addr;
	return Result::Ok;
}

Value& Thread::Top() {
	return Pick(1);
}

Value& Thread::Pick(Index depth) {
	return _valueStack[_valueStackTop - depth];
}

void Thread::Reset() {
	_valueStackTop = 0;
	_callStackTop = 0;
}

Thread::Result Thread::Push(Value value) {
	CHECK_STACK();
	_valueStack[_valueStackTop++] = value;
	return Result::Ok;
}

Value Thread::Pop() {
	return _valueStack[--_valueStackTop];
}

Value Thread::ValueAt(Index at) const {
	assert(at < _valueStackTop);
	return _valueStack[at];
}

template<typename T>
Thread::Result Thread::Push(T value) {
	return PushRep<T>(ToRep(value));
}

template<typename T>
T Thread::Pop() {
	return FromRep<T>(PopRep<T>());
}

template<typename T>
Thread::Result Thread::PushRep(ValueTypeRep<T> value) {
	return Push(MakeValue<T>(value));
}

template<typename T>
ValueTypeRep<T> Thread::PopRep() {
	return GetValue<T>(Pop());
}

void Thread::StoreResult(Value *begin, Index stack, Index results) {
	const auto d = _valueStack.data();
	auto resultsPtr = d + _valueStackTop - results;
	_valueStackTop = (begin - d) + stack + results;
	if (results > 0) {
		memmove(begin + stack, resultsPtr, results * sizeof(Value));
	}
}

Thread::Result Thread::PushCall(const RuntimeModule &module, const Func &func) {
	TRAP_IF(_callStackTop >= _callStack.size(), CallStackExhausted);
	_callStack[_callStackTop] = CallStackFrame{&module, &func, _valueStack.data() + _valueStackTop - func.types.size(), func.opcodes.data()};
	++ _callStackTop;
	return Result::Ok;
}

Thread::Result Thread::PushCall(const RuntimeModule &module, Index idx, bool import) {
	TrySync();
	if (!import) {
		return PushCall(module, *module.func[idx].first);
	} else {
		TRAP_IF(_callStackTop >= _callStack.size(), CallStackExhausted);
		auto &fn = module.func[idx];
		if (fn.first) {
			if (auto rtMod = _runtime->getModule(fn.first->module)) {
				return PushCall(*rtMod, *module.func[idx].first);
			}
		} else if (fn.second) {
			Index newTop = _valueStackTop - fn.second->sig.params.size() + fn.second->sig.results.size();
			TRAP_IF(newTop >= _valueStack.size(), ValueStackExhausted);
			if (fn.second->callback(this, fn.second, _valueStack.data() + _valueStackTop - fn.second->sig.params.size()) == wasm::Result::Ok) {
				_valueStackTop = newTop;
				return Result::Returned;
			}
		}
	}
	return Result::TrapHostTrapped;
}

void Thread::PopCall(Index idx) {
	const Index newTop = _currentFrame->locals - _valueStack.data() + idx;
	if (idx > 0) {
		memmove(_currentFrame->locals, _valueStack.data() + _valueStackTop - idx, idx * sizeof(Value));
	}
	_valueStackTop = newTop;
	-- _callStackTop;
}

template <typename MemType, typename ResultType>
Thread::Result Thread::Load(const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	static_assert(std::is_floating_point<MemType>::value == std::is_floating_point<ExtendedType>::value,
			"Extended type should be float iff MemType is float");

	void* src;
	CHECK_TRAP(GetAccessAddress<MemType>(pc, &src));
	MemType value;
	LoadFromMemory<MemType>(&value, src);
	return Push<ResultType>(static_cast<ExtendedType>(value));
}

template <typename MemType, typename ResultType>
Thread::Result Thread::Store(const Func::OpcodeRec * pc) {
	typedef typename WrapMemType<ResultType, MemType>::type WrappedType;
	WrappedType value = PopRep<ResultType>();
	void* dst;
	CHECK_TRAP(GetAccessAddress<MemType>(pc, &dst));
	StoreToMemory<WrappedType>(dst, value);
	return Result::Ok;
}

template <typename MemType, typename ResultType>
Thread::Result Thread::AtomicLoad(const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	static_assert(!std::is_floating_point<MemType>::value,
			"AtomicLoad type can't be float");
	void* src;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &src));
	MemType value;
	LoadFromMemory<MemType>(&value, src);
	return Push<ResultType>(static_cast<ExtendedType>(value));
}

template<typename MemType, typename ResultType>
Thread::Result Thread::AtomicStore(const Func::OpcodeRec * pc) {
	typedef typename WrapMemType<ResultType, MemType>::type WrappedType;
	WrappedType value = PopRep<ResultType>();
	void* dst;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &dst));
	StoreToMemory<WrappedType>(dst, value);
	return Result::Ok;
}

template<typename MemType, typename ResultType>
Thread::Result Thread::AtomicRmw(BinopFunc<ResultType, ResultType> func, const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	MemType rhs = PopRep<ResultType>();
	void* addr;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &addr));
	MemType read;
	LoadFromMemory<MemType>(&read, addr);
	StoreToMemory<MemType>(addr, func(read, rhs));
	return Push<ResultType>(static_cast<ExtendedType>(read));
}

template<typename MemType, typename ResultType>
Thread::Result Thread::AtomicRmwCmpxchg(const Func::OpcodeRec * pc) {
	typedef typename ExtendMemType<ResultType, MemType>::type ExtendedType;
	MemType replace = PopRep<ResultType>();
	MemType expect = PopRep<ResultType>();
	void* addr;
	CHECK_TRAP(GetAtomicAccessAddress<MemType>(pc, &addr));
	MemType read;
	LoadFromMemory<MemType>(&read, addr);
	if (read == expect) {
		StoreToMemory<MemType>(addr, replace);
	}
	return Push<ResultType>(static_cast<ExtendedType>(read));
}

template<typename R, typename T>
Thread::Result Thread::Unop(UnopFunc<R, T> func) {
	auto value = PopRep<T>();
	return PushRep<R>(func(value));
}

template<typename R, typename T>
Thread::Result Thread::UnopTrap(UnopTrapFunc<R, T> func) {
	auto value = PopRep<T>();
	ValueTypeRep<R> result_value;
	CHECK_TRAP(func(value, &result_value));
	return PushRep<R>(result_value);
}

template<typename R, typename T>
Thread::Result Thread::Binop(BinopFunc<R, T> func) {
	auto rhs_rep = PopRep<T>();
	auto lhs_rep = PopRep<T>();
	return PushRep<R>(func(lhs_rep, rhs_rep));
}

template<typename R, typename T>
Thread::Result Thread::BinopTrap(BinopTrapFunc<R, T> func) {
	auto rhs_rep = PopRep<T>();
	auto lhs_rep = PopRep<T>();
	ValueTypeRep<R> result_value;
	CHECK_TRAP(func(lhs_rep, rhs_rep, &result_value));
	return PushRep<R>(result_value);
}

Thread::Thread(const Runtime *runtime, Index tag) : _runtime(runtime), _tag(tag) { }

bool Thread::init(uint32_t valueStackSize, uint32_t callStackSize) {
	_callStackTop = 0;
	_valueStackTop = 0;

	_valueStack.resize(valueStackSize);
	_callStack.resize(callStackSize);

	return true;
}

void Thread::setSyncContext(ThreadContext *ctx) {
	_context = ctx;
	if (_context) {
		_contextLock = std::shared_lock<std::shared_timed_mutex>(_context->mutex, std::defer_lock_t());
	} else {
		_contextLock = std::shared_lock<std::shared_timed_mutex>();
	}
}

ThreadContext *Thread::getSyncContext() const {
	return _context;
}

void Thread::setUserContext(uint32_t ctx) {
	_userContext = ctx;
}
uint32_t Thread::getUserContext() const {
	return _userContext;
}

void Thread::setThreadContext(void *ctx) {
	_threadContext = ctx;
}
void *Thread::getThreadContext() const {
	return _threadContext;
}

void Thread::setUserStackPointer(uint32_t pointer, uint32_t guard) {
	_userStackPointer = pointer;
	_userStackGuard = guard;
}
uint32_t Thread::getUserStackPointer() const {
	return _userStackPointer;
}
uint32_t Thread::getUserStackGuard() const {
	return _userStackGuard;
}

Thread::Result Thread::allocStack(uint32_t size, uint32_t &result) {
	if (_userStackGuard + size > _userStackPointer) {
		return Result::TrapUserStackExhausted;
	} else {
		_userStackPointer -= size;
		result = _userStackPointer;
		return Result::Ok;
	}
}

void Thread::freeStack(uint32_t size) {
	_userStackPointer += size;
}

const uint8_t* GetIstream() {
	return nullptr;
}

Thread::Result Thread::Run(const RuntimeModule &module, const Func &func, Value *buffer, bool silent) {
	bool locked = false;
	if (_contextLock.mutex() && !_contextLock.owns_lock()) {
		_contextLock.lock();
		locked = true;
	}

	auto origStack = _callStackTop;
	auto origValue = _valueStackTop;
	CHECK_TRAP(PushLocals(func, buffer));
	CHECK_TRAP(PushCall(module, func));
	auto res = Run(origStack);
	if (res == Result::Ok || res == Result::Returned) {
		Index nresults = func.sig->results.size();
		memcpy(buffer, &_valueStack[_valueStackTop - nresults], nresults * sizeof(Value));
		_valueStackTop -= nresults;
	} else {
		if (!silent) {
			_runtime->onThreadError(*this);
		}
		_callStackTop = origStack;
		_valueStackTop = origValue;
	}

	if (locked) {
		_contextLock.unlock();
	}
	return res;
}

RuntimeMemory *Thread::GetMemoryPtr(Index memIndex) const {
	if (_currentFrame && memIndex < _currentFrame->module->memory.size()) {
		return _currentFrame->module->memory[memIndex];
	}
	return nullptr;
}

uint8_t *Thread::GetMemory(Index memIndex, Index offset) const {
	if (_currentFrame && memIndex < _currentFrame->module->memory.size()) {
		auto mem = _currentFrame->module->memory[memIndex];
		if (offset < mem->size) {
			return &mem->data[offset];
		}
	}
	return nullptr;
}

uint8_t *Thread::GetMemory(Index memIndex, Index offset, Index size) const {
	if (_currentFrame && memIndex < _currentFrame->module->memory.size()) {
		auto mem = _currentFrame->module->memory[memIndex];
		if (offset + size <= mem->size) {
			return &mem->data[offset];
		}
	}
	return nullptr;
}

Thread::Result Thread::PushLocals(const Func &func, const Value *buffer, Index storeParams) {
	const Index paramsSpace = func.types.size() - storeParams;
	const Index extraStackSpace = std::max(paramsSpace, Index(func.sig->results.size()));
	const Index nParams = func.sig->params.size();
	const Index nLocals = func.types.size();

	if (_valueStackTop + extraStackSpace > _valueStack.size()) {
		return Result::TrapValueStackExhausted;
	}

	if (buffer) {
		auto ptr = _valueStack.data() + _valueStackTop;
		memcpy(ptr, buffer, (func.sig->params.size() - storeParams) * sizeof(Value));
	}
	memset(_valueStack.data() + _valueStackTop + nParams, 0, sizeof(Value) * (nLocals - nParams));

	_valueStackTop += paramsSpace;
	return Result::Ok;
}

bool Thread::GrowMemory(const RuntimeMemory *memory, Index pages) {
	bool ret = true;
	bool locked = _contextLock.owns_lock();
	if (locked) {
		_contextLock.unlock();
		_context->stopFlag.store(true); // flag to suspend others thread
		_context->mutex.lock(); // acquire exclusive lock;
		_context->stopFlag.store(false);
	}
	if (!_runtime->growMemory(*memory, pages)) {
		ret = false;
	}
	if (locked) {
		_context->mutex.unlock();
		_context->cond.notify_all();
		_contextLock.lock();
	}
	return ret;
}

void Thread::TrySync() {
	if (_context && _context->stopFlag.load()) {
		if (_contextLock) {
			_context->cond.wait(_contextLock);
		}
	}
}

void Thread::PrintStackFrame(std::ostream &stream, const CallStackFrame &frame, Index maxOpcodes) const {
	StringView modName = _runtime->getModuleName(frame.module);
	auto funcName = _runtime->getModuleFunctionName(*frame.module, frame.func);

	if (frame.func && !frame.func->name.empty()) {
		stream << frame.func->name << ": ";
	}

	stream << "[" << funcName.first << "] " <<  modName << " " << funcName.second << ":\n";

	stream << "\tLocals:";
	Index i = 0;
	for (auto &it : frame.func->types) {
		stream << "\t\t";
		if (i < frame.func->sig->params.size()) {
			stream << "param l" << i;
		} else {
			stream << "local l" << i;
		}

		stream << ": ";
		printType(stream, it);
		stream << " = ";
		switch (it) {
		case Type::I32:
			stream << "0x" << std::setw(8) << std::setfill('0') << std::hex << frame.locals[i].i32 << " memory:";
			printMemoryBlock(stream, (const uint8_t *)&frame.locals[i].i32, 4);
			stream << " ( " << std::dec << std::setw(1) << frame.locals[i].i32 << " )";
			break;
		case Type::I64:
			stream << "0x" << std::setw(16) << std::setfill('0') << std::hex << frame.locals[i].i64 << " memory:";
			printMemoryBlock(stream, (const uint8_t *)&frame.locals[i].i64, 8);
			stream << " ( " << std::dec << std::setw(1) << frame.locals[i].i64 << " )";
			break;
		case Type::F32:
			stream << "0x" << std::setw(8) << std::setfill('0') << std::hex << frame.locals[i].f32_bits << " memory:";
			printMemoryBlock(stream, (const uint8_t *)&frame.locals[i].f32_bits, 4);
			stream << " ( " << std::dec << std::setw(1) << reinterpret_cast<float &>(frame.locals[i].f32_bits) << " ):";
			break;
		case Type::F64:
			stream << "0x" << std::setw(16) << std::setfill('0') << std::hex << frame.locals[i].f64_bits << " memory:";
			printMemoryBlock(stream, (const uint8_t *)&frame.locals[i].f64_bits, 8);
			stream << " ( " << std::dec << std::setw(1) << reinterpret_cast<double &>(frame.locals[i].f64_bits) << " ):";
			break;
		default:
			break;
		}
		stream << "\n";
		++ i;
	}

	Index position = frame.position - frame.func->opcodes.data();
	Index nOpcodes = std::min(maxOpcodes, position + 1);

	stream << "\tCode:\n";
	auto data = frame.func->opcodes.data();
	for (Index i = 0; i < nOpcodes; ++ i) {
		auto opcode = frame.position - nOpcodes + i + 1;

		stream << "\t\t(" << opcode - data  << ") " << Opcode(opcode->opcode).GetName() << " ";
		switch (opcode->opcode) {
		case Opcode::I64Const:
		case Opcode::F64Const:
			stream << opcode->value64;
			break;
		default:
			stream << opcode->value32.v1 << " " << opcode->value32.v2;
			break;
		}
		stream << "\n";
	}
}

void Thread::PrintStackTrace(std::ostream &stream, Index maxUnwind, Index maxOpcodes) const {
	stream << "Stack unwind:\n";
	Index unwind = std::min(_callStackTop, maxUnwind);
	for (Index i = 0; i < unwind; ++ i) {
		auto frame = &_callStack[_callStackTop - 1 - i];

		stream << "(" << i << ") ";
		PrintStackFrame(stream, *frame);
	}
}

void Thread::PrintMemoryDump(std::ostream &stream, Index memIndex, uint32_t address, uint32_t size) {
	if (auto mem = GetMemoryPtr(memIndex)) {
		mem->print(stream, address, size);
	}
}

void Thread::onThreadError() const {
	_runtime->onThreadError(*this);
}

}
