#include "linker.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace {

// 工具链在不在 PATH 上，用 where 探一次；ml.exe 与 link.exe 只有加载了
// vcvarsall.bat 的终端（开发者命令提示符）才找得到
bool tool_available(const std::string & tool) {
    return std::system(("where " + tool + " >nul 2>nul").c_str()) == 0;
}

std::string object_path_of(const std::string & assembly_path) {
    std::string path = assembly_path;
    size_t dot = path.rfind('.');
    size_t separator = path.find_last_of("/\\");
    if(dot != std::string::npos && (separator == std::string::npos || dot > separator)){
        path.erase(dot);
    }
    return path + ".obj";
}

}

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
    if(target.platform == SystemPlatform::Windows){
        return link_coff(assembly_path,output_path,target,failure);
    }
    // 装配与链接都交给本机工具链驱动，避免硬编码各平台的 crt、syslibroot 参数。
    std::string command = "cc -o " + shell_quote(output_path) + " " + shell_quote(assembly_path);
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

bool Linker::link_coff(const std::string & assembly_path,const std::string & output_path,
                       const CompileTarget & target,std::string & failure) {
    std::string assembler = target.architecture == Architecture::X64 ? "ml64.exe" : "ml.exe";
    if(!tool_available(assembler) || !tool_available("link.exe")){
        std::stringstream ss;
        ss << "找不到 " << (tool_available(assembler) ? "link.exe" : assembler)
           << "：windows 目标的装配与链接要用 Visual Studio 自带的 MASM 和链接器，"
           << "请在「开发者命令提示符」（已加载 vcvarsall.bat）里运行，"
           << "装好 Visual Studio 或 Build Tools 即可，也可改用 -S 只生成汇编文件";
        failure = ss.str();
        return false;
    }
    // 目标文件只是链接的中间产物，和汇编一样写完就用完，不留在这台机器上
    std::string object = object_path_of(assembly_path);
    std::string command = assembler + " /nologo /c";
    if(target.architecture != Architecture::X64){
        // 32 位的 ml.exe 默认把标识符映射成大写，非本地名要靠 /Cx 保住原样，
        // 否则和 CRT 里的 _printf 一类的名字对不上；ml64 没有 /Cu 这个选项，
        // 大小写本来就是保留的
        command += " /Cx";
    }
    command += " /Fo" + shell_quote(object) + " " + shell_quote(assembly_path);
    if(std::system(command.c_str()) != 0){
        std::remove(object.c_str());
        failure = "装配失败：" + command;
        return false;
    }
    // VS2015 起 CRT 拆成三块：msvcrt.lib 是本机 CRT 启动代码（mainCRTStartup 在这里），
    // ucrt.lib 是 C 库（含 printf / scanf），vcruntime.lib 是 C++ 运行库。
    // 名字不需要任何改写：MASM 的 C 约定在 32 位给外部名补下划线，而 32 位的 CRT 导入库
    // 用的正是 _printf 这种带下划线的导出名，两边天然一致；64 位哪边都不补。
    // 先前那两个方向都给的 alternatename 是让 _printf 和 printf 互相指认，链接器解不出
    // 这种环，于是要么不改写、要么把本来能解析的引用改到没有的那个名字上去。
    command = "link.exe /nologo /SUBSYSTEM:CONSOLE /OUT:" + shell_quote(output_path)
              + " " + shell_quote(object)
              + " msvcrt.lib ucrt.lib vcruntime.lib";
    bool linked = std::system(command.c_str()) == 0;
    std::remove(object.c_str());
    if(!linked){
        failure = "链接失败：" + command;
    }
    return linked;
}
