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
#include "SPData.h"
#include "SPFilesystem.h"
#include "SPLog.h"
#include "ScriptApplication.h"
#include "TestApplication.h"

using namespace stappler;

static constexpr auto HELP_STRING(
R"HelpString(usage:
    script-test <source-file>
)HelpString");


int parseOptionSwitch(data::Value &ret, char c, const char *str) {
	if (c == 'h') {
		ret.setBool(true, "help");
	} else if (c == 'v') {
		ret.setBool(true, "verbose");
	}
	return 1;
}

int parseOptionString(data::Value &ret, const String &str, int argc, const char * argv[]) {
	if (str == "help") {
		ret.setBool(true, "help");
	} else if (str == "verbose") {
		ret.setBool(true, "verbose");
	} else if (str == "dir") {
		ret.setString(argv[0], "dir");
		return 2;
	}
	return 1;
}

int _spMain(argc, argv) {
	data::Value opts = data::parseCommandLineOptions(argc, argv, &parseOptionSwitch, &parseOptionString);

	bool help = opts.getBool("help");
	bool verbose = opts.getBool("verbose");

	if (verbose) {
		std::cout << " Current work dir: " << stappler::filesystem::currentDir() << "\n";
		std::cout << " Documents dir: " << stappler::filesystem::documentsPath() << "\n";
		std::cout << " Cache dir: " << stappler::filesystem::cachesPath() << "\n";
		std::cout << " Writable dir: " << stappler::filesystem::writablePath() << "\n";
		std::cout << " Options: " << stappler::data::EncodeFormat::Pretty << opts << "\n";
	}

	if (help) {
		std::cout << HELP_STRING << "\n";
		return 0;
	}

	String dir(opts.getString("dir"));
	if (!dir.empty()) {
		auto app = app::TestApplication::getInstance();
		dir = filepath::reconstructPath(filesystem::currentDir(dir));

		filesystem::ftw(dir, [&] (const String &path, bool isFile) {
			if (isFile) {
				if (filepath::lastExtension(path) == "assert") {
					app->loadAsserts(filepath::name(path), filesystem::readFile(path));
				} else if (filepath::lastExtension(path) == "wasm") {
					std::cout << path << "\n";
					app->loadModule(filepath::name(path), filesystem::readFile(path));
				}
			}
		});

		app->run();
	}

	auto &args = opts.getValue("args");
	if (args.size() == 2) {
		String path(args.getString(1));
		if (!path.empty()) {
			path = filepath::reconstructPath(filesystem::currentDir(path));
			auto data = filesystem::readFile(path);
			if (!data.empty()) {
				auto app = app::ScriptApplication::getInstance();
				app->loadModule(filepath::name(path), data);
				app->run();
			}
		}
	}

	return 0;
}
