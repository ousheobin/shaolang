//
// Created by Ou Sheobin on 2019/3/31.
//

#include "symbol_table.h"
#include "ir_generator.h"

Variable * SymbolTable::voidVariable = NULL;
Variable * SymbolTable::oneVariable = new Variable(1);
Variable * SymbolTable::fourVariable = new Variable(4);
Variable * SymbolTable::eightVariable = new Variable(8);

string SymbolTable::SPECIAL_VAR_NAME_STARTER = "<<";

SymbolTable::SymbolTable(){
    current_scope_id = 0;
    voidVariable = new Variable();
}

SymbolTable::~SymbolTable() {

}

void SymbolTable::enter_new_scope() {
    current_scope_id ++;
    scope_vector.push_back(current_scope_id);
    if(current_function != NULL){
        current_function -> enter_esp_scope();
    }
}

void SymbolTable::exit_current_scope() {
    scope_vector.pop_back();
    if(current_function){
        current_function -> exit_esp_scope();
    }
}

vector<int> SymbolTable::get_current_scope_path(){
    return scope_vector;
}

Variable * SymbolTable::get_void_variable() {
    return voidVariable;
}

void SymbolTable::set_ir_generator(IRGenerator *irGen) {
    this -> irGenerator = irGen;
}

void SymbolTable::add_variable(Variable * variable) {

    string name = variable->get_variable_name();
    if(name.empty()){
        return;
    }

    if(variableTable.find(name)==variableTable.end()){
        vector<Variable*> * variableVec = new vector<Variable *>();
        variableVec -> push_back(variable);
        variable_define_order_list.push_back(name);
        variableTable[variable->get_variable_name()] = variableVec;
    }else{
        vector<Variable*> & list = * variableTable[name];
        int iter_index = 0;
        int last_scope = variable -> get_scope_path().back();
        while (iter_index<list.size()){
            if(list[iter_index]->get_scope_path().back() == last_scope){
                break;
            }
            iter_index ++;
        }
        if(iter_index == list.size() || name.compare(0, SPECIAL_VAR_NAME_STARTER.size(), SPECIAL_VAR_NAME_STARTER)==0){
            list.push_back(variable);
        }else{
            SE_ERROR(semanticErrorHints[VARIABLE_DUPLICATE]);
            delete variable;
        }
    }

    if(irGenerator != NULL){
        bool not_constraint = irGenerator -> generate_variable_init(variable);
        if(current_function != NULL && not_constraint ){
            // TODO: Update status in curr function
        }
    }

}

Variable* SymbolTable::get_variable(string var_name) {

    Variable *val = NULL;
    if (variableTable.find(var_name) != variableTable.end()) {
        vector<Variable *> & valVector = * variableTable[var_name];
        int currLength = scope_vector.size();
        int maxLength = 0;
        for (int i = 0; i < valVector.size(); ++i) {
            int pathLength = valVector[i]->get_scope_path().size();
            if(pathLength <=currLength
              && scope_vector[pathLength-1] == valVector[i]->get_scope_path()[pathLength-1]
              && pathLength > maxLength){
                val = valVector[i];
                maxLength = pathLength;
            }
        }
    }
    if(val == NULL){
        SE_ERROR(semanticErrorHints[VARIABLE_NOT_DEFINE]);
    }
    return val;
}

void SymbolTable::declare_function(Function *fun) {
    string name = fun->get_function_name();
    fun -> enable_declare_flag();
    if(functionTable.find(name) == functionTable.end()){
        functionTable[name] = fun;
        function_define_order_list.push_back(name);
    }else{
        Function * existsFunction = functionTable[name];
        if(!existsFunction->equals(fun)){
            SE_ERROR(semanticErrorHints[FUNCTION_DUPLICATE]);
        }
        delete fun;
    }
}

void SymbolTable::define_function(Function *fun) {
    string name = fun->get_function_name();
    if(functionTable.find(name) == functionTable.end()){
        functionTable[name] = fun;
        function_define_order_list.push_back(name);
        current_function = fun;
    }else {
        Function * existsFunction = functionTable[name];
        if(!existsFunction->is_declare()){
            SE_ERROR(semanticErrorHints[FUNCTION_DUPLICATE]);
        }else{
            if(!existsFunction->equals(fun)){
                SE_ERROR(semanticErrorHints[FUNCTION_DUPLICATE]);
            }
            existsFunction -> disable_declare_flag();
        }
        delete fun;
        current_function = existsFunction;
    }

}

void SymbolTable::end_define_function() {
    current_function = NULL;
}

void SymbolTable::add_string(Variable *str_val) {
    constStringTable[str_val->get_variable_name()] = str_val;
}

Function* SymbolTable::get_function(string name, vector<Variable *> & args) {
    Function * target = NULL;
    if(functionTable.find(name) != functionTable.end()){
        target = functionTable[name];
        if(target -> equals(args)){
            return target;
        }else{
            SE_ERROR(semanticErrorHints[FUNCTION_PARAMETER_NOT_MATCH]);
        }
    }else{
        SE_ERROR(semanticErrorHints[FUNCTION_NOT_DEFINE]);
    }
    return NULL;
}

Function * SymbolTable::get_current_function() {
    return current_function;
}

void SymbolTable::add_inter_instruct(IntermediateInstruct *instruct) {
    if(current_function){
        current_function -> add_instruct(instruct);
    }else{
        delete instruct;
    }
}

void SymbolTable::show_all_code() {
    for(int i = 0 ; i < function_define_order_list.size() ; i ++ ){
        Function * fun = functionTable[function_define_order_list[i]];
        fun -> print_code();
    }
}

Variable* SymbolTable::get_true_var() {
    return oneVariable;
}