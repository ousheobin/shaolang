//
// Created by Ou Sheobin on 2019/3/31.
//

#include <iostream>
#include <sstream>

#include "function.h"
#include "variable.h"
#include "commons/argument.h"
#include "commons/type_util.h"
#include "optimization/const_opt.h"
#include "dataflow/data_flow_graph.h"
#include "dataflow/block.h"

Function::Function(LexicalType lexicalType, string func_name, vector<Variable *> parameter_list) {
    type= lexicalType;
    function_name = func_name;
    parameters = parameter_list;
    has_declare = false;
    is_opt = false;
    return_point = NULL;
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
    for (unsigned long i = 0; i < size; ++i) {
        // 形参由实参初始化，兼容规则与赋值一致
        if(!TypeUtil::type_check(parameters[i],source_para[i])){
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

IntermediateInstruct* Function::get_return_point() {
    return return_point;
}

void Function::set_return_point(IntermediateInstruct *return_point) {
    this -> return_point = return_point;
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

void Function::do_optimize(SymbolTable * symbolTable) {
    if(is_opt){
        return;
    }
    DataFlowGraph * dfg = new DataFlowGraph(interCodeCollection);
    ConstPropagation * constPropagation = new ConstPropagation(dfg,symbolTable,parameters);
    constPropagation -> optimize();
    dfg -> write_opt_code(optCode);
    if(CompilerArguments::showDataFlowBlocks){
        std::cout << dfg -> to_string();
    }
    delete constPropagation;
    delete dfg;
    is_opt = true;
}

vector<IntermediateInstruct *> Function::get_emitted_code() {
    if(is_opt){
        return optCode;
    }
    return interCodeCollection.get_codes();
}

void Function::print_code() {
    std::cout << "function - "<<function_name << std::endl;
    vector<IntermediateInstruct *> codes = get_emitted_code();
    for(unsigned long i = 0 ; i < codes.size() ; i ++ ){
        cout << codes[i]->display();
    }
}
