#ifndef SHAOLANG_TARGET_H
#define SHAOLANG_TARGET_H

#include <string>

using namespace std;

enum class Architecture {
    ARM64
};

enum class SystemPlatform {
    MacOS,
    Linux,
    Windows
};

struct CompileTarget {
    Architecture architecture;
    SystemPlatform platform;
};

// 与本机运行环境一致的目标，由编译时的 MAC_OSX / LINUX / WINDOWS 宏决定
CompileTarget host_target();

// 解析 --target= 的取值，例如 macos-arm64 / linux-arm64
bool parse_target(const string & text,CompileTarget & target);

string target_to_string(const CompileTarget & target);

bool target_supported(const CompileTarget & target,string & reason);

#endif //SHAOLANG_TARGET_H
