//
// Created by Ou Sheobin on 2019/5/1.
//

#include "type_util.h"
#include "symbols/symbol_table.h"

bool TypeUtil::type_check(Variable *left, Variable *right) {
    if(left==NULL||right==NULL){
        return false;
    }
    if(left->is_basic_type() && right -> is_basic_type()){
        return true;
    }else if(!left->is_basic_type() && !right -> is_basic_type()){
        return left->get_type() == right->get_type();
    }else{
        return false;
    }
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