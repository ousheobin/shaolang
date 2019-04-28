//
// Created by Ou Sheobin on 2019/3/31.
//

#ifndef SHAOLANG_SYMBOLS_H
#define SHAOLANG_SYMBOLS_H

#include <vector>
#include "lexer.h"
#include "commons/token.h"
#include "ir/inter_code.h"
#include "ir/inter_code_collection.h"

class Function{
protected:
    string function_name;
    LexicalType type;
    vector<Variable*> parameters;

    bool has_declare;

    int frame_max_depth;
    int current_esp_ptr;

    vector<int> scope_esp;
    string get_value_display();
    InterCodeCollection interCodeCollection;

public:
    Function(LexicalType lexicalType,string func_name,vector<Variable *>parameter_list);

    string get_function_name();
    vector<Variable*> get_parameters();
    void enable_declare_flag();
    void disable_declare_flag();
    bool is_declare();
    LexicalType get_type();

    bool equals(Function * other_fun);
    bool equals(vector<Variable*> parameters);

    void enter_esp_scope();
    void exit_esp_scope();

    void add_instruct(IntermediateInstruct * instruct);
    void print_code();

};

#endif //SHAOLANG_SYMBOLS_H