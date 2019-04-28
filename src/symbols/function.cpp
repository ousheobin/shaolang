//
// Created by Ou Sheobin on 2019/3/31.
//

#include <iostream>
#include <sstream>

#include "function.h"
#include "variable.h"

void Function::enable_declare_flag() {
    has_declare = true;
}

void Function::disable_declare_flag() {
    has_declare = false;
}

bool Function::is_declare() {
    return has_declare;
}

LexicalType Function::get_type() {
    return type;
}

void Function::enter_esp_scope() {
    scope_esp.push_back(0);
}

void Function::exit_esp_scope() {
    scope_esp.pop_back();
}

void Function::add_instruct(IntermediateInstruct *instruct) {
    interCodeCollection.add_code(instruct);
}

void Function::print_code() {
    std::cout << "function - "<<function_name << std::endl;
    vector<IntermediateInstruct *> codes = interCodeCollection.get_codes();
    for(int i = 0 ; i < codes.size() ; i ++ ){
        IntermediateInstruct * ins = codes[i];
        string leftName = (ins -> getLeftArg() != NULL)?ins -> getLeftArg()-> get_variable_name() : " ";
        string rightName = (ins -> getRightArg() != NULL)?ins -> getRightArg()-> get_variable_name() : " ";
        string resultName = (ins -> getResultVar() != NULL)?ins -> getResultVar()-> get_variable_name() : " ";
        cout << "("<< interOpHints[ins->getInterCodeOperator()] << "," <<  leftName << ","
        << rightName << ","<<resultName<<")"<<endl;
    }
}