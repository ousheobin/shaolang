# ShaoLang #
[![CI](https://github.com/ousheobin/shaolang/actions/workflows/ci.yml/badge.svg)](https://github.com/ousheobin/shaolang/actions/workflows/ci.yml)
[![Build status](https://ci.appveyor.com/api/projects/status/github/ousheobin/shaolang?svg=true)](https://ci.appveyor.com/project/ousheobin/shaolang)

A tiny compiler.

### Dependencies ###

- CMake (3.10 and above)
- GCC, Clang or MSVC
- Visual Studio or [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) to link windows-x86 and windows-x64 programs

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

Seven targets cover three architectures; windows-arm64 stays out of scope. Each host links only its own target: when the system or word size differs from the host, the driver declines before invoking the toolchain, reports that assembly is all it can generate and exits without a link failure, which makes -S the only meaningful option there. The Windows rows are MASM text, so ml or ml64 and link must already be on the PATH, meaning the compiler runs from a Developer Command Prompt or a shell that has loaded vcvarsall; the driver names the missing tool otherwise.

| Target | System | Instruction set | Assembly syntax | Toolchain needed to link |
| --- | --- | --- | --- | --- |
| macos-arm64 | macOS | AArch64 | GNU assembler | Xcode command line tools |
| macos-x86 | macOS | i386 | GNU assembler | Xcode command line tools with an i386 SDK |
| linux-arm64 | Linux | AArch64 | GNU assembler | gcc or clang, plus binutils |
| linux-x86 | Linux | i386 | GNU assembler | a 32-bit capable gcc, plus binutils |
| linux-x64 | Linux | x86-64 | GNU assembler | gcc or clang, plus binutils |
| windows-x86 | Windows | i386 | MASM | Visual Studio: ml and link |
| windows-x64 | Windows | x86-64 | MASM | Visual Studio: ml64 and link |

Regression tests live in test/e2e: one .sl source plus the .expected output per case, run by test/e2e/run.sh, which builds the compiler, compiles every case and compares stdout and exit status byte for byte.
