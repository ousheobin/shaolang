//
// Created by Ou Sheobin on 2019/3/31.
//

#include "inter_code.h"
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