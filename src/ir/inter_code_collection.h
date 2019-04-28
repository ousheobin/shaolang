//
// Created by Ou Sheobin on 2019/4/28.
//

#ifndef SHAOLANG_INTERCODECOLLECTION_H
#define SHAOLANG_INTERCODECOLLECTION_H

#include <vector>
#include "inter_code.h"


class InterCodeCollection{

    vector<IntermediateInstruct*> codes;

public:
    void add_code(IntermediateInstruct * instruct);
    vector<IntermediateInstruct*> get_codes();

};


#endif //SHAOLANG_INTERCODECOLLECTION_H
