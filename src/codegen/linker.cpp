#include "linker.h"

#include <cstdlib>
#include <sstream>

std::string Linker::shell_quote(const std::string & path) {
#if defined(WINDOWS) || defined(_WIN32)
    // Windows 走 cmd.exe，单引号不是引用符，路径里有空格时只能用双引号
    std::string quoted = "\"";
    for(unsigned long i = 0 ; i < path.size() ; i ++ ){
        if(path[i] == '"'){
            quoted += "\\\"";
        }else{
            quoted += path[i];
        }
    }
    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for(unsigned long i = 0 ; i < path.size() ; i ++ ){
        if(path[i] == '\''){
            quoted += "'\\''";
        }else{
            quoted += path[i];
        }
    }
    quoted += "'";
    return quoted;
#endif
}

bool Linker::linkable_on_host(const CompileTarget & target,std::string & reason) {
    CompileTarget host = host_target();
    if(host.platform != target.platform || host.architecture != target.architecture){
        std::stringstream ss;
        ss << "目标 " << target_to_string(target) << " 与本机 " << target_to_string(host) << " 不同，只能生成汇编文件，"
           << "链接需要 " << target_to_string(target) << " 的工具链";
        reason = ss.str();
        return false;
    }
    return true;
}

bool Linker::assemble_and_link(const std::string & assembly_path,const std::string & output_path,
                               const CompileTarget & target,std::string & failure) {
    // 装配与链接都交给本机工具链驱动，避免硬编码各平台的 crt、syslibroot 参数。
    // Windows 上没有 cc 这个驱动名，MinGW 装的是 gcc
    std::string command = std::string(
#if defined(WINDOWS) || defined(_WIN32)
            "gcc -o "
#else
            "cc -o "
#endif
            ) + shell_quote(output_path) + " " + shell_quote(assembly_path);
    if(target.platform == SystemPlatform::MacOS){
        // 32 位与 64 位的指令集不同，必须显式指定，否则工具链按本机架构装配必然失败
        if(target.architecture == Architecture::X86){
            command += " -arch i386";
        }else if(target.architecture == Architecture::X64){
            command += " -arch x86_64";
        }
    }
    if(target.platform == SystemPlatform::Linux && target.architecture == Architecture::X86){
        // 静态数据用的是绝对地址引用，发行版默认的 PIE 链接会拒绝这类重定位
        command += " -no-pie";
    }
    if(std::system(command.c_str()) != 0){
        failure = "链接失败：" + command;
        return false;
    }
    if(target.platform == SystemPlatform::MacOS){
        // ad-hoc 签名，签名标识与文件内容绑定，改完二进制必须重签
        command = "codesign --force --sign - " + shell_quote(output_path) + " 2>/dev/null";
        if(std::system(command.c_str()) != 0){
            failure = "签名失败：" + command;
            return false;
        }
    }
    return true;
}
