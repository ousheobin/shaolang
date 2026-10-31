#include "target.h"

CompileTarget host_target() {
    CompileTarget target;
    target.architecture = Architecture::ARM64;
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
    target.architecture = Architecture::ARM64;
    if(text == "macos-arm64" || text == "darwin-arm64" || text == "apple-arm64"){
        target.platform = SystemPlatform::MacOS;
        return true;
    }else if(text == "linux-arm64" || text == "aarch64-linux"){
        target.platform = SystemPlatform::Linux;
        return true;
    }
    return false;
}

string target_to_string(const CompileTarget & target) {
    switch (target.platform){
        case SystemPlatform::MacOS:   return "macos-arm64";
        case SystemPlatform::Linux:   return "linux-arm64";
        case SystemPlatform::Windows: return "windows-arm64";
    }
    return "unknown";
}

bool target_supported(const CompileTarget & target,string & reason) {
    if(target.architecture != Architecture::ARM64){
        reason = "目前只支持 ARM64 (aarch64) 指令集";
        return false;
    }
    if(target.platform == SystemPlatform::Windows){
        // Windows ARM64 需要调用方预留 shadow space，汇编格式与 ELF / Mach-O 均不相同
        reason = "windows-arm64 后端尚未实现";
        return false;
    }
    return true;
}
