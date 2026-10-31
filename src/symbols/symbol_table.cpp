//
// Created by Ou Sheobin on 2019/3/31.
//

#include "symbol_table.h"
#include "ir_generator.h"

Variable * SymbolTable::voidVariable = NULL;
Variable * SymbolTable::zeroVariable = new Variable(0);
Variable * SymbolTable::oneVariable = new Variable(1);
Variable * SymbolTable::fourVariable = new Variable(4);
Variable * SymbolTable::eightVariable = new Variable(8);

SymbolTable::SymbolTable(){
    current_function = NULL;
    current_scope_id = 0;
    // 文件级作用域本身就是一个作用域层级，全局变量的 scope path 为 [0]
    scope_vector.push_back(current_scope_id);
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
    // 文件级作用域不参与弹出，避免错误恢复路径上的失衡弹出
    if(scope_vector.size() > 1){
        scope_vector.pop_back();
    }
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

    // 错误恢复路径上语法分析可能返回空定义
    if(variable == NULL){
        return;
    }

    string name = variable->get_variable_name();
    if(name.empty()){
        return;
    }

    vector<Variable *> * list = NULL;
    std::unordered_map<string,vector<Variable *> *,str_hash>::iterator found = variableTable.find(name);
    if(found == variableTable.end()){
        list = new vector<Variable *>();
        variableTable[name] = list;
        variable_define_order_list.push_back(name);
    }else{
        list = found->second;
    }

    if(variable->is_constant_value()){
        // 常量字面量按出现次数各自保存，不参与重复定义检查
        list->push_back(variable);
    }else{
        int last_scope = variable -> get_scope_path().back();
        unsigned long iter_index = 0;
        while (iter_index < list->size() && (*list)[iter_index]->get_scope_path().back() != last_scope){
            iter_index ++;
        }
        if(iter_index == list->size()){
            list->push_back(variable);
        }else{
            SE_ERROR(semanticErrorHints[VARIABLE_DUPLICATE]);
            delete variable;
            return;
        }
    }

    if(variable->get_scope_path().size() == 1 && !variable->is_literal_value()){
        globalVariables.push_back(variable);
    }

    if(irGenerator != NULL){
        irGenerator -> generate_variable_init(variable);
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
            // 重复定义：报告错误，函数体照常解析但不并入已存在的函数
            SE_ERROR(semanticErrorHints[FUNCTION_DUPLICATE]);
            current_function = fun;
        }else{
            if(!existsFunction->equals(fun)){
                SE_ERROR(semanticErrorHints[FUNCTION_DUPLICATE]);
            }
            existsFunction -> disable_declare_flag();
            delete fun;
            current_function = existsFunction;
        }
    }
    irGenerator -> generate_function_head(current_function);

}

void SymbolTable::end_define_function() {
    irGenerator -> generate_function_tail(current_function);
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

vector<Function *> SymbolTable::get_defined_functions() {
    vector<Function *> functions;
    for(unsigned long i = 0 ; i < function_define_order_list.size() ; i ++ ){
        Function * fun = functionTable[function_define_order_list[i]];
        // 只有声明没有定义的函数不生成代码
        if(fun != NULL && !fun->is_declare()){
            functions.push_back(fun);
        }
    }
    return functions;
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

const vector<Variable *> & SymbolTable::get_global_variables() {
    return globalVariables;
}

Variable* SymbolTable::get_true_var() {
    return oneVariable;
}

void SymbolTable::optimize() {
    for(int i = 0 ; i < function_define_order_list.size() ; i ++ ) {
        Function *fun = functionTable[function_define_order_list[i]];
        // 只有原型没有定义的函数没有指令流，空指令序列送进控制流图会越界
        if(fun == NULL || fun -> is_declare()){
            continue;
        }
        fun -> do_optimize(this);
    }
}