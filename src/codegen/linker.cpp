#include "linker.h"

#include <cstdlib>
#include <sstream>

std::string Linker::shell_quote(const std::string & path) {
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
}

bool Linker::linkable_on_host(const CompileTarget & target,std::string & reason) {
    CompileTarget host = host_target();
    if(host.platform != target.platform){
        std::stringstream ss;
        ss << "目标 " << target_to_string(target) << " 与本机系统不同，只能生成汇编文件，"
           << "链接需要 " << target_to_string(target) << " 的工具链";
        reason = ss.str();
        return false;
    }
    return true;
}

bool Linker::assemble_and_link(const std::string & assembly_path,const std::string & output_path,
                               const CompileTarget & target,std::string & failure) {
    // 装配与链接都交给本机工具链驱动，避免硬编码各平台的 crt、syslibroot 参数
    std::string command = "cc -o " + shell_quote(output_path) + " " + shell_quote(assembly_path);
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
