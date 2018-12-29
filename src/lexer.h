//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_LEXER_H
#define SHAOLANG_LEXER_H

#include <vector>

#include "token.h"
#include "keyword.h"
#include "scanner.h"

using namespace std;

class Lexer{
private:
    static KeyWord keyWord;
    FileScanner & scanner;
    char current_char = '\0';
    Token * current_token;
public:
    Lexer(FileScanner & scanner);
    ~Lexer();
    bool scan_next();
    Token * get_token();
};

#endif //SHAOLANG_LEXER_H
