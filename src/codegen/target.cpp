#include "target.h"

CompileTarget host_target() {
    CompileTarget target;
#if defined(_M_ARM64) || defined(__aarch64__)
    target.architecture = Architecture::ARM64;
#elif defined(_M_X64) || defined(__x86_64__)
    target.architecture = Architecture::X64;
#elif defined(_M_IX86) || defined(__i386__)
    target.architecture = Architecture::X86;
#else
    // MSVC 的架构宏与 GCC/Clang 的不同，两套都认；认不出来只能按主开发平台兜底
    target.architecture = Architecture::ARM64;
#endif
#if defined(WINDOWS) || defined(_WIN32) || defined(_M_ARM64)
    target.platform = SystemPlatform::Windows;
#elif defined(LINUX) || defined(__linux__) || defined(__ANDROID__)
    target.platform = SystemPlatform::Linux;
#elif defined(MAC_OSX) || defined(__APPLE__)
    target.platform = SystemPlatform::MacOS;
#else
    // WINDOWS / LINUX / MAC_OSX 由 CMake 注入，编译器自身的宏保证裸命令编译也能识别
    #error "无法识别本机操作系统，请定义 MAC_OSX / LINUX / WINDOWS 之一"
#endif
    return target;
}

bool parse_target(const string & text,CompileTarget & target) {
    if(text == "macos-arm64" || text == "darwin-arm64" || text == "apple-arm64"){
        target.architecture = Architecture::ARM64;
        target.platform = SystemPlatform::MacOS;
        return true;
    }else if(text == "linux-arm64" || text == "aarch64-linux"){
        target.architecture = Architecture::ARM64;
        target.platform = SystemPlatform::Linux;
        return true;
    }else if(text == "linux-x86" || text == "i386-linux" || text == "x86-linux"){
        target.architecture = Architecture::X86;
        target.platform = SystemPlatform::Linux;
        return true;
    }else if(text == "linux-x64" || text == "x86_64-linux" || text == "amd64-linux"){
        target.architecture = Architecture::X64;
        target.platform = SystemPlatform::Linux;
        return true;
    }else if(text == "windows-x86" || text == "win-x86" || text == "i386-windows"){
        target.architecture = Architecture::X86;
        target.platform = SystemPlatform::Windows;
        return true;
    }else if(text == "windows-x64" || text == "win-x64" || text == "amd64-windows"){
        target.architecture = Architecture::X64;
        target.platform = SystemPlatform::Windows;
        return true;
    }else if(text == "macos-x86" || text == "darwin-x86" || text == "osx-x86"){
        target.architecture = Architecture::X86;
        target.platform = SystemPlatform::MacOS;
        return true;
    }
    // 兜底：--target= 解析失败时调用方直接报错，这里的初值只为避免未初始化
    target.architecture = Architecture::ARM64;
    target.platform = SystemPlatform::MacOS;
    return false;
}

string target_to_string(const CompileTarget & target) {
    string system;
    switch (target.platform){
        case SystemPlatform::MacOS:   system = "macos"; break;
        case SystemPlatform::Linux:   system = "linux"; break;
        case SystemPlatform::Windows: system = "windows"; break;
    }
    string architecture;
    switch (target.architecture){
        case Architecture::ARM64: architecture = "arm64"; break;
        case Architecture::X86:   architecture = "x86"; break;
        case Architecture::X64:   architecture = "x64"; break;
    }
    if(system.empty() || architecture.empty()){
        return "unknown";
    }
    return system + "-" + architecture;
}

bool target_supported(const CompileTarget & target,string & reason) {
    // 语言在各目标上都是 64 位整数语义，x86 用双字槽与软件除法承载
    if(target.architecture == Architecture::ARM64 && target.platform != SystemPlatform::Windows){
        return true;
    }
    if(target.architecture == Architecture::X86){
        return true;
    }
    // macos-x64 还没实现：Mach-O 的下划线符号与段名要另一套路出逻辑
    if(target.architecture == Architecture::X64 && target.platform != SystemPlatform::MacOS){
        return true;
    }
    if(target.architecture == Architecture::ARM64 && target.platform == SystemPlatform::Windows){
        // 语言只在 macOS 与 Linux 的 ARM64 上验证过，Windows ARM64 不在支持计划里
        reason = "不支持 windows-arm64，Windows 的目标只有 windows-x86 与 windows-x64";
        return false;
    }
    reason = "不支持 " + target_to_string(target) + "，目标只有 macos-arm64 / linux-arm64 / "
             "macos-x86 / linux-x86 / windows-x86 / linux-x64 / windows-x64";
    return false;
}
