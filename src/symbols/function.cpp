//
// Created by Ou Sheobin on 2019/3/31.
//

#include <iostream>
#include <sstream>

#include "function.h"
#include "variable.h"

Function::Function(LexicalType lexicalType, string func_name, vector<Variable *> parameter_list) {
    type= lexicalType;
    function_name = func_name;
    parameters = parameter_list;
    has_declare = false;
}

string Function::get_function_name() {
    return function_name;
}

bool Function::equals(Function *other_fun) {
    if(other_fun == nullptr){
        return false;
    }
    string otherName = other_fun -> get_function_name();
    if(otherName != function_name){
        return false;
    }
    return equals(other_fun->get_parameters());
}

bool Function::equals(vector<Variable *> source_para) {
    if(source_para.size()!=parameters.size()){
        return false;
    }
    unsigned long size = parameters.size();
    for (int i = 0; i < size; ++i) {
        if(source_para[i]->get_type() !=  parameters[i]->get_type()){
            return false;
        }
    }
    return true;
}

vector<Variable*> Function::get_parameters() {
    return parameters;
}


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
        string leftName = (ins -> getLeftArg() != NULL)?ins -> getLeftArg()-> get_value_display() : " ";
        string rightName = (ins -> getRightArg() != NULL)?ins -> getRightArg()-> get_value_display() : " ";
        string resultName = (ins -> getResultVar() != NULL)?ins -> getResultVar()-> get_value_display() : " ";
        cout << "("<< interOpHints[ins->getInterCodeOperator()] << "," <<  leftName << ","
        << rightName << ","<<resultName<<")"<<endl;
    }
}