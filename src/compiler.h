//
// Created by Ou Sheobin on 2019/1/26.
//
#ifndef SHAOLANG_COMPILER_H
#define SHAOLANG_COMPILER_H

#include "commons/argument.h"

#include "lexer.h"
#include "parser.h"
#include "symbols/symbol_table.h"
#include "ir/inter_code.h"
#include "ir_generator.h"
#include "commons/scanner.h"
#include "commons/error.h"

class Compiler {

private:

    vector<char *> files;

    Lexer * lexer;
    Parser * parser;
    FileScanner * scanner;
    SymbolTable * symbolTable;
    IRGenerator * irGenerator;

public:

    Compiler(Lexer * lexer_ptr,FileScanner * scanner_ptr);
    ~Compiler();
    void compile();
};


#endif //SHAOLANG_COMPILER_H
