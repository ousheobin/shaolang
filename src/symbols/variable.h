//
// Created by Ou Sheobin on 2019/4/28.
//

#ifndef SHAOLANG_VARIABLE_H
#define SHAOLANG_VARIABLE_H

#include <vector>
#include "lexer.h"
#include "commons/token.h"
#include "ir/inter_code.h"

class Variable{
private:

    string variable_name;
    LexicalType type;
    std::vector<int> var_scope_path;

    bool is_temp_store;

    bool is_constraint;
    bool is_pointer;
    bool is_array;

    bool is_left_value;
    bool already_init;

    // Initial Value Definition
    int array_length;
    Variable * initial_value;
    int integer_initial_value;
    char char_initial_value;
    string str_initial_value;
    string ptr_initial_value;
    Variable * ptr;

    int size_of_variable;
    int frame_offsize;

    void set_variable_name(const string &variable_name);
    void set_type(LexicalType type);
    void set_is_pointer(bool is_pointer);
    void set_array_length(int array_length);
    void set_scope_path(vector<int> scope_path);

    void init_var_obj();

public:

    static int tempId;

    // DFG Opt Fields
    int opt_index;
    bool is_live;

    // Reg assignment
    int register_id;
    bool in_main_memory;

    // Normal Var
    Variable(string var_name, vector<int> scope,LexicalType type,Variable * init_value);
    // Constraint
    Variable(Token * token);
    // Integer
    Variable(int val);
    // Temp Var
    Variable(vector<int> scope,LexicalType type,bool is_ptr);
    Variable(vector<int> scope,Variable * var);
    // Void
    Variable();

    // Scope Path
    vector<int> get_scope_path();

    // Type
    LexicalType get_type();
    bool is_char();
    bool is_char_pointer();
    bool is_ptr();
    bool is_arr();
    bool is_void();
    bool is_refference();
    bool is_basic_type();
    bool is_literal_value();

    // Common
    string get_variable_name();
    string get_ptr_variable_name();
    string get_raw_string_value();
    string get_string_value();

    // Pointer
    Variable * get_pointer();
    void set_pointer(Variable * ptr);

    void set_is_left(bool is_left);
    bool is_left_var();
    void set_init_value(Variable * val);
    void set_frame_offset(int offset);
    int get_frame_offset(int offset);
    int get_size();

    // To String
    string get_value_display();

    // IR Generate
    bool check_init();
    Variable * get_init_var();

    // DFG
    bool is_not_init();
    bool not_a_const();
    int get_integer_const_val();

};
#endif //SHAOLANG_VARIABLE_H
