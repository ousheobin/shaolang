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
    static int tempId;
    string variable_name;
    LexicalType type;
    std::vector<int> var_scope_path;

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
    double double_initial_value;
    string ptr_initial_value;
    Variable * ptr;
    int size_of_variable;
    int frame_offsize;

    void set_variable_name(const string &variable_name);
    void set_type(LexicalType type);
    void set_is_pointer(bool is_pointer);
    void set_array_length(int array_length);
    void set_scope_path(vector<int> scope_path);
    void set_init_value(Variable * val);

    void init_var_obj();

public:
    // Normal Var
    Variable(string var_name,vector<int> scope,LexicalType type,bool is_ptr,Variable * init_value);
    // Array
    Variable(string var_name, vector<int> scope,LexicalType type,int array_length);
    // Constraint
    Variable(Token * token);
    // Integer
    Variable(int val);
    // Float
    Variable(double val);
    // Temp Var
    Variable(vector<int> scope,LexicalType type,bool is_ptr);
    Variable(vector<int> scope,Variable * var);
    // Void
    Variable();

    string get_variable_name();
    LexicalType get_type();
    vector<int> get_scope_path();

    bool is_basic_type();
    bool is_refference();
    bool is_left_var();
    void set_left_var(bool is_left);
    bool is_arr();

    Variable * get_ptr();
    void set_ptr(Variable * var);
    string get_value_display();

};
#endif //SHAOLANG_VARIABLE_H
