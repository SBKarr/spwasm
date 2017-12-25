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

#ifndef SRC_THREAD_H_
#define SRC_THREAD_H_

#include <shared_mutex>
#include <atomic>
#include <condition_variable>
#include "Module.h"

#define WABT_WARN_UNUSED __attribute__ ((warn_unused_result))

namespace wasm {

#define FOREACH_INTERP_RESULT(V)                                              \
	V(Ok, "ok")                                                               \
	/* returned from the top-most function */                                 \
	V(Returned, "returned")                                                   \
	/* memory access is out of bounds */                                      \
	V(TrapMemoryAccessOutOfBounds, "out of bounds memory access")             \
	/* atomic memory access is unaligned  */                                  \
	V(TrapAtomicMemoryAccessUnaligned, "atomic memory access is unaligned")   \
	/* converting from float -> int would overflow int */                     \
	V(TrapIntegerOverflow, "integer overflow")                                \
	/* dividend is zero in integer divide */                                  \
	V(TrapIntegerDivideByZero, "integer divide by zero")                      \
	/* converting from float -> int where float is nan */                     \
	V(TrapInvalidConversionToInteger, "invalid conversion to integer")        \
	/* function table index is out of bounds */                               \
	V(TrapUndefinedTableIndex, "undefined table index")                       \
	/* function table element is uninitialized */                             \
	V(TrapUninitializedTableElement, "uninitialized table element")           \
	/* unreachable instruction executed */                                    \
	V(TrapUnreachable, "unreachable executed")                                \
	/* call indirect signature doesn't match function table signature */      \
	V(TrapIndirectCallSignatureMismatch, "indirect call signature mismatch")  \
	/* ran out of call stack frames (probably infinite recursion) */          \
	V(TrapCallStackExhausted, "call stack exhausted")                         \
	/* ran out of value stack space */                                        \
	V(TrapValueStackExhausted, "value stack exhausted")                       \
	/* ran out of value stack space */                                        \
	V(TrapUserStackExhausted, "user stack exhausted")                         \
	/* we called a host function, but the return value didn't match the */    \
	/* expected type */                                                       \
	V(TrapHostResultTypeMismatch, "host result type mismatch")                \
	/* we called an import function, but it didn't complete succesfully */    \
	V(TrapHostTrapped, "host function trapped")                               \
	/* we attempted to call a function with the an argument list that doesn't \
	* match the function signature */                                         \
	V(ArgumentTypeMismatch, "argument type mismatch")                         \
	/* we tried to get an export by name that doesn't exist */                \
	V(UnknownExport, "unknown export")                                        \
	/* the expected export kind doesn't match. */                             \
	V(ExportKindMismatch, "export kind mismatch")

struct RuntimeMemory;

struct ThreadContext {
	std::atomic<bool> stopFlag;
	std::shared_timed_mutex mutex;
	std::condition_variable_any cond;
};

class Thread {
public:
	enum class Result {
	#define V(Name, str) Name,
		FOREACH_INTERP_RESULT(V)
	#undef V
	};

	static const uint32_t kDefaultValueStackSize = 1024;
	static const uint32_t kDefaultCallStackSize = 256;

	struct CallStackFrame {
		const RuntimeModule *module = nullptr;
		const Func *func = nullptr;
		Value *locals = nullptr;
		const Func::OpcodeRec * position = nullptr;
	};

	explicit Thread(const Runtime *, Index tag = 0);

	bool init(uint32_t = kDefaultValueStackSize, uint32_t = kDefaultCallStackSize);

	void setSyncContext(ThreadContext *ctx);
	ThreadContext *getSyncContext() const;

	void setUserContext(uint32_t);
	uint32_t getUserContext() const;

	void setThreadContext(void *);
	void *getThreadContext() const;

	void setUserStackPointer(uint32_t pointer, uint32_t guard = 0);
	uint32_t getUserStackPointer() const;
	uint32_t getUserStackGuard() const;

	Result allocStack(uint32_t size, uint32_t &result);
	void freeStack(uint32_t size);

	template <typename Callback>
	Result allocCallback(uint32_t size, const Callback &cb) {
		uint32_t ptr;
		auto res = allocStack(size, ptr);
		if (res == Result::Ok) {
			res = cb(ptr, size);
		}
		return res;
	}

	void Reset();
	Index NumValues() const { return _valueStackTop; }
	Result Push(Value) WABT_WARN_UNUSED;
	Value Pop();
	Value ValueAt(Index at) const;

	template <typename Callback>
	Result Prepare(const RuntimeModule &, const Func &, const Callback &, bool silent = false);

	Result Run(const RuntimeModule &, const Func &, Value *buffer = nullptr, bool silent = false);

	RuntimeMemory *GetMemoryPtr(Index memIndex) const;

	uint8_t *GetMemory(Index memIndex, Index offset) const;
	uint8_t *GetMemory(Index memIndex, Index offset, Index size) const;

	void PrintStackFrame(std::ostream &, const CallStackFrame &, Index maxOpcodes = kInvalidIndex) const;
	void PrintStackTrace(std::ostream &, Index maxUnwind = kInvalidIndex, Index maxOpcodes = kInvalidIndex) const;

	void PrintMemoryDump(std::ostream &stream, Index memIndex, uint32_t address, uint32_t size);

	bool GrowMemory(const RuntimeMemory *module, Index pages);

private:
	Result PushLocals(const Func &func, const Value *buffer, Index storeParams = 0);

	template<typename MemType>
	Result GetAccessAddress(const Func::OpcodeRec * pc, void** out_address);

	template<typename MemType>
	Result GetAtomicAccessAddress(const Func::OpcodeRec * pc, void** out_address);

	Value& Top();
	Value& Pick(Index depth);

	// Push/Pop values with conversions, e.g. Push<float> will convert to the
	// ValueTypeRep (uint32_t) and push that. Similarly, Pop<float> will pop the
	// value and convert to float.
	template<typename T> Result Push(T) WABT_WARN_UNUSED;
	template<typename T> T Pop();

	// Push/Pop values without conversions, e.g. Push<float> will take a uint32_t
	// argument which is the integer representation of that float value.
	// Similarly, PopRep<float> will not convert the value to a float.
	template<typename T> Result PushRep(ValueTypeRep<T>) WABT_WARN_UNUSED;
	template<typename T> ValueTypeRep<T> PopRep();

	void StoreResult(Value *, Index stack, Index results);

	void TrySync();

	Result Run(Index stackTop);
	Result PushCall(const RuntimeModule &module, const Func &func) WABT_WARN_UNUSED;
	Result PushCall(const RuntimeModule &module, Index idx, bool import) WABT_WARN_UNUSED;
	void PopCall(Index);

	template<typename R, typename T> using UnopFunc = R(T);
	template<typename R, typename T> using UnopTrapFunc = Result(T, R*);
	template<typename R, typename T> using BinopFunc = R(T, T);
	template<typename R, typename T> using BinopTrapFunc = Result(T, T, R*);

	template<typename MemType, typename ResultType = MemType>
	Result Load(const Func::OpcodeRec * pc) WABT_WARN_UNUSED;
	template<typename MemType, typename ResultType = MemType>
	Result Store(const Func::OpcodeRec * pc) WABT_WARN_UNUSED;
	template<typename MemType, typename ResultType = MemType>
	Result AtomicLoad(const Func::OpcodeRec * pc) WABT_WARN_UNUSED;
	template<typename MemType, typename ResultType = MemType>
	Result AtomicStore(const Func::OpcodeRec * pc) WABT_WARN_UNUSED;
	template<typename MemType, typename ResultType = MemType>
	Result AtomicRmw(BinopFunc<ResultType, ResultType>, const Func::OpcodeRec * pc) WABT_WARN_UNUSED;
	template<typename MemType, typename ResultType = MemType>
	Result AtomicRmwCmpxchg(const Func::OpcodeRec * pc) WABT_WARN_UNUSED;

	template<typename R, typename T = R>
	Result Unop(UnopFunc<R, T> func) WABT_WARN_UNUSED;
	template<typename R, typename T = R>
	Result UnopTrap(UnopTrapFunc<R, T> func) WABT_WARN_UNUSED;

	template<typename R, typename T = R>
	Result Binop(BinopFunc<R, T> func) WABT_WARN_UNUSED;
	template<typename R, typename T = R>
	Result BinopTrap(BinopTrapFunc<R, T> func) WABT_WARN_UNUSED;

	void onThreadError() const;

	const Runtime *_runtime = nullptr;

	std::shared_lock<std::shared_timed_mutex> _contextLock;
	ThreadContext *_context = nullptr;

	Vector<Value> _valueStack;
	Index _valueStackTop = 0;

	CallStackFrame *_currentFrame = nullptr;
	Vector<CallStackFrame> _callStack;
	uint32_t _callStackTop = 0;
	Index _tag = 0;

	uint32_t _userStackPointer = 0;
	uint32_t _userStackGuard = 0;
	uint32_t _userContext = 0;
	void *_threadContext = nullptr;
};


template <typename Callback>
inline Thread::Result Thread::Prepare(const RuntimeModule &module, const Func &func, const Callback &cb, bool silent) {
	auto origStack = _callStackTop;
	auto origValue = _valueStackTop;

	const Index extraStackSpace = std::max(func.types.size(), func.sig->results.size());

	if (_valueStackTop + extraStackSpace > _valueStack.size()) {
		return Result::TrapValueStackExhausted;
	}

	if (_callStackTop >= _callStack.size()) {
		return Result::TrapCallStackExhausted;
	}

	memset(_valueStack.data() + _valueStackTop, 0, sizeof(Value) * extraStackSpace);

	return cb(_valueStack.data() + _valueStackTop, [&] () -> Result {
		bool locked = false;
		if (_contextLock.mutex() && !_contextLock.owns_lock()) {
			_contextLock.lock();
			locked = true;
		}

		const auto nParams = func.types.size();
		_valueStackTop += nParams;
		auto res = PushCall(module, func);
		if (res != Result::Ok) {
			return res;
		}

		res = Run(origStack);
		if (res == Result::Ok || res == Result::Returned) {
			Index nResults = func.sig->results.size();
			if (nParams != 0) {
				memmove(&_valueStack[_valueStackTop - nResults - nParams], &_valueStack[_valueStackTop - nResults], nResults * sizeof(Value));
			}
		} else {
			if (!silent) {
				onThreadError();
			}
		}

		_callStackTop = origStack;
		_valueStackTop = origValue;

		if (locked) {
			_contextLock.unlock();
		}
		return res;
	});
}

}

#endif /* SRC_THREAD_H_ */
