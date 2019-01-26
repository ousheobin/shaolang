//
// Created by Ou Sheobin on 2018/12/29.
//

#include "keyword.h"

KeyWord::KeyWord() {
    hashMap["double"] = TK_DOUBLE;
    hashMap["int"] = TK_INT;
    hashMap["char"] = TK_CHAR;
    hashMap["void"] = TK_VOID;
    hashMap["break"] = K_BREAK;
    hashMap["case"] = K_CASE;
    hashMap["continue"] = K_CONTINUE;
    hashMap["default"] = K_DEFAULT;
    hashMap["do"] = K_DO;
    hashMap["else"] = K_ELSE;
    hashMap["for"] = K_FOR;
    hashMap["if"] = K_IF;
    hashMap["return"] = K_RETURN;
    hashMap["switch"] = K_SWITCH;
    hashMap["while"] = K_WHILE;
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
