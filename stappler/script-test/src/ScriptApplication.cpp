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

#include "SPCommon.h"
#include "ScriptApplication.h"
#include "SPLog.h"

NS_SP_EXT_BEGIN(app)

static ScriptApplication *s_sharedApp = nullptr;

ScriptApplication *ScriptApplication::getInstance() {
	if (!s_sharedApp) {
		s_sharedApp = new ScriptApplication;
	}
	return s_sharedApp;
}

ScriptApplication::ScriptApplication() {
	memory::pool::initialize();
	_env = Arc<wasm::ScriptEnvironment>::create();
	_env->init();

	memory::pool::push(_pool);
}

ScriptApplication::~ScriptApplication() {
	_env = nullptr;
	memory::pool::pop();
	memory::pool::terminate();
}

bool ScriptApplication::loadModule(const StringView &name, const Bytes &buf) {
	::wasm::ReadOptions opts;
	opts.read_debug_names = true;
	if (_env->loadModule(name, buf.data(), buf.size(), opts)) {
		return true;
	}
	return false;
}

void ScriptApplication::run() {
	log::format("Root", "allocated: %lu", _pool.getAllocatedBytes());
	log::format("Env", "allocated: %lu", _env->getPool().getAllocatedBytes());

	auto runtime = Arc<wasm::ScriptRuntime>::create();
	if (runtime->init(_env)) {
		log::format("Runtime", "allocated: %lu", runtime->getPool().getAllocatedBytes());
		log::format("System", "allocated: %lu", memory::pool::get_allocator_allocated_bytes(_env->getPool()));
		auto thread = Arc<wasm::ScriptThread>::create();
		if (thread->init(runtime, wasm::ScriptThread::Config())) {
			thread->prepare("test", "run", [&] (const RuntimeModule &module, const Func &func, Value *buf, auto cb) -> Thread::Result {
				return cb();
			});

			thread->prepare("test", "runString", [&] (const RuntimeModule &module, const Func &func, Value *buf, auto cb) -> Thread::Result {
				buf[0].i32 = thread->pushString(*module.memory[0], "test");
				return cb();
			});

			thread->prepare("test", "runString", [&] (const RuntimeModule &module, const Func &func, Value *buf, auto cb) -> Thread::Result {
				buf[0].i32 = thread->pushString(*module.memory[0], "nametest");
				return cb();
			});

			thread->prepare("test", "runString", [&] (const RuntimeModule &module, const Func &func, Value *buf, auto cb) -> Thread::Result {
				buf[0].i32 = thread->pushString(*module.memory[0], "memcmp");
				return cb();
			});

			log::format("Thread", "allocated: %lu", thread->getPool().getAllocatedBytes());
		}
		log::format("Runtime", "allocated: %lu", runtime->getPool().getAllocatedBytes());
		log::format("System", "allocated: %lu", memory::pool::get_allocator_allocated_bytes(_env->getPool()));
	}
}

NS_SP_EXT_END(app)
