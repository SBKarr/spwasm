/*
 * Copyright 2018 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef SCRIPT_TEST_SRC_TESTAPPLICATION_H_
#define SCRIPT_TEST_SRC_TESTAPPLICATION_H_

#include "SPWasmRuntime.h"
#include "wasm/SExpr.h"

NS_SP_EXT_BEGIN(app)

class TestApplication {
public:
	struct Test {
		wasm::String name;
		wasm::String data;
		wasm::Vector<sexpr::Token> list;
	};

	static TestApplication *getInstance();

	TestApplication();
	~TestApplication();

	bool loadAsserts(const StringView &name, const Bytes &buf);
	bool loadModule(const StringView &name, const Bytes &buf);
	bool runTest(wasm::Runtime *runtime, wasm::Thread *thread, const Test &test);
	void run();

protected:
	memory::MemPool _pool = memory::MemPool::ManagedRoot;
	memory::MemPool _testPool = memory::MemPool::None;
	Arc<wasm::Environment> _env;
	wasm::Vector<Test> _tests;
};

NS_SP_EXT_END(app)

#endif /* SCRIPT_TEST_SRC_TESTAPPLICATION_H_ */
