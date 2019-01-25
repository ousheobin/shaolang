//
// Created by Ou Sheobin on 2018/12/28.
//

#ifndef SHAOLANG_COMMON_H
#define SHAOLANG_COMMON_H

#include <string>

using namespace std;

enum LexicalType {
    END, ERR,
    ID,
    C_INTEGER, C_FLOAT, C_CHAR, C_STRING,
    TK_CHAR, TK_INT, TK_DOUBLE, TK_VOID,
    K_IF, K_ELSE,
    K_SWITCH, K_CASE, K_DEFAULT, K_BREAK,
    K_FOR, K_WHILE, K_DO, K_CONTINUE,
    K_RETURN,
    ADD, SUB, MUL, DIV, MOD,
    INC, DEC,
    NOT, AND, OR,
    GT, GE, LT, LE, EQ, NEQ,
    LEA, ASSIGN,
    COMMA, COLON, SEMICOLON,
    L_PARENTHESE, R_PARENTHESE, L_BRACKET, R_BRACKET, L_BRACE, R_BRACE
};

const string typeName[] = {
        "语句结束符", "错误符号",
        "标识符",
        "整形类型", "浮点类型", "字符类型", "字符串类型",
        "char", "int" ,"double","void",
        "if","else",
        "switch", "case" ,"default" ,"break",
        "for", "wihile" ,"do" ,"continue",
        "return",
        "+" ,"-", "*" ,"/", "%",
        "++","--",
        "!" ,"&&" ,"||",
        ">",">=","<","<=","==","!=",
        "&","=",
        ",", ":",";",
        "(",")","[","]","{","}"
};

#endif //SHAOLANG_COMMON_H
