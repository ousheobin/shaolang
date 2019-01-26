//
// Created by Ou Sheobin on 2018/12/28.
//

#ifndef SHAOLANG_TOKEN_H
#define SHAOLANG_TOKEN_H

#include "common.h"

class Token{
public:
    LexicalType type;
    Token(LexicalType t);
    virtual string to_string();
    virtual ~Token();
};

class IDToken:public Token{
public:
    string name;
    IDToken(string id);
    virtual string to_string();
};

class IntegerToken:public Token{
public:
    int value;
    IntegerToken(int value);
    virtual string to_string();
};

class FloatToken:public Token{
public:
    double value;
    FloatToken(double value);
    virtual string to_string();
};

class CharToken:public Token{
public:
    char value;
    CharToken(char value);
    virtual string to_string();
};

class StringToken:public Token{
public:
    string value;
    StringToken(string value);
    virtual string to_string();
};

#endif //SHAOLANG_TOKEN_H
