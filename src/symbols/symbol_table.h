//
// Created by Ou Sheobin on 2019/3/31.
//

#ifndef SHAOLANG_SYMBOL_TABLE_H
#define SHAOLANG_SYMBOL_TABLE_H

#include <functional>
#include <stack>

#if defined(WIN32) || defined(WIN64)
#include <unordered_map>
using namespace stdext;
#else
#include <unordered_map>
#endif

#include "commons/error.h"
#include "commons/common.h"
#include "variable.h"
#include "function.h"
#include "ir/inter_code.h"

#define SE_ERROR(err_msg) Error::showMessage(ErrorType::Error,err_msg);
#define SE_WARNING(err_msg) Error::showMessage(ErrorType::Warning,err_msg);

class SymbolTable {
private:
    static string SPECIAL_VAR_NAME_STARTER;
    struct str_hash{
        size_t operator()(const string& str) const {
            std::hash<std::string> hash_struct;
            return hash_struct(str.c_str());
        }
    };

    vector<string> variable_define_order_list;
    vector<string> function_define_order_list;

    std::unordered_map<string,vector<Variable *> *,str_hash> variableTable;
    std::unordered_map<string,Function *,str_hash> functionTable;
    std::unordered_map<string,Variable *,str_hash> constStringTable;
    Function * current_function;
    int current_scope_id;
    vector<int> scope_vector;

    IRGenerator * irGenerator;

public:
    SymbolTable();
    ~SymbolTable();

    static Variable * voidVariable;
    static Variable * oneVariable;
    static Variable * fourVariable;
    static Variable * eightVariable;

    void enter_new_scope();
    void exit_current_scope();
    Variable * get_void_variable();

    void set_ir_generator(IRGenerator * irGen );

    // Variable
    void add_variable(Variable * variable);
    Variable * get_variable(string var_name);
    vector<int> get_current_scope_path();
    Variable * get_true_var();

    // Function
    void declare_function(Function *fun);
    void define_function(Function *fun);
    void end_define_function();
    Function * get_function(string name,vector<Variable*> & args);
    Function * get_current_function();

    // String
    void add_string(Variable * str_val);

    // Inter Code
    void add_inter_instruct(IntermediateInstruct * instruct);
    void show_all_code();
};


#endif //SHAOLANG_SYMBOL_TABLE_H
