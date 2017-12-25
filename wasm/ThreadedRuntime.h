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

#ifndef SRC_RUNTIMEENVIRONMENT_H_
#define SRC_RUNTIMEENVIRONMENT_H_

#include "Environment.h"
#include "Thread.h"

namespace wasm {

struct LinkingThreadOptions : LinkingPolicy {
	uint32_t valueStackSize = Thread::kDefaultValueStackSize;
	uint32_t callStackSize = Thread::kDefaultCallStackSize;
};

class ThreadedRuntime : public Runtime {
public:
	virtual ~ThreadedRuntime() { }
	ThreadedRuntime();

	bool init(const Environment *, const LinkingThreadOptions & = LinkingThreadOptions());

	const Func *getExportFunc(const StringView &module, const StringView &name) const;
	const Func *getExportFunc(const RuntimeModule &module, const StringView &name) const;

	const RuntimeGlobal *getGlobal(const StringView &module, const StringView &name) const;
	const RuntimeGlobal *getGlobal(const RuntimeModule &module, const StringView &name) const;

	bool setGlobal(const StringView &module, const StringView &name, const Value &);
	bool setGlobal(const RuntimeModule &module, const StringView &name, const Value &);

	bool call(const RuntimeModule &module, const Func &, Vector<Value> &paramsInOut);
	bool call(const RuntimeModule &module, const Func &, Value *paramsInOut);

	bool call(const Func &, Vector<Value> &paramsInOut);
	bool call(const Func &, Value *paramsInOut);

	Thread::Result callSafe(const RuntimeModule &module, const Func &, Vector<Value> &paramsInOut);
	Thread::Result callSafe(const RuntimeModule &module, const Func &, Value *paramsInOut);

	Thread::Result callSafe(const Func &, Vector<Value> &paramsInOut);
	Thread::Result callSafe(const Func &, Value *paramsInOut);

	virtual void onError(StringStream &) const;
	virtual void onThreadError(const Thread &) const;

protected:
	bool _silent = false;
	Thread _mainThread;
};

}

#endif /* SRC_RUNTIMEENVIRONMENT_H_ */
