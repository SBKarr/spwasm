spwasm - standalone portable Webassembly interpreter

# Install

To build interpreter static library, run `make lib`

# Test

## Build tests

Requirements:

- Binaryen
- WABT

If WABT and Binaryen already installed, run

```
make test-exec BINARYEN_BIN=<Binaryen binary dir> WABT_BIN=<WABT bin dir> RELEASE=1
```

If not intalled - you can use submodules:

```
git submodule update --init --recursive

# Build wabt
cd wabt
make
cd -

# Build binaryen
cd binaryen
cmake .; make
cd -

make test-exec RELEASE=1
```

## Run tests

Test executeble read all .wasm files from dir as WebAssembly modules,
and all .assert files as test scripts

Each module then linked into single runtime. Then, each module is
tested with it's script.

For internal interpreter tests, run:
```
build/release/wasm-interp -D build/test
```

You can write you own tests with simple S-expressions:

```
(assert_return (invoke "<function_name>" (i32.const 97) <other arguments> ... ) <return value>)

;; or

(assert_trap (invoke "<function_name>" <arguments> ...) "<error name>")
```
