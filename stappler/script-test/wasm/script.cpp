#include <WasmScript/String.h>
#include <WasmScript/Math.h>
#include <WasmScript/Memory.h>

WASM_SCRIPT_MAGIC

using namespace script;

WASM_EXPORT void run() {
	auto pool = mempool::create(nullptr);

	char *ptr = (char *)mempool::palloc(pool, 128);
	memcpy(ptr, "Hello world\n", 12);
	ws_print(ptr);

	mempool::destroy(pool);
}

WASM_EXPORT void runString(const char *str) {
	if (strcmp(str, "test") == 0) {
		ws_print("test\n");
	} else if (strncmp(str, "namestr", 4) == 0) {
		ws_print("namestr\n");
	} else if (memcmp(str, "memcmp", 6) == 0) {
		ws_print("memcmp\n");
	} else {
		ws_print("unknown\n");
	}
}

WASM_EXPORT float export_float_nan() {
	return script::nan();
}

WASM_EXPORT double export_double_nan() {
	return script::nan<double>();
}


/*

WASM_IMPORT void import_function(int);
WASM_IMPORT void use_var(test_struct *);

const char *STRING_1 = "string1";
char *STRING_2 = "string2";

test_struct g_struct = {
	.valueChar = -1,
	.valueShort = -2,
	.valueInt = -3,
	.valueLong = -4,
	.valueLongLong = -5,
	.valueUnsignedChar = 1,
	.valueUnsignedShort = 2,
	.valueUnsignedInt = 3,
	.valueUnsignedLong = 4,
	.valueUnsignedLongLong = 5,
	.valueFloat = 123.0f,
	.valueDouble = 5.0,
	.valueCharPointer = "StringValue"
};

WASM_IMPORT test_struct e_struct;
WASM_IMPORT test_struct e_struct2;

WASM_EXPORT double test2() {
	test_struct str;
	use_var(&str);
	return str.valueDouble;
}

WASM_EXPORT int64_t get_global_value() {
	return 12345;
}

const char *get_string1() {
	return STRING_1;
}

char *get_string2() {
	return STRING_2;
}

test_struct *get_struct2() {
	return &e_struct;
}
test_struct *get_struct3() {
	return &e_struct2;
}*/
