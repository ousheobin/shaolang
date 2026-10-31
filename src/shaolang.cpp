//
// Created by Ou Sheobin on 2018/12/21.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "compiler.h"
#include "commons/argument.h"
#include "commons/error.h"
#include "codegen/linker.h"
#include "codegen/target.h"
#include "shaolang.h"

using namespace std;

static string temp_assembly_path(int index){
    const char * directory = getenv("TMPDIR");
    string base = (directory != NULL && directory[0] != '\0') ? string(directory) : string("/tmp");
    if(base[base.size() - 1] == '/'){
        base.erase(base.size() - 1);
    }
    stringstream ss;
    ss << base << "/shaolang-" << getpid() << "-" << index << ".s";
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
         << "--target=<目标>   编译目标，支持 macos-arm64 / linux-arm64，缺省为本机" << endl << endl
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
                    // 汇编中间文件写在系统临时目录，链接成功后立即删除
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
                    if(Linker::assemble_and_link(assembly_path,file_output,target,failure)){
                        if(CompilerArguments::showCompileSummary){
                            cout << "已生成可执行文件：" << file_output <<endl;
                        }
                        remove(assembly_path.c_str());
                    }else{
                        cout << failure << "，汇编保留在 " << assembly_path <<endl;
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
