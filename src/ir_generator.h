//
// Created by Ou Sheobin on 2019/4/6.
//

#ifndef SHAOLANG_IR_GENERATOR_H
#define SHAOLANG_IR_GENERATOR_H

#include "symbols/symbol_table.h"
#include "commons/common.h"

class IRGenerator{

private:
    SymbolTable * symbolTable;

    vector< IntermediateInstruct * > head_link;
    vector< IntermediateInstruct * > tail_link;
    void push(IntermediateInstruct *head, IntermediateInstruct * tail);
    void pop();//删除一个作用域

    void generate_function_param(Variable * var);

    // 表达式操作数统一转换成“值”：索引/解引用单元取出所存内容，数组名退化为首元素地址
    Variable * value_of(Variable * var);
    Variable * copy_of(Variable * var);
    Variable * address_of(Variable * var);
    Variable * make_cell(LexicalType element_type,Variable * address);

    // Two Op Instructs
    Variable * generate_assign(Variable * left,Variable * right);
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
    Variable * generate_minus(Variable * var);
    Variable * generate_dec_first(Variable * var);
    Variable * generate_inc_first(Variable * var);
    Variable * generate_dec_last(Variable * var);
    Variable * generate_inc_last(Variable * var);
    Variable * modify_by(Variable * var,InterCodeOperator op);
    bool check_modifyable(Variable * var);

public:

    IRGenerator(SymbolTable * symbolTab);

    Variable * generate_two_value_op(Variable * left,LexicalType op,Variable * right);
    Variable * generate_function_call(Function * fun,vector<Variable*> * args);
    Variable * generate_one_value_op(Variable * left,LexicalType type,Variable * right);

    // 取地址 / 解引用 / 数组下标：都定位到一个可读写的存储单元
    Variable * generate_lea(Variable * var);
    Variable * generate_deref(Variable * var);
    Variable * generate_index(Variable * base,Variable * index);

    void generate_while_head(IntermediateInstruct *& while_enter ,IntermediateInstruct *& while_exit);
    void generate_while_conditions(Variable *cond,IntermediateInstruct * while_exit);
    void generate_while_tail(IntermediateInstruct *& while_enter,IntermediateInstruct * while_exit);

    void generate_if_head(Variable * cond,IntermediateInstruct *& if_to_else);
    void generate_if_tail(IntermediateInstruct * if_to_else);
    void generate_else_head(IntermediateInstruct * else_enter,IntermediateInstruct *& else_exit);
    void generate_else_tail(IntermediateInstruct * else_exit);


    void generate_break();
    void generate_continue();
    void generate_return(Variable *ret);
    void generate_read(Variable * target);
    void generate_print(Variable * value);
    bool generate_variable_init(Variable * var);
    void generate_function_head(Function * function);
    void generate_function_tail(Function * function);

};

#endif //SHAOLANG_IR_GENERATOR_H
