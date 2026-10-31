//
// Created by Ou Sheobin on 2019/5/1.
//

#ifndef SHAOLANG_TYPE_UTIL_H
#define SHAOLANG_TYPE_UTIL_H

#include "symbols/variable.h"

class TypeUtil{
public:
    static bool type_check(Variable * left,Variable * right);
    static LexicalType canonical_type(LexicalType type);
    // 变量参与运算时的值是地址（指针变量、数组名或存放地址的临时变量）
    static bool is_address_value(Variable * var);
    static Variable * get_offset_step(Variable * var);
};

#endif //SHAOLANG_TYPE_UTIL_H
