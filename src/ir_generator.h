//
// Created by Ou Sheobin on 2019/4/6.
//

#ifndef SHAOLANG_IR_GENERATOR_H
#define SHAOLANG_IR_GENERATOR_H

#include "symbol_table.h"
#include "common.h"

class IRGenerator{

private:
    static int id;
    SymbolTable * symbolTable;

    void generate_function_param(Variable * var);

    // Two Op Instructs
    Variable * generate_assign(Variable * left,Variable * right);
    Variable * generate_assign_ptr(Variable * var);
    Variable * generate_or(Variable * left,Variable * right);
    Variable * generate_and(Variable * left,Variable * right);
    Variable * generate_gt(Variable * left,Variable * right);
    Variable * generate_ge(Variable * left,Variable * right);
    Variable * generate_lt(Variable * left,Variable * right);
    Variable * generate_le(Variable * left,Variable * right);
    Variable * generate_eq(Variable * left,Variable * right);
    Variable * generate_neq(Variable * left,Variable * right);
    Variable * generate_add(Variable * left,Variable * right);
    Variable * generate_sub(Variable * left,Variable * right);
    Variable * generate_mul(Variable * left,Variable * right);
    Variable * generate_div(Variable * left,Variable * right);
    Variable * generate_mod(Variable * left,Variable * right);

    Variable * generate_not(Variable * var);
    Variable * generate_lea(Variable * var);
    Variable * generate_minus(Variable * var);
    Variable * generate_ptr(Variable * var);
    Variable * generate_dec_first(Variable * var);
    Variable * generate_inc_first(Variable * var);
    Variable * generate_dec_last(Variable * var);
    Variable * generate_inc_last(Variable * var);

public:

    IRGenerator(SymbolTable * symbolTab);

    Variable * generate_two_value_op(Variable * left,LexicalType op,Variable * right);
    Variable * generate_function_call(Function * fun,vector<Variable*> * args);
    Variable * generate_array(Variable * arrayDefine,Variable * index);
    Variable * generate_one_value_op(Variable * left,LexicalType type,Variable * right);

    static bool type_check(Variable * left,Variable * right);
    static Variable * get_offset_step(Variable * var);

};

#endif //SHAOLANG_IR_GENERATOR_H
