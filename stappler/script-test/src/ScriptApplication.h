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

#ifndef SCRIPT_TEST_SRC_SCRIPTAPPLICATION_H_
#define SCRIPT_TEST_SRC_SCRIPTAPPLICATION_H_

#include "SPWasmScriptRuntime.h"
#include "wasm/SExpr.h"

NS_SP_EXT_BEGIN(app)

using namespace wasm;

class ScriptApplication {
public:
	struct Test {
		wasm::String name;
		wasm::String data;
		wasm::Vector<sexpr::Token> list;
	};

	static ScriptApplication *getInstance();

	ScriptApplication();
	~ScriptApplication();

	bool loadModule(const StringView &name, const Bytes &buf);
	void run();

protected:
	memory::MemPool _pool = memory::MemPool::ManagedRoot;
	Arc<wasm::ScriptEnvironment> _env;
};

NS_SP_EXT_END(app)

#endif /* SCRIPT_TEST_SRC_SCRIPTAPPLICATION_H_ */
