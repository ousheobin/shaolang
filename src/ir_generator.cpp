//
// Created by Ou Sheobin on 2019/4/6.
//

#include "ir_generator.h"
#include "commons/type_util.h"

IRGenerator::IRGenerator(SymbolTable *symbolTab) {
     this -> symbolTable = symbolTab;
     tail_link.push_back(NULL);
     head_link.push_back(NULL);
}

void IRGenerator::push(IntermediateInstruct * head, IntermediateInstruct *tail) {
    head_link.push_back(head);
    tail_link.push_back(tail);
}

void IRGenerator::pop() {
    head_link.pop_back();
    tail_link.pop_back();
}

Variable * IRGenerator::generate_two_value_op(Variable *left, LexicalType op, Variable *right) {

    if(left == NULL || right == NULL ){
        return NULL;
    }else if( left->get_type() == TK_VOID || right -> get_type() == TK_VOID ){
        SE_ERROR(semanticErrorHints[EXPR_RETURN_VOID]);
        return NULL;
    }

    if( op == ASSIGN) {
        return generate_assign(left,right);
    } else if( op == AND) {
        return generate_and(left,right);
    } else if( op == OR) {
        return generate_or(left,right);
    } else if( op == GT) {
        return generate_gt(left,right);
    } else if( op == GE) {
        return generate_ge(left,right);
    } else if( op == EQ) {
        return generate_eq(left,right);
    } else if( op == NEQ) {
        return generate_neq(left,right);
    } else if( op == LE) {
        return generate_le(left,right);
    } else if( op == LT) {
        return generate_lt(left,right);
    } else if( op == ADD) {
        return generate_add(left,right);
    } else if( op == SUB) {
        return generate_sub(left,right);
    } else if( op == MUL) {
        return generate_mul(left,right);
    } else if( op == DIV) {
        return generate_div(left,right);
    } else if( op == MOD) {
        return generate_mod(left,right);
    }

    return left;
}

Variable *IRGenerator::generate_function_call(Function *fun, vector<Variable *> * args) {
    if(fun==NULL){
        return NULL;
    }
    for (int i = args->size() - 1; i >= 0 ; ++i) {
        generate_function_param(args->at(i));
    }

    if(fun->get_type() != TK_VOID){
        // 构造返回数据存储点
        Variable * returnVar = new Variable(symbolTable->get_current_scope_path(),fun->get_type(),false);
        symbolTable -> add_variable(returnVar);
        // 添加带返回数据的中间指令
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_CALL,fun,returnVar));
        return returnVar;
    }else{
        // 添加不带返回数据的返回
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_PROC,fun));
        return symbolTable -> get_void_variable();
    }
}

void IRGenerator::generate_function_param(Variable *var) {
    if(var->is_refference()){
        var = generate_assign_ptr(var);
    }
    IntermediateInstruct * inst = new IntermediateInstruct(OP_ARG,var);
    symbolTable -> add_inter_instruct(inst);
}

Variable * IRGenerator::generate_one_value_op(Variable *left, LexicalType type, Variable *right) {
    if((left != NULL && right != NULL) || (left==NULL && right == NULL)){
        return NULL;
    }
    if(left) {
        if (left->get_type() == TK_VOID) {
            SE_ERROR(semanticErrorHints[VARIABLE_CAN_NOT_BE_VOID]);
            return NULL;
        }
        if (type == INC) {
            return generate_inc_first(left);
        } else if (type == DEC) {
            return generate_dec_first(left);
        }

        if (left->is_refference()) {
            left = generate_assign_ptr(left);
        }
        if (type == NOT) {
            return generate_not(left);
        } else if (type == SUB) {
            return generate_minus(left);
        }
        return left;
    }else{
        // INC DEC
        if(right->get_type()==TK_VOID){
            SE_ERROR(semanticErrorHints[VARIABLE_CAN_NOT_BE_VOID]);
            return NULL;
        }else if(!right->is_left_var()){
            SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR]);
            return right;
        }
        if(type == INC){
            return generate_inc_last(right);
        }else if(type == DEC){
            return generate_dec_last(right);
        }
        return right;
    }
}


Variable *IRGenerator::generate_assign(Variable *left, Variable *right) {
    if(!left->is_left_var()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR]);
        return right;
    }
    if(!TypeUtil::type_check(left,right)){
        SE_ERROR(semanticErrorHints[ASSIGN_TYPE_NOT_MATCH]);
        return right;
    }
    if(right->is_refference()){
        if(!left ->is_refference()){
            // left = right -> ptr
            symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_PTR_GET,left,right->get_pointer()));
            return left;
        }else{
            // left -> ptr = right -> ptr;
            right = generate_assign_ptr(right);
        }
    }
    if(left->is_refference()){
        // left -> ptr = right
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_PTR_SET,right,left->get_pointer()));
    }else{
        // left = right
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ASSIGN,left,right));
    }
    return left;
}

Variable* IRGenerator::generate_assign_ptr(Variable * var) {
    Variable * newVar = new Variable(symbolTable->get_current_scope_path(),var);
    symbolTable -> add_variable(newVar);
    if(var -> is_refference()){
        // newVar = var -> ptr
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_PTR_GET,newVar,var->get_pointer()));
    }else{
        // newVar = var
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ASSIGN,newVar,var));
    }
    return newVar;
}

Variable *IRGenerator::generate_or(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_OR,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_and(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_AND,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_gt(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_GT,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_ge(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_GE,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_lt(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_LT,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_le(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_LE,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_eq(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_EQ,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_neq(Variable *left, Variable *right) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_NEQ,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_add(Variable *left, Variable *right) {
    Variable * tmpStore = NULL;
    if((left->is_refference() || left -> is_arr()) && right -> is_basic_type()){
        tmpStore = new Variable(symbolTable->get_current_scope_path(), left);
        right = generate_mul(right,TypeUtil::get_offset_step(left));
    }else if( left -> is_basic_type() && (right->is_refference() || right -> is_arr())){
        tmpStore = new Variable(symbolTable->get_current_scope_path(), right);
        left = generate_mul(left,TypeUtil::get_offset_step(right));
    }else if( left -> is_basic_type()&& right -> is_basic_type()){
        LexicalType type = TK_INT;
        tmpStore = new Variable(symbolTable->get_current_scope_path(), type ,false);
    }else{
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE]);
        return left;
    }
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ADD,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_sub(Variable *left, Variable *right) {
    Variable * tmpStore = NULL;
    if( left -> is_basic_type() && (right->is_refference() || right -> is_arr())){
        SE_ERROR(semanticErrorHints[SUB_LEFT_VALUE_INVAILD]);
        return left;
    }
    if( left->is_refference() || left -> is_arr() ){
        tmpStore = new Variable(symbolTable->get_current_scope_path(), left);
        right = generate_mul(right,TypeUtil::get_offset_step(left));
    }else{
        tmpStore = new Variable(symbolTable->get_current_scope_path(), TK_INT ,false);
    }
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_SUB,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_mul(Variable *left, Variable *right) {
    LexicalType tgrType = TK_INT;
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),tgrType, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_MUL,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_div(Variable *left, Variable *right) {
    LexicalType tgrType = TK_INT;
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),tgrType, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_DIV,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_mod(Variable *left, Variable *right) {
    LexicalType tgrType = TK_INT;
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),tgrType, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_MOD,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_not(Variable *var) {
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_NOT,tmpStore,var));
    return tmpStore;
}

Variable *IRGenerator::generate_minus(Variable *var) {
    if(!var->is_basic_type()){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE]);
        return var;
    }
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_MINUS,var));
    return tmpStore;
}

Variable *IRGenerator::generate_dec_first(Variable *var) {
    if(!var -> is_left_var()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR]);
        return var;
    }
    if(var->is_refference()){
        Variable * storePoint1 = generate_assign_ptr(var);
        Variable * storePoint2 = generate_sub(storePoint1,TypeUtil::get_offset_step(var));
        return generate_assign(var,storePoint2);
    }
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_SUB,var,var,TypeUtil::get_offset_step(var)));
    return var;
}

Variable *IRGenerator::generate_inc_first(Variable *var) {
    if(!var -> is_left_var()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR]);
        return var;
    }
    if(var->is_refference()){
        Variable * storePoint1 = generate_assign_ptr(var);
        Variable * storePoint2 = generate_add(storePoint1,TypeUtil::get_offset_step(var));
        return generate_assign(var,storePoint2);
    }
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ADD,var,var,TypeUtil::get_offset_step(var)));
    return var;
}

Variable *IRGenerator::generate_dec_last(Variable *var) {
    Variable * tmpStore = generate_assign_ptr(var);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_SUB,var,var,TypeUtil::get_offset_step(var)));
    return tmpStore;
}

Variable *IRGenerator::generate_inc_last(Variable *var) {
    Variable * tmpStore = generate_assign_ptr(var);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ADD,var,var,TypeUtil::get_offset_step(var)));
    return tmpStore;
}

void IRGenerator::generate_while_head(IntermediateInstruct *& while_enter, IntermediateInstruct *& while_exit) {


    while_enter=new IntermediateInstruct();
    symbolTable -> add_inter_instruct(while_enter);
    while_exit=new IntermediateInstruct();

    // Push enter point to head list and exit point to back list
    push(while_enter,while_exit);
}

void IRGenerator::generate_while_conditions(Variable * conditions, IntermediateInstruct * while_exit) {
    if(conditions){
        if( conditions -> is_void() ){
             conditions = symbolTable -> get_true_var();
        }
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_JMP_FALSE, while_exit, conditions));
    }
}

void IRGenerator::generate_while_tail(IntermediateInstruct *& while_enter, IntermediateInstruct * while_exit) {
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_JMP,while_enter));
    symbolTable -> add_inter_instruct(while_exit);
    this -> pop();
}

void IRGenerator::generate_if_head(Variable * cond, IntermediateInstruct *& if_to_else) {
    if_to_else = new IntermediateInstruct();
    if(cond){
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_JMP_FALSE,if_to_else,cond));
    }
}

void IRGenerator::generate_if_tail(IntermediateInstruct * if_to_else) {
    symbolTable -> add_inter_instruct(if_to_else);
}

void IRGenerator::generate_else_head(IntermediateInstruct * else_enter, IntermediateInstruct *& else_exit) {
    else_exit = new IntermediateInstruct();
    symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_JMP,else_exit));
    symbolTable -> add_inter_instruct( else_enter );
}

void IRGenerator::generate_else_tail(IntermediateInstruct * else_exit) {
    symbolTable -> add_inter_instruct(else_exit);
}

void IRGenerator::generate_break() {
    IntermediateInstruct * tail = tail_link.back();
    if(tail != NULL ){
        symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_JMP,tail));
    }else{
        SE_ERROR(semanticErrorHints[BREAK_POSITION_INVAILD]);
    }
}

void IRGenerator::generate_continue() {
    IntermediateInstruct * head = head_link.back();
    if( head != NULL ){
        symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_JMP,head));
    }else{
        SE_ERROR(semanticErrorHints[CONTINUE_POSITION_INVAILD]);
    }
}

void IRGenerator::generate_return(Variable * return_var) {
    if(return_var == NULL){
        return;
    }
    Function *fun = symbolTable->get_current_function();
    if((return_var->is_void() && fun->get_type()!=TK_VOID) || (return_var->is_basic_type()&&fun->get_type()==TK_VOID)){
        SE_ERROR(semanticErrorHints[RETURN_TYPE_NOT_MATCH]);
        return;
    }
    IntermediateInstruct* returnPoint=fun->get_return_point();
    if( return_var ->is_void() ){
        symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_RETURN,returnPoint));
    }else{
        symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_RETURN_WITH_VAL,returnPoint,return_var));
    }
}

bool IRGenerator::generate_variable_init(Variable *var) {

    if(var->is_literal_value()){
        return false;
    }

    symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_DECLARE,var));

    if(var->check_init()){
        generate_two_value_op(var,ASSIGN,var->get_init_var());
    }
    return true;
}

void IRGenerator::generate_function_head(Function *function) {
    function->enter_esp_scope();
    symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_ENTER,function));
    function->set_return_point(new IntermediateInstruct());
}

void IRGenerator::generate_function_tail(Function *function) {
    symbolTable -> add_inter_instruct(function->get_return_point());
    symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_EXIT,function));
    function->exit_esp_scope();
}
