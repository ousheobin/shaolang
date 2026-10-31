# ShaoLang #
[![CI](https://github.com/ousheobin/shaolang/actions/workflows/ci.yml/badge.svg)](https://github.com/ousheobin/shaolang/actions/workflows/ci.yml)
[![Build status](https://ci.appveyor.com/api/projects/status/github/ousheobin/shaolang?svg=true)](https://ci.appveyor.com/project/ousheobin/shaolang)

A tiny compiler.

### Dependencies ###

- CMake (3.10 and above)
- GCC, Clang or MSVC

### Build ###

**macOS and Linux**

```shell
chmod a+x build.sh
./build.sh
```

The script configures into the build directory and runs make, leaving the driver at build/shaolang. The same two steps by hand:

```shell
cmake -S . -B build
cmake --build build
```

**Windows**

```shell
cmake -S . -B build
cmake --build build --config Release
```

Visual Studio is a multi-config generator, so the executable lands in build/Release/shaolang.exe.

### Usage ###

```shell
shaolang source-file [options]
```

Like gcc and javac, the default job is to produce a program: shaolang assembles the intermediate code into target assembly, then runs the host toolchain to link it. Without -o the executable is written to the current directory under the source file's own name, so shaolang demo.sl leaves ./demo. Nothing is printed on the way; diagnostics are the only output.

| Option | Effect |
| --- | --- |
| -h | Print the help text |
| -t | Print the result of lexical analysis |
| -b | Print the data flow graph |
| -ir | Print the intermediate code before and after optimization, and build nothing |
| -v | Print where the executable was written and the warning and error counts |
| -S | Emit assembly only; without -o it goes to standard output |
| -o file | Name the output: an assembly file with -S, otherwise the executable |
| --target=name | Compile target: macos-arm64, linux-arm64, macos-x86, linux-x86, windows-x86, linux-x64, windows-x64, defaulting to the host |

Examples:

```shell
shaolang demo.sl                       # writes and links ./demo
shaolang demo.sl -v                    # the same, plus the compile summary
shaolang demo.sl -S -o demo.s          # assembly only
shaolang demo.sl --target=linux-arm64 -S -o demo.s
shaolang demo.sl -ir                   # intermediate code, no output file
```

Seven targets cover three architectures: arm64 on macOS and Linux, x86 on macOS, Linux and Windows, x64 on Linux and Windows. windows-arm64 stays out of scope. Linking runs through the host toolchain, so a target whose platform or word size differs from the host is declined before the toolchain is invoked: the driver reports that only assembly can be generated and exits without a link failure, which makes -S the only meaningful option there. Each host therefore links its own target natively, and the remaining combinations are checked as far as the assembler: the CI matrix runs the suite on linux-arm64, linux-x64, macos-arm64 and, under MSYS2, windows-x86 and windows-x64, while test/e2e/run.sh cross-assembles every target the local toolchain is able to target.

Regression tests live in test/e2e: one .sl source plus the .expected output per case, run by test/e2e/run.sh, which builds the compiler, compiles every case and compares stdout and exit status byte for byte.
