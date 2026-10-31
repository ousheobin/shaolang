#ifndef SHAOLANG_LINKER_H
#define SHAOLANG_LINKER_H

#include <string>

#include "codegen/target.h"

/**
 * 调用本机工具链把汇编装配、链接成可直接运行的可执行文件。
 *
 * macOS 上还须做 ad-hoc 签名，Apple Silicon 的内核会拒绝执行未签名的代码。
 */
class Linker {

private:

    static std::string shell_quote(const std::string & path);

public:

    static bool linkable_on_host(const CompileTarget & target,std::string & reason);

    static bool assemble_and_link(const std::string & assembly_path,const std::string & output_path,
                                  const CompileTarget & target,std::string & failure);
};

#endif //SHAOLANG_LINKER_H
