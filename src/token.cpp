//
// Created by Ou Sheobin on 2018/12/28.
//

#include <token.h>
#include <sstream>

Token::Token(LexicalType t):type(t) {
    type = t;
}

string Token::to_string() {
    return "["+typeName[type]+"]" ;
}

Token::~Token() {
}

IDToken::IDToken(string id):Token(ID) {
    this -> name = id;
}

string IDToken::to_string() {
    return "["+typeName[type]+"]:"+ name;
}

IntegerToken::IntegerToken(int value):Token(C_INTEGER) {
    this -> value = value;
}

string IntegerToken::to_string() {
    std::stringstream stream;
    stream<<value;
    return "["+typeName[type]+"]:"+stream.str();
}

FloatToken::FloatToken(double value):Token(C_FLOAT){
    this -> value = value;
}

string FloatToken::to_string() {
    std::stringstream stream;
    stream<<value;
    return "["+typeName[type]+"]:"+stream.str();
}

CharToken::CharToken(char value):Token(C_CHAR) {
    this -> value = value;
}

string CharToken::to_string() {
    return "["+typeName[type]+"]:"+ value;
}

StringToken::StringToken(string value):Token(C_STRING) {
    this -> value = value;
}

string StringToken::to_string() {
    return "["+typeName[type]+"]:"+ value;
}
