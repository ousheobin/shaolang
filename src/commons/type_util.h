//
// Created by Ou Sheobin on 2019/5/1.
//

#ifndef SHAOLANG_TYPE_UTIL_H
#define SHAOLANG_TYPE_UTIL_H

#include "symbols/variable.h"

class TypeUtil{
public:
    static bool type_check(Variable * left,Variable * right);
    static Variable * get_offset_step(Variable * var);
};

#endif //SHAOLANG_TYPE_UTIL_H
