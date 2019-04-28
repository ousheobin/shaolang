//
// Created by Ou Sheobin on 2019/4/28.
//

#include "variable.h"
#include "function.h"

#include <sstream>

int Variable::tempId = 0;

Variable::Variable(string var_name, vector<int> scope, LexicalType type, bool is_ptr, Variable *init) {
    init_var_obj();
    set_variable_name(var_name);
    set_scope_path(scope);
    set_type(type);
    set_is_pointer(is_ptr);
    set_init_value(init);
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

vector<int> Variable::get_scope_path() {
    return var_scope_path;
}

LexicalType Variable::get_type() {
    return type;
}

bool Variable::is_char() {
    return (type == TK_CHAR);
}

bool Variable::is_char_pointer() {
    return (type == TK_CHAR) ;
}

bool Variable::is_ptr() {
    return is_pointer;
}

bool Variable::is_arr() {
    return is_array;
}

bool Variable::is_void() {
    return type == TK_VOID;
}

bool Variable::is_refference() {
    return (is_array || is_pointer);
}

bool Variable::is_basic_type() {
    return (!is_array && !is_pointer);
}

bool Variable::is_literal_value() {
    return is_constraint && is_basic_type();
}

string Variable::get_variable_name() {
    return variable_name;
}

string Variable::get_ptr_variable_name() {
    return ptr_initial_value;
}

// TODO: Need finish
string Variable::get_raw_string_value() {
    return str_initial_value;
}

string Variable::get_string_value() {
    return str_initial_value;
}

Variable *Variable::get_pointer() {
    return ptr;
}

void Variable::set_pointer(Variable *pointer) {
    ptr = pointer;
}

void Variable::set_is_left(bool is_left) {
    this -> is_left_value = is_left;
}

bool Variable::is_left_var() {
    return is_left_value;
}

void Variable::set_frame_offset(int offset) {
    this->frame_offsize = offset;
}

int Variable::get_frame_offset(int offset) {
    return frame_offsize;
}

int Variable::get_size() {
    return size_of_variable;
}

string Variable::get_value_display() {
    std::stringstream ss;
    if(!is_array && !is_pointer){
        if(type == TK_INT){
            ss << integer_initial_value;
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

bool Variable::is_not_init() {
    return false;
}

bool Variable::not_a_const() {
    return false;
}

int Variable::get_const_val() {
    return 0;
}
