//
// Created by Ou Sheobin on 2019/3/31.
//

#include <iostream>
#include <sstream>

#include "symbols.h"

int Variable::tempId = 0;

void Variable::set_variable_name(const string &variable_name) {
    this->variable_name = variable_name;
}

void Variable::set_type(LexicalType type) {
    this->type = type;
}

void Variable::set_is_pointer(bool is_pointer) {
    this->is_pointer = is_pointer;
}

void Variable::set_array_length(int array_length) {
    this->array_length = array_length;
}

void Variable::set_scope_path(vector<int> scope_path) {
    this -> var_scope_path = scope_path;
}

void Variable::set_init_value(Variable *val) {
    this -> initial_value = val;
}

void Variable::init_var_obj() {

    variable_name = "";

    var_scope_path.push_back(-1);
    is_constraint = false;
    is_pointer = false;
    is_array  = false;

    is_left_value = true;
    already_init = false;

    array_length = 0;
    initial_value = NULL;
    ptr = NULL;

    frame_offsize = 0;
}

Variable::Variable(string var_name, vector<int> scope, LexicalType type, bool is_ptr, Variable *init_value) {
    init_var_obj();
    set_variable_name(var_name);
    set_scope_path(scope);
    set_type(type);
    set_is_pointer(is_ptr);
    set_init_value(initial_value);
}

Variable::Variable(string var_name, vector<int> scope, LexicalType type, int array_length) {
    init_var_obj();
    set_variable_name(var_name);
    set_scope_path(scope);
    set_type(type);
    set_array_length(array_length);
}

Variable::Variable(Token *token) {
    init_var_obj();
    is_constraint = true;
    is_left_value = false;
    if(token->type == C_INTEGER){
        set_type(C_INTEGER);
        set_variable_name("<<int>>");
        integer_initial_value = ((IntegerToken*)token)->value;
    }else if(token->type == C_FLOAT){
        set_type(C_FLOAT);
        set_variable_name("<<double>>");
        double_initial_value = ((FloatToken*)token)->value;
    }else if(token->type == C_STRING){
        set_type(C_STRING);
        set_variable_name("<<string>>");
        str_initial_value = ((StringToken*)token)->value;
    }else if(token->type == C_CHAR){
        set_type(C_CHAR);
        set_variable_name("<<char>>");
        char_initial_value = ((CharToken*)token)->value;
    }
}

Variable::Variable(int val) {
    init_var_obj();
    set_variable_name("<<int>>");
    is_constraint = true;
    is_left_value = false;
    set_type(LexicalType::C_INTEGER);
}

Variable::Variable(double val) {
    init_var_obj();
    set_variable_name("<<double>>");
    is_constraint = true;
    is_left_value = false;
    set_type(LexicalType::C_FLOAT);
}

Variable::Variable(vector<int> scope, LexicalType type, bool is_ptr) {
    stringstream name;
    name <<"<<temp_"<< (tempId ++ ) <<">>";
    init_var_obj();
    set_variable_name(name.str());
    set_type(type);
    set_is_pointer(is_ptr);
    set_scope_path(scope);
    is_left_value = false;
}

Variable::Variable(vector<int> scope, Variable *var) {
    init_var_obj();
    set_scope_path(scope);
    set_type(var->get_type());
    set_is_pointer(var->is_refference());
    set_variable_name(var->get_variable_name()+"_temp");
    is_left_value = false;
}

Variable::Variable(){
    init_var_obj();
    set_variable_name("<<void>>");
    is_left_value = false;
    is_constraint = false;
    type = TK_VOID;
    is_pointer = true;
}

string Variable::get_variable_name() {
    return variable_name;
}

LexicalType Variable::get_type() {
    return type;
}

vector<int> Variable::get_scope_path() {
    return var_scope_path;
}


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

bool Variable::is_basic_type() {
    return !(is_pointer || is_array);
}

bool Variable::is_left_var() {
    return is_left_value;
}

void Variable::set_left_var(bool is_left) {
    is_left_value = is_left;
}

bool Variable::is_refference() {
    return is_pointer;
}

bool Variable::is_arr() {
    return is_array;
}

Variable* Variable::get_ptr() {
    return ptr;
}

void Variable::set_ptr(Variable *var) {
    this -> ptr = var;
}

string Variable::get_value_display() {
    std::stringstream ss;
    if(!is_array && !is_pointer){
        if(type == TK_INT){
            ss << integer_initial_value;
        }else if(type == TK_DOUBLE){
            ss << double_initial_value;
        }else if(type == TK_VOID){
            ss << "VOID";
        }else if(type == TK_CHAR){
            ss << char_initial_value;
        }
    }else if(is_array){
        ss << typeName[type] << "_" << "ARRAY" << endl;
    }else if(is_pointer && ptr != this){
        ss << ptr -> get_value_display();
    }
    return ss.str();
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
        string leftName = (ins -> getLeftArg() != NULL)?ins -> getLeftArg()-> get_variable_name() : " ";
        string rightName = (ins -> getRightArg() != NULL)?ins -> getRightArg()-> get_variable_name() : " ";
        string resultName = (ins -> getResultVar() != NULL)?ins -> getResultVar()-> get_variable_name() : " ";
        cout << "("<< interOpHints[ins->getInterCodeOperator()] << "," <<  leftName << ","
        << rightName << ","<<resultName<<")"<<endl;
    }
}