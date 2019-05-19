//
// Created by Ou Sheobin on 2019/3/31.
//


#ifndef SHAOLANG_INTER_CODE_H
#define SHAOLANG_INTER_CODE_H

#include <vector>
#include "commons/common.h"

class IntermediateInstruct{
private:
    string label;
    InterCodeOperator interCodeOperator;

    Variable  * resultVar;
    IntermediateInstruct * jumpTarget;

    Variable * leftArg;
    Function * function;
    Variable * rightArg;

    bool first;

    Block * block;

    void init();
public:

    // Default Instruct
    IntermediateInstruct(InterCodeOperator op,Variable * result,Variable * left,Variable * right = NULL);
    // Call Function
    IntermediateInstruct(InterCodeOperator op,Function * fun, Variable * result = NULL);
    // Push Parameters to Stack
    IntermediateInstruct(InterCodeOperator op,Variable * variable);
    // JMP instruct
    IntermediateInstruct(InterCodeOperator op,IntermediateInstruct * target,Variable * left = NULL ,Variable * right = NULL);
    // EMPTY
    IntermediateInstruct();

    void replace_inst(InterCodeOperator op,Variable * result,Variable * left,Variable * right);
    void optimise_jmp(InterCodeOperator op,IntermediateInstruct * target,Variable * left,Variable * right);

    void chg_call_to_proc();

    const string &getLabel() const;

    InterCodeOperator getInterCodeOperator() const;

    Variable *getResultVar() const;

    IntermediateInstruct *getJumpTarget() const;

    Variable *getLeftArg() const;

    Function *getFunction() const;

    Variable *getRightArg() const;

    Block * get_block();
    void set_block(Block * block);

    string generate_label();

    string display();

    bool is_jmp();
    bool is_jmcond();
    bool is_jmp_or_jmcond();

    void set_first();
    bool is_first();
};

#endif //SHAOLANG_INTER_CODE_H
