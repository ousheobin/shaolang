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

Block* IntermediateInstruct::get_block() {
    return block;
}

void IntermediateInstruct::set_block(Block *block) {
    this -> block = block;
}

string IntermediateInstruct::generate_label() {
    string lb=".L";
    stringstream ss;
    ss<<lb<<(Variable::tempId ++);
    return ss.str();
}

bool IntermediateInstruct::is_jmp() {
    return (interCodeOperator == OP_JMP);
}

bool IntermediateInstruct::is_jmcond() {
    return (interCodeOperator == OP_JMP_NEQ || interCodeOperator == OP_JMP_FALSE ||
            interCodeOperator == OP_JMP_TRUE);
}

bool IntermediateInstruct::is_jmp_or_jmcond() {
    return (is_jmp() || is_jmcond());
}

void IntermediateInstruct::set_first() {
    this -> first = true;
}

bool IntermediateInstruct::is_first() {
    return first;
}

string IntermediateInstruct::display() {
    stringstream ss;
    if(label!="") {
        ss << label.c_str() << endl;
        return ss.str();
    }
    switch(interCodeOperator){
        case OP_DECLARE: ss <<"dec "<< leftArg->get_value_display()<<endl;break;
        case OP_ENTER: ss <<"entry"<<endl;break;
        case OP_EXIT: ss <<"exit"<<endl;break;
        case OP_ASSIGN: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<endl;break;
        case OP_ADD: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" + "<<rightArg->get_value_display()<<endl;break;
        case OP_SUB: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" - "<<rightArg->get_value_display()<<endl;break;
        case OP_MUL: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" * "<<rightArg->get_value_display()<<endl;break;
        case OP_DIV: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" / "<<rightArg->get_value_display()<<endl;break;
        case OP_MOD: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" %% "<<rightArg->get_value_display()<<endl;break;
        case OP_MINUS: ss <<resultVar->get_value_display()<<" = "<<"-"<<leftArg->get_value_display()<<endl;break;
        case OP_GT: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" > "<<rightArg->get_value_display()<<endl;break;
        case OP_GE: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" >= "<<rightArg->get_value_display()<<endl;break;
        case OP_LT: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" < "<<rightArg->get_value_display()<<endl;break;
        case OP_LE: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" <= "<<rightArg->get_value_display()<<endl;break;
        case OP_EQ: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" == "<<rightArg->get_value_display()<<endl;break;
        case OP_NEQ: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" != "<<rightArg->get_value_display()<<endl;break;
        case OP_NOT: ss <<resultVar->get_value_display()<<" = "<<"!"<<leftArg->get_value_display()<<endl;break;
        case OP_AND: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" && "<<rightArg->get_value_display()<<endl;break;
        case OP_OR: ss <<resultVar->get_value_display()<<" = "<<leftArg->get_value_display()<<" || "<<rightArg->get_value_display()<<endl;break;
        case OP_JMP: ss <<"goto "<<jumpTarget->label.c_str()<<endl;break;
        case OP_JMP_TRUE: ss <<"if "<<leftArg->get_value_display()<<" true jmp to "<<jumpTarget->label.c_str()<<endl;break;
        case OP_JMP_FALSE: ss <<"if "<<leftArg->get_value_display()<<" no true jmp to "<<jumpTarget->label.c_str()<<endl;break;
        case OP_JMP_NEQ: ss <<"if "<<leftArg->get_value_display()<<" != "<<rightArg->get_value_display()<<" jmp to "<<jumpTarget->label.c_str()<<endl;break;
        case OP_ARG: ss <<"arg "<<leftArg->get_value_display()<<endl;break;
        case OP_PROC: ss <<function->get_function_name().c_str()<<"("<<endl;break;
        case OP_CALL: ss <<resultVar->get_value_display()<<" = "<<function->get_function_name().c_str()<<"()"<<endl;break;
        case OP_RETURN: ss <<"goto "<<jumpTarget->label.c_str()<<endl;break;
        case OP_RETURN_WITH_VAL: ss <<"return "<<leftArg->get_value_display()<<" then goto "<<jumpTarget->label.c_str()<<endl;break;
        default: ss<<interOpHints[interCodeOperator]<<resultVar->get_value_display()<<endl;
    }
    return ss.str();
}
