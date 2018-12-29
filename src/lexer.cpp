//
// Created by Ou Sheobin on 2018/12/21.
//

#include <vector>

#include "lexer.h"

Lexer::Lexer(FileScanner &sc):scanner(sc) {
}

Lexer::~Lexer() {
    if(!current_token){
        delete current_token;
        current_token = NULL;
    }
}

bool Lexer::scan_next() {
    return true;
}

Token * Lexer::get_token() {
    return NULL;
}