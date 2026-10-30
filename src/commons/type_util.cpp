//
// Created by Ou Sheobin on 2019/5/1.
//

#include "type_util.h"
#include "symbols/symbol_table.h"

LexicalType TypeUtil::canonical_type(LexicalType type) {
    switch (type){
        case C_INTEGER: return TK_INT;
        case C_CHAR:    return TK_CHAR;
        case C_STRING:  return TK_CHAR;
        default:        return type;
    }
}

bool TypeUtil::type_check(Variable * left,Variable * right) {
    if(left==NULL||right==NULL){
        return false;
    }
    if(left->is_basic_type() && right -> is_basic_type()){
        return true;
    }else if(!left->is_basic_type() && !right -> is_basic_type()){
        return canonical_type(left->get_type()) == canonical_type(right->get_type());
    }else{
        return false;
    }
}

int TypeUtil::type_size(Variable * var) {
    if(var == NULL){
        return 0;
    }
    // 指针与数组按所指元素的宽度计算步长
    return canonical_type(var->get_type()) == TK_CHAR ? 1 : 4;
}

Variable* TypeUtil::get_offset_step(Variable *var) {
    if(var->is_basic_type()){
        return SymbolTable::oneVariable;
    }else if(var->get_type()==TK_CHAR){
        return SymbolTable::oneVariable;
    }else if(var->get_type()==TK_INT){
        return SymbolTable::fourVariable;
    }
    return NULL;
}
