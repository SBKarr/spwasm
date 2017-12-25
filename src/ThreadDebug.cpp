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

void Thread::PrintStackFrame(std::ostream &stream, const CallStackFrame &frame, Index maxOpcodes) const {
	StringView modName = _runtime->getModuleName(frame.module);
	auto funcName = _runtime->getModuleFunctionName(*frame.module, frame.func);

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

		Index position = frame->func->opcodes.data() - frame->position;
		Index nOpcodes = std::min(maxOpcodes, position);

	}
}

}
