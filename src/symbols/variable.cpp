//
// Created by Ou Sheobin on 2019/4/28.
//

#include "variable.h"
#include "function.h"

#include "commons/type_util.h"

#include <sstream>

#ifndef SE_ERROR
#include "commons/error.h"
#define SE_ERROR(err_msg) Error::showMessage(ErrorType::Error,err_msg);
#endif

int Variable::tempId = 0;

Variable::Variable(string var_name, vector<int> scope, LexicalType type, Variable *init) {
    init_var_obj();
    set_variable_name(var_name);
    set_scope_path(scope);
    set_type(type);
    set_init_value(init);
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
    is_temp_store = true;
    is_left_value = false;
}

Variable::Variable(vector<int> scope, Variable *var) {
    stringstream name;
    name <<"<<temp_"<< (tempId ++ ) <<">>";
    init_var_obj();
    set_scope_path(scope);
    set_type(var->get_type());
    set_is_pointer(var->is_refference());
    set_variable_name(name.str());
    is_temp_store = true;
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
    is_temp_store = false;
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
    if(is_constraint && !is_refference()){
        if(type == C_INTEGER){
            ss << integer_initial_value;
        }else if(type == C_CHAR){
            ss << char_initial_value;
        }else if(type == C_STRING){
            ss << variable_name;
        }
    }else if(is_array){
        ss << typeName[type] << "_" << "ARRAY";
    }else if(is_pointer && ptr != NULL && ptr != this ){
        ss << ptr -> get_value_display();
    }else{
        ss << variable_name;
    }
    return ss.str();
}

bool Variable::is_not_init() {
    return already_init;
}

bool Variable::not_a_const() {
    return !is_literal_value();
}

int Variable::get_integer_const_val() {
    return integer_initial_value;
}

bool Variable::check_init() {
    already_init = false;
    if(!initial_value){
        return false;
    }
    if(!TypeUtil::type_check(this,initial_value)){
        SE_ERROR(semanticErrorHints[VARIABLE_INIT_ERROR]);
    }else if(initial_value->is_literal_value()){
        // Use constraint to init the var
        if(initial_value -> get_type() == C_STRING ){
            ptr_initial_value = initial_value -> get_variable_name();
        }else if(initial_value -> get_type() == C_INTEGER){
            integer_initial_value = initial_value -> integer_initial_value;
        }
        already_init = true;
    }else{
        // Use temp store point
        if(var_scope_path.size()==1){
            SE_ERROR(semanticErrorHints[GLOBAL_VARIABLE_INIT_ERROR]+":"+variable_name);
        }else{
            // Tell IR Generator => generate assign inst
            return true;
        }
    }
    return false;
}

Variable* Variable::get_init_var() {
    return initial_value;
}