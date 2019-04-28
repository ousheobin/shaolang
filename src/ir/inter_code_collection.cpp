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