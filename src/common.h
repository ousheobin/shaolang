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

enum syntaxError{
    NONE,
    TYPE_LOST,TYPE_ERROR,
    ID_LOST,ID_ERROR,
    L_PARENTHESE_LOST,L_PARENTHESE_ERROR,
    R_PARENTHESE_LOST,R_PARENTHESE_ERROR,
    L_BRACKET_LOST,L_BRACKET_ERROR,
    R_BRACKET_LOST,R_BRACKET_ERROR,
    L_BRACE_LOST,L_BRACE_ERROR,
    R_BRACE_LOST,R_BRACE_ERROR,
    COMMA_LOST,COMMA_ERROR,
    SEMICOLON_LOST,SEMICOLON_ERROR,
    COLON_LOST,COLON_ERROR,
    KW_WHILE_LOST,KW_WHILE_ERROR,
    VALUE_DEFINE_ERROR,CONSTRANT_DEFINE_ERROR
};

const string syntaxErrorHints[] ={
        "未知错误",
        "缺少类型定义","类型定义不正确",
        "缺少标识符定义","标识符定义不正确",
        "缺少'('符号","'('符号处于错误的位置",
        "缺少')'符号","')'符号处于错误的位置",
        "缺少'['符号","'['符号处于错误的位置",
        "缺少']'符号","']'符号处于错误的位置",
        "缺少'{'符号","'{'符号处于错误的位置",
        "缺少'}'符号","'}'符号处于错误的位置",
        "缺少逗号","逗号位置错误",
        "缺少分号","分号位置异常",
        "缺少':'符号","':'符号位置不正确",
        "缺少while关键字","while关键字不正确",
        "变量或函数定义不正确","常量定义不正确"
};

#endif //SHAOLANG_COMMON_H
