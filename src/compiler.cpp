//
// Created by Ou Sheobin on 2019/1/26.
//

#include "compiler.h"
#include <iostream>

Compiler::Compiler(Lexer *lexer_ptr, FileScanner *scanner_ptr) {
    this -> lexer = lexer_ptr;
    this -> scanner = scanner_ptr;
}

void Compiler::compile() {
    Token * token = NULL;
    token = lexer->next_token();
    do{
        if(CompilerArguments::showLexicalResult){
            cout<<token->to_string()<<endl;
        }
        token = lexer->next_token();
    }while (token != NULL && token->type != LexicalType::END);
}

Compiler::~Compiler() {
    if(lexer){
        delete lexer;
    }
    if(scanner){
        delete scanner;
    }
}
