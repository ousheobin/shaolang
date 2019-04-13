//
// Created by Ou Sheobin on 2019/1/26.
//

#include "compiler.h"
#include <iostream>

Compiler::Compiler(Lexer *lexer_ptr, FileScanner *scanner_ptr) {
    this -> lexer = lexer_ptr;
    this -> scanner = scanner_ptr;
    this -> parser = NULL;
    this -> symbolTable = new SymbolTable();
    this -> irGenerator = new IRGenerator(symbolTable);
}

void Compiler::compile() {
    if(parser){
        delete parser;
    }
    parser = new Parser(lexer,symbolTable,irGenerator);
    parser -> do_anlayse();
    symbolTable -> show_all_code();
}

Compiler::~Compiler() {
    if(parser){
        delete parser;
    }
    if(lexer){
        delete lexer;
    }
    if(scanner){
        delete scanner;
    }
}
