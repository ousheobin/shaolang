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

Like gcc and javac, the default job is to produce a program: shaolang assembles the intermediate code into aarch64 assembly, then runs the host toolchain to link it. Without -o the executable is written to the current directory under the source file's own name, so shaolang demo.sl leaves ./demo. Nothing is printed on the way; diagnostics are the only output.

| Option | Effect |
| --- | --- |
| -h | Print the help text |
| -t | Print the result of lexical analysis |
| -b | Print the data flow graph |
| -ir | Print the intermediate code before and after optimization, and build nothing |
| -v | Print where the executable was written and the warning and error counts |
| -S | Emit assembly only; without -o it goes to standard output |
| -o file | Name the output: an assembly file with -S, otherwise the executable |
| --target=name | Compile target, macos-arm64 or linux-arm64, defaulting to the host |

Examples:

```shell
shaolang demo.sl                       # writes and links ./demo
shaolang demo.sl -v                    # the same, plus the compile summary
shaolang demo.sl -S -o demo.s          # assembly only
shaolang demo.sl --target=linux-arm64 -S -o demo.s
shaolang demo.sl -ir                   # intermediate code, no output file
```

Both targets are arm64, and the host is assumed to be arm64 as well, which is what the driver treats as "本机". Linking runs through the host toolchain, so asking for a target that differs from the host gives the message 目标 linux-arm64 与本机系统不同，只能生成汇编文件 instead of a link failure: only -S is meaningful there. On Windows the back end declines before doing anything, since windows-arm64 is not implemented, so the Windows CI job proves that MSVC can build the compiler and nothing more.

Regression tests live in test/e2e: one .sl source plus the .expected output per case, run by test/e2e/run.sh, which builds the compiler, compiles every case and compares stdout and exit status byte for byte.
