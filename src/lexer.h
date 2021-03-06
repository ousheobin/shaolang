﻿//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_LEXER_H
#define SHAOLANG_LEXER_H

#include <vector>

#include "commons/token.h"
#include "commons/scanner.h"

using namespace std;

class Lexer{
private:

    FileScanner & scanner;
    char current_char = -1;
    Token * current_token;

    bool  check_is_valid_char(const char ch);
    bool  check_is_number(const char ch);
    bool  check_is_blank(const char ch);

public:

    Lexer(FileScanner & scanner);
    ~Lexer();

    Token * next_token();
};

#endif //SHAOLANG_LEXER_H
