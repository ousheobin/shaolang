//
// Created by Ou Sheobin on 2019/4/28.
//

#include "inter_code_collection.h"

void InterCodeCollection::add_code(IntermediateInstruct *instruct) {
    codes.push_back(instruct);
}

vector<IntermediateInstruct*> InterCodeCollection::get_codes() {
    return codes;
}

void InterCodeCollection::mark_all_first_inst() {
    unsigned long codeAmount = codes.size();
    if(codeAmount < 1){
        return;
    }
    codes[0] -> set_first();

    if(codeAmount > 1){
        codes[1] -> set_first();
    }

    if(codeAmount > 2){
        codes[codeAmount-1] -> set_first();
    }

    for(unsigned long index = 0 ; index < codeAmount ; index ++ ){
        if( codes[index]->is_jmp_or_jmcond() ){
            if( index + 1 < codeAmount){
                codes[index + 1] -> set_first();
            }
            if(codes[index]->getJumpTarget()!=NULL){
                codes[index]->getJumpTarget()-> set_first();
            }
        }
    }

}