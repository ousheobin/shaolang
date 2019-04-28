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

int main(int argc, char * argv[]){
    vector<char *> files;
    if( argc > 1 ){
        for(int i = 1 ; i < argc ; i ++ ){
            char * arg = argv[i];
            if( arg[0] == '-'){
                if(!strcmp("-h",arg)){
                    CompilerArguments::showHelp = true;
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
        cout<<"shaoLang 编译器 Version "<<SHAOLANG_COMPLIER_VERSION<<endl
        <<"用法:\nshaolang 源文件 [options]\n"<<endl
        <<"Options:\n-h 显示帮助信息"<<endl;
    }else if(!files.empty()){
        for(int file_index = 0 ; file_index < files.size() ; file_index ++ ){
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