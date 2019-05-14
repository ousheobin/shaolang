//
// Created by Ou Sheobin on 2019/3/31.
//

#include "inter_code.h"
#include "symbols/variable.h"
#include "symbols/function.h"
#include <sstream>
#include <iostream>

IntermediateInstruct::IntermediateInstruct(InterCodeOperator op, Variable *result, Variable *left, Variable *right) {
    init();
    this -> interCodeOperator = op;
    this -> resultVar = result;
    this -> leftArg = left;
    this -> rightArg = right;
}

IntermediateInstruct::IntermediateInstruct(InterCodeOperator op, Function *fun, Variable *result) {
    init();
    this -> interCodeOperator = op;
    this -> function = fun;
    this -> resultVar = result;
}

IntermediateInstruct::IntermediateInstruct(InterCodeOperator op, Variable *variable) {
    init();
    this -> interCodeOperator = op;
    this -> leftArg = variable;
}

IntermediateInstruct::IntermediateInstruct(InterCodeOperator op, IntermediateInstruct *target, Variable *left,
                                           Variable *right) {
    init();
    this -> interCodeOperator = op;
    this -> jumpTarget = target;
    this -> leftArg = left;
    this -> rightArg = right;
}

IntermediateInstruct::IntermediateInstruct() {
    init();
    label = generate_label();
}

void IntermediateInstruct::init() {
    interCodeOperator = OP_EMPTY_INST;
    resultVar = NULL;
    leftArg = NULL;
    rightArg = NULL;
    jumpTarget = NULL;
    function = NULL;
    first = false;
}

void IntermediateInstruct::replace_inst(InterCodeOperator op, Variable *result, Variable *left, Variable *right) {
    this -> interCodeOperator = op;
    this -> resultVar = result;
    this -> leftArg = left;
    this -> rightArg = right;
}

void IntermediateInstruct::optimise_jmp(InterCodeOperator op, IntermediateInstruct *target, Variable *left,
                                        Variable *right) {
    this -> interCodeOperator = op;
    this -> jumpTarget = target;
    this -> leftArg = left;
    this -> rightArg = right;
}

void IntermediateInstruct::chg_call_to_proc() {
    this -> interCodeOperator = OP_PROC;
    this -> resultVar = NULL;
}

const string &IntermediateInstruct::getLabel() const {
    return label;
}

InterCodeOperator IntermediateInstruct::getInterCodeOperator() const {
    return interCodeOperator;
}

Variable *IntermediateInstruct::getResultVar() const {
    return resultVar;
}

IntermediateInstruct *IntermediateInstruct::getJumpTarget() const {
    return jumpTarget;
}

Variable *IntermediateInstruct::getLeftArg() const {
    return leftArg;
}

Function *IntermediateInstruct::getFunction() const {
    return function;
}

Variable *IntermediateInstruct::getRightArg() const {
    return rightArg;
}

string IntermediateInstruct::generate_label() {
    string lb=".L";
    stringstream ss;
    ss<<lb<<(Variable::tempId ++);
    return ss.str();
}

void IntermediateInstruct::display() {
    if(label!=""){
        printf("%s:\n",label.c_str());
        return;
    }
    switch(interCodeOperator)
    {
        case OP_DECLARE: cout <<"dec "<< leftArg->get_value_display()<<endl;break;
        case OP_ENTER: cout <<"entry"<<endl;break;
        case OP_EXIT: cout <<"exit"<<endl;break;
        case OP_ASSIGN: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<endl;break;
        case OP_ADD: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" + "<<rightArg->get_value_display()<<endl;break;
        case OP_SUB: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" - "<<rightArg->get_value_display()<<endl;break;
        case OP_MUL: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" * "<<rightArg->get_value_display()<<endl;break;
        case OP_DIV: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" / "<<rightArg->get_value_display()<<endl;break;
        case OP_MOD: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" %% "<<rightArg->get_value_display()<<endl;break;
        case OP_MINUS: cout <<resultVar->get_value_display()<<" = "<<"-"<<leftArg->get_value_display()<<endl;break;
        case OP_GT: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" > "<<rightArg->get_value_display()<<endl;break;
        case OP_GE: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" >= "<<rightArg->get_value_display()<<endl;break;
        case OP_LT: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" < "<<rightArg->get_value_display()<<endl;break;
        case OP_LE: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" <= "<<rightArg->get_value_display()<<endl;break;
        case OP_EQ: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" == "<<rightArg->get_value_display()<<endl;break;
        case OP_NEQ: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" != "<<rightArg->get_value_display()<<endl;break;
        case OP_NOT: cout <<resultVar->get_value_display()<<" = "<<"!"<<leftArg->get_value_display()<<endl;break;
        case OP_AND: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" && "<<rightArg->get_value_display()<<endl;break;
        case OP_OR: cout <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" || "<<rightArg->get_value_display()<<endl;break;
        case OP_JMP: cout <<"goto "<<jumpTarget->label.c_str()<<endl;break;
        case OP_JMP_TRUE: cout <<"if "<<leftArg->get_value_display()<<" true jmp to "<<jumpTarget->label.c_str()<<endl;break;
        case OP_JMP_FALSE: cout <<"if "<<leftArg->get_value_display()<<" no true jmp to "<<jumpTarget->label.c_str()<<endl;break;
        case OP_JMP_NEQ: cout <<"if "<<leftArg->get_value_display()<<" != "<<rightArg->get_value_display()<<" jmp to "<<jumpTarget->label.c_str()<<endl;break;
        case OP_ARG: cout <<"arg "<<leftArg->get_value_display()<<endl;break;
        case OP_PROC: cout <<function->get_function_name().c_str()<<"("<<endl;break;
        case OP_CALL: cout <<resultVar->get_value_display()<<" = "<<function->get_function_name().c_str()<<"()"<<endl;break;
        case OP_RETURN: cout <<"goto "<<jumpTarget->label.c_str()<<endl;break;
        case OP_RETURN_WITH_VAL: cout <<"return "<<leftArg->get_value_display()<<" then goto "<<jumpTarget->label.c_str()<<endl;break;
        case OP_LEA: cout <<resultVar->get_value_display()<<" = "<<"&"<<leftArg->get_value_display()<<endl;break;
    }
}
