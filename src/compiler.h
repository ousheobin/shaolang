//
// Created by Ou Sheobin on 2019/1/26.
//
#ifndef SHAOLANG_COMPILER_H
#define SHAOLANG_COMPILER_H

#include "argument.h"

#include "lexer.h"
#include "parser.h"
#include "scanner.h"
#include "error.h"

class Compiler {

private:

    vector<char *> files;

    Lexer * lexer;
    Parser * parser;
    FileScanner * scanner;

public:

    Compiler(Lexer * lexer_ptr,FileScanner * scanner_ptr);
    ~Compiler();
    void compile();
};


#endif //SHAOLANG_COMPILER_H
