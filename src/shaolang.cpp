//
// Created by Ou Sheobin on 2018/12/21.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdio>
#if defined(WINDOWS) || defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "compiler.h"
#include "commons/argument.h"
#include "commons/error.h"
#include "codegen/linker.h"
#include "codegen/target.h"
#include "shaolang.h"

using namespace std;

// 临时目录必须逐个确认能用：环境变量可能给出本机可执行文件认不出的路径，
// 比如 MSYS 一类的环境会把 TMP 原样给成 /tmp，它只对 argv 做 POSIX 到 Win32 的转换，
// 环境变量不归它管。文件名带进程号，别的程序的临时文件不会被覆盖；同一进程内
// 多个源文件再按下标区分。
#if defined(WINDOWS) || defined(_WIN32)
static bool usable_directory(const string & dir){
    DWORD attributes = GetFileAttributesA(dir.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
#else
static bool usable_directory(const string & dir){
    struct stat info;
    return stat(dir.c_str(), &info) == 0 && S_ISDIR(info.st_mode)
        && access(dir.c_str(), R_OK | W_OK | X_OK) == 0;
}
#endif

static string temp_assembly_path(int index){
    vector<string> candidates;
#if defined(WINDOWS) || defined(_WIN32)
    // GetTempPath 自己按 TMP、TEMP、Windows\Temp 的次序找，返回的一定是 Win32 路径
    char buffer[MAX_PATH];
    DWORD length = GetTempPathA(MAX_PATH, buffer);
    if(length > 0 && length < MAX_PATH){
        candidates.push_back(string(buffer, length));
    }
#else
    const char * names[] = { "TMPDIR", "TMP", "TEMP" };
    for(int i = 0 ; i < 3 ; i ++ ){
        const char * value = getenv(names[i]);
        if(value != NULL && value[0] != '\0'){
            candidates.push_back(value);
        }
    }
    candidates.push_back("/var/tmp");
    candidates.push_back("/tmp");
#endif
    // 全都不能用就写在当前位置，链接失败时至少还有汇编可看
    candidates.push_back(".");
    string base;
    for(unsigned long i = 0 ; i < candidates.size() ; i ++ ){
        base = candidates[i];
        if(base.size() > 1){
            // 结尾分隔符统一剥掉，驱动器根 C:\ 这种要留着，否则拼出 C:tmp
            while(base.size() > 1 && (base[base.size() - 1] == '/' || base[base.size() - 1] == '\\')){
                base.erase(base.size() - 1);
            }
        }
        if(usable_directory(base)){
            break;
        }
    }
    stringstream ss;
#if defined(WINDOWS) || defined(_WIN32)
    ss << base << "\\" << "shaolang-" << _getpid() << "-" << index << ".s";
#else
    ss << base << "/" << "shaolang-" << getpid() << "-" << index << ".s";
#endif
    return ss.str();
}

static string default_output_path(const char * source_path){
    string name(source_path);
    size_t separator = name.find_last_of("/\\");
    if(separator != string::npos){
        name = name.substr(separator + 1);
    }
    size_t dot = name.find_last_of('.');
    if(dot != string::npos && dot != 0){
        name = name.substr(0, dot);
    }
    return name;
}

// Windows 的可执行文件必须带 .exe，否则 CreateProcess 拒载
static string with_exe_suffix(const string & path){
    size_t dot = path.find_last_of('.');
    size_t separator = path.find_last_of("/\\");
    if(dot == string::npos || (separator != string::npos && dot < separator)){
        return path + ".exe";
    }
    return path;
}

static void show_usage(){
    cout << "shaoLang 编译器 Version " << SHAOLANG_COMPLIER_VERSION << endl
         << "用法:\nshaolang 源文件 [options]" << endl << endl
         << "Options:" << endl
         << "-h                显示帮助信息" << endl
         << "-t                显示词法分析结果" << endl
         << "-b                显示数据流图" << endl
         << "-ir               显示中间代码，不生成目标文件" << endl
         << "-v                显示编译统计信息" << endl
         << "-S                只生成目标机汇编指令" << endl
         << "-o <文件>         输出文件名：配合 -S 写汇编文件，否则写可执行文件" << endl
         << "--target=<目标>   编译目标：macos-arm64 / linux-arm64 / macos-x86 / linux-x86 / " << endl
         << "                  windows-x86 / linux-x64 / windows-x64，缺省为本机" << endl << endl
         << "缺省装配并链接为可执行文件，未指定 -o 时在当前位置写出与源文件同名的文件" << endl;
}

int main(int argc, char * argv[]){
    vector<char *> files;
    CompileTarget target = host_target();
    string output_path = "";
    bool has_output_path = false;
    if( argc > 1 ){
        for(int i = 1 ; i < argc ; i ++ ){
            char * arg = argv[i];
            if( arg[0] == '-'){
                if(!strcmp("-h",arg)){
                    CompilerArguments::showHelp = true;
                }else if(!strcmp("-t",arg)){
                    CompilerArguments::showLexicalResult = true;
                }else if(!strcmp("-b",arg)){
                    CompilerArguments::showDataFlowBlocks = true;
                }else if(!strcmp("-ir",arg)){
                    CompilerArguments::showIntermediateCode = true;
                }else if(!strcmp("-v",arg)){
                    CompilerArguments::showCompileSummary = true;
                }else if(!strcmp("-S",arg)){
                    CompilerArguments::emitAssembly = true;
                }else if(!strcmp("-o",arg)){
                    if(i + 1 >= argc){
                        cout << "-o 需要指定输出文件名 "<<endl;
                        return 2;
                    }
                    output_path = argv[++i];
                    has_output_path = true;
                }else if(!strncmp(arg,"--target=",9)){
                    if(!parse_target(arg + 9,target)){
                        cout << "不支持的编译目标：" << (arg + 9)
                             << "，请使用 shaolang -h 查询" <<endl;
                        return 2;
                    }
                }else{
                    cout << "无效指令，请使用 shaolang -h 查询 "<<endl;
                    return 2;
                }
            }else{
                files.push_back(arg);
            }
        }
    }
    string unsupported_reason;
    if(!target_supported(target,unsupported_reason)){
        cout << unsupported_reason <<endl;
        return 2;
    }
    CompilerArguments::generateExecutable = !CompilerArguments::emitAssembly
                                            && !CompilerArguments::showIntermediateCode;
    if(has_output_path && files.size() > 1){
        cout << "-o 只能配合单个源文件使用 "<<endl;
        return 2;
    }
    if(CompilerArguments::showHelp){
        show_usage();
    }else if(!files.empty()){
        for(int file_index = 0 ; file_index < (int)files.size() ; file_index ++ ){
            string file_output = has_output_path ? output_path : default_output_path(files[file_index]);
#if defined(WINDOWS) || defined(_WIN32)
            if(CompilerArguments::generateExecutable){
                file_output = with_exe_suffix(file_output);
            }
#endif
            int errors_before = Error::get_error_amount();
            FileScanner * scanner = new FileScanner(files[file_index]);
            Lexer * lexer = new Lexer( * scanner);
            Compiler * compiler = new Compiler(lexer,scanner);
            compiler -> compile();
            bool has_error = Error::get_error_amount() > errors_before;
            if(has_error && (CompilerArguments::emitAssembly || CompilerArguments::generateExecutable)){
                cout << "源文件存在错误，没有生成目标代码 "<<endl;
            }else if(CompilerArguments::emitAssembly){
                if(has_output_path){
                    ofstream file(output_path.c_str());
                    if(!file.is_open()){
                        cout << "无法打开输出文件：" << output_path <<endl;
                        delete compiler;
                        return 2;
                    }
                    compiler -> emit_assembly(file,target);
                    file.close();
                }else{
                    compiler -> emit_assembly(cout,target);
                }
            }else if(CompilerArguments::generateExecutable){
                string reason;
                if(!Linker::linkable_on_host(target,reason)){
                    cout << reason << "，请改用 -S 生成汇编文件 "<<endl;
                }else{
                    // 汇编中间文件写在临时目录，链接一结束就删
                    string assembly_path = temp_assembly_path(file_index);
                    ofstream file(assembly_path.c_str());
                    if(!file.is_open()){
                        cout << "无法写入临时文件：" << assembly_path <<endl;
                        delete compiler;
                        return 2;
                    }
                    compiler -> emit_assembly(file,target);
                    file.close();
                    string failure;
                    bool linked = Linker::assemble_and_link(assembly_path,file_output,target,failure);
                    // 中间汇编不留在这台机器上：链接成败都删。工具链的报错已经收进 failure，
                    // 真要看汇编用 -S，那才是留给用户的产物
                    remove(assembly_path.c_str());
                    if(linked){
                        if(CompilerArguments::showCompileSummary){
                            cout << "已生成可执行文件：" << file_output <<endl;
                        }
                    }else{
                        cout << failure <<endl;
                    }
                }
            }
            delete compiler;
        }
        if(CompilerArguments::showCompileSummary){
            int warning = Error::get_warning_amount();
            int error = Error::get_error_amount();
            cout << "编译完成: "<< warning <<((warning>1)?" Warnings":" Warning") << ","
                 << error <<((error>1)?" Errors":" Error")<<" ."<<endl;
        }
    }else{
        cout << "无效指令，请使用 shaolang -h 查询 "<<endl;
        return 2;
    }

    return 0;
}
