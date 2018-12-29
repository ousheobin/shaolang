//
// Created by Ou Sheobin on 2018/12/29.
//

#include "keyword.h"

KeyWord::KeyWord() {
    hashMap["float"] = TK_FLOAT;
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

LexicalType KeyWord::get_type(string name) {
    return hashMap[name];
}
