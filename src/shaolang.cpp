//
// Created by Ou Sheobin on 2018/12/21.
//

#include <iostream>
#include <vector>
#include <cstring>
#include "compiler.h"
#include "commons/argument.h"
#include "commons/error.h"
#include "shaolang.h"

using namespace std;

static void show_usage(){
    cout << "shaoLang 编译器 Version " << SHAOLANG_COMPLIER_VERSION << endl
         << "用法:\nshaolang 源文件 [options]" << endl << endl
         << "Options:" << endl
         << "-h          显示帮助信息" << endl
         << "-t          显示词法分析结果" << endl
         << "-b          显示数据流图" << endl;
}

int main(int argc, char * argv[]){
    vector<char *> files;
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
                }else{
                    cout << "无效指令，请使用 shaolang -h 查询 "<<endl;
                    return 2;
                }
            }else{
                files.push_back(arg);
            }
        }
    }
    if(CompilerArguments::showHelp){
        show_usage();
    }else if(!files.empty()){
        for(int file_index = 0 ; file_index < (int)files.size() ; file_index ++ ){
            FileScanner * scanner = new FileScanner(files[file_index]);
            Lexer * lexer = new Lexer( * scanner);
            Compiler * compiler = new Compiler(lexer,scanner);
            compiler -> compile();
            delete compiler;
        }
        int warning = Error::get_warning_amount();
        int error = Error::get_error_amount();
        cout << "编译完成: "<< warning <<((warning>1)?" Warnings":" Warning") << ","
             << error <<((error>1)?" Errors":" Error")<<" ."<<endl;
    }else{
        cout << "无效指令，请使用 shaolang -h 查询 "<<endl;
        return 2;
    }

    return 0;
}
