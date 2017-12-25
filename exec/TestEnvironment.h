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

#ifndef EXEC_TESTENVIRONMENT_H_
#define EXEC_TESTENVIRONMENT_H_

#include "src/RuntimeEnvironment.h"
#include "SExpr.h"

namespace wasm {
namespace test {

class TestEnvironment : public Environment {
public:
	struct Test {
		String name;
		String data;
		Vector<sexpr::Token> list;
	};

	static TestEnvironment *getInstance();

	TestEnvironment();

	bool run();
	bool loadAsserts(const StringView &, const uint8_t *, size_t);

protected:
	bool runTest(wasm::ThreadedRuntime &, const Test &);

	HostModule *_testModule = nullptr;
	Vector<Test> _tests;
};

}
}

#endif /* EXEC_TESTENVIRONMENT_H_ */
