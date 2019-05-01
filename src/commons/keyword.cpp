//
// Created by Ou Sheobin on 2018/12/29.
//

#include "keyword.h"

KeyWord::KeyWord() {
    hashMap["int"] = TK_INT;
    hashMap["char"] = TK_CHAR;
    hashMap["void"] = TK_VOID;
    hashMap["break"] = K_BREAK;
    hashMap["continue"] = K_CONTINUE;
    hashMap["else"] = K_ELSE;
    hashMap["if"] = K_IF;
    hashMap["while"] = K_WHILE;
    hashMap["return"] = K_RETURN;
    hashMap["input"] = K_INPUT;
    hashMap["output"] = K_OUTPUT;
}

KeyWord::~KeyWord() {
    hashMap.clear();
}

bool KeyWord::contains(string name) {
    return hashMap.find( name ) != hashMap.end();
}

LexicalType KeyWord::get_type(string name) {
    return hashMap[name];
}
