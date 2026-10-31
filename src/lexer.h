//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_LEXER_H
#define SHAOLANG_LEXER_H

#include <vector>

#include "commons/token.h"
#include "commons/scanner.h"

class Lexer{
private:

    FileScanner & scanner;
    int current_char;
    Token * current_token;

    bool check_is_valid_char(const int ch) const;
    bool check_is_number(const int ch) const;
    bool check_is_blank(const int ch) const;

    void advance();
    void skip_blanks();
    void skip_line_comment();
    void skip_block_comment();

    Token * read_identifier();
    Token * read_string();
    Token * read_char();
    Token * read_number();
    Token * read_operator();

    Token * invalid_operator(int ch);

public:

    Lexer(FileScanner & scanner);
    ~Lexer();

    Token * next_token();
};

#endif //SHAOLANG_LEXER_H
