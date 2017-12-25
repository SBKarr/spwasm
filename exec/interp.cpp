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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <ftw.h>
#include <dirent.h>

#include <iostream>
#include "TestEnvironment.h"

namespace wasm {

namespace host {

Result do_decrement(const Thread *thread, const HostFunc * func, Value* buffer) {
	-- buffer[0].i32;
	return Result::Ok;
}

Result do_increment(const Thread *thread, const HostFunc * func, Value* buffer) {
	++ buffer[0].i32;
	return Result::Ok;
}

}

}

void process_file_data(const char *filename, const uint8_t *data, size_t size) {
	wasm::Environment env;
	if (auto mod = env.loadModule("test", data, size)) {
		mod->printInfo(std::cout);

		auto envMod = env.getEnvModule();
		envMod->addFunc("do_decrement", { wasm::Type::I32 }, { wasm::Type::I32 }, &wasm::host::do_decrement);
		envMod->addFunc("do_increment", { wasm::Type::I32 }, { wasm::Type::I32 }, &wasm::host::do_increment);

		wasm::ThreadedRuntime runtime;
		if (runtime.init(&env, wasm::LinkingThreadOptions())) {
			if (auto func = runtime.getExportFunc("test", "plus_one")) {
				wasm::Vector<wasm::Value> buffer{ wasm::Value(uint32_t(42)) };
				if (runtime.call(*func, buffer)) {
					printf("call: %u\n", buffer[0].i32);
				}
			}
			printf("success\n");
		} else {
			printf("failed\n");
		}
	}
}

void read_file(const char *filename) {
	char buf[PATH_MAX + 1] = { 0 };

	FILE *fp;
	fp = fopen(filename, "r");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long int size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		uint8_t buf[size];
		if (fread(buf, size, 1, fp) == 1) {
			process_file_data(filename, buf, size);
		}

		fclose(fp);
	}
}

void read_file(const char *dirname, const char *filename) {
	char buf[PATH_MAX + 1] = { 0 };

	FILE *fp;
	sprintf(buf, "%s/%s", dirname, filename);
	fp = fopen(buf, "r");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long int size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		uint8_t buf[size];
		if (fread(buf, size, 1, fp) == 1) {
			wasm::StringView name(filename, strlen(filename) - 5);
			//if (name == "endianness") {
				if (auto mod = wasm::test::TestEnvironment::getInstance()->loadModule(name, buf, size)) {
					//std::cout << "Module " << name << " loaded\n";
					//mod->printInfo(std::cout);
				} else {
					//std::cout << "===== Fail to load module: " << name << " =====\n";
				}
			//}
		}

		fclose(fp);
	}
}

void read_assert(const char *dirname, const char *filename) {
	char buf[PATH_MAX + 1] = { 0 };

	FILE *fp;
	sprintf(buf, "%s/%s", dirname, filename);
	fp = fopen(buf, "r");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long int size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		uint8_t buf[size];
		if (fread(buf, size, 1, fp) == 1) {
			wasm::StringView name(filename, strlen(filename) - 7);
			if (wasm::test::TestEnvironment::getInstance()->loadAsserts(name, buf, size)) {
				//std::cout << "Test asserts " << name << " loaded\n";
			}
		}

		fclose(fp);
	}
}

int main(int argc, char** argv) {
	char buf[PATH_MAX + 1] = { 0 };

	char *cwd = nullptr;

	if (argc == 2) {
		cwd = realpath(argv[1], buf);

		if (cwd) {
			read_file(cwd);
		}

	} else if (argc == 3) {
		if (strcmp(argv[1], "--test-dir") == 0 || strcmp(argv[1], "-D") == 0) {

			cwd = realpath(argv[2], buf);
			if (!cwd) {
				return -1;
			}

			if (cwd) {
				DIR *dir;
				struct dirent *dp;
				dir = opendir(cwd);
				while ((dp = readdir(dir)) != NULL) {
					const char *name = dp->d_name;
					size_t len = strlen(name);
					if (len > 5 && memcmp(".wasm", name + len - 5, 5) == 0) {
						read_file(cwd, name);
					} else if (len > 7 && memcmp(".assert", name + len - 7, 7) == 0) {
						read_assert(cwd, name);
					}
				}
				closedir(dir);

				if (!wasm::test::TestEnvironment::getInstance()->run()) {
					return -1;
				}
			}
		}
	}

	return 0;
}
