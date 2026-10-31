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

    // 除赋值的目标外，参与运算的都是变量的值
    right = value_of(right);
    if(op != ASSIGN){
        left = value_of(left);
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
    for (int i = (int)args->size() - 1; i >= 0 ; --i) {
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
    IntermediateInstruct * inst = new IntermediateInstruct(OP_ARG,value_of(var));
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

        if (type == NOT) {
            return generate_not(value_of(left));
        } else if (type == SUB) {
            return generate_minus(left);
        }
        return value_of(left);
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
    if(left -> is_arr()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR] + ": 数组名不可赋值");
        return right;
    }
    if(!left->is_left_var()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR]);
        return right;
    }
    if(!TypeUtil::type_check(left,right)){
        SE_ERROR(semanticErrorHints[ASSIGN_TYPE_NOT_MATCH]);
        return right;
    }
    if(left -> is_cell()){
        // 索引或解引用定位出的单元：把值写入它所指的地址
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_PTR_SET,(Variable *) NULL,left -> get_pointer(),right));
    }else{
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ASSIGN,left,right));
    }
    return left;
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
    bool left_address = TypeUtil::is_address_value(left);
    bool right_address = TypeUtil::is_address_value(right);
    if(left_address && right_address){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": 指针之间不能相加");
        return left;
    }
    Variable * tmpStore = NULL;
    if(left_address){
        // 地址加上偏移，结果仍然指向同类元素
        tmpStore = new Variable(symbolTable->get_current_scope_path(), left);
        right = generate_mul(right,TypeUtil::get_offset_step(left));
    }else if(right_address){
        tmpStore = new Variable(symbolTable->get_current_scope_path(), right);
        left = generate_mul(left,TypeUtil::get_offset_step(right));
    }else{
        tmpStore = new Variable(symbolTable->get_current_scope_path(), TK_INT ,false);
    }
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ADD,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_sub(Variable *left, Variable *right) {
    bool left_address = TypeUtil::is_address_value(left);
    bool right_address = TypeUtil::is_address_value(right);
    Variable * tmpStore = NULL;
    if(left_address && right_address){
        // 同一数组内的两个地址相减，得到相隔的元素个数
        if(TypeUtil::canonical_type(left -> get_type()) != TypeUtil::canonical_type(right -> get_type())){
            SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": 只有指向同类元素的指针可以相减");
            return left;
        }
        Variable * distance = new Variable(symbolTable->get_current_scope_path(),TK_INT,false);
        symbolTable -> add_variable(distance);
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_SUB,distance,left,right));
        return generate_div(distance,TypeUtil::get_offset_step(left));
    }
    if( right_address ){
        SE_ERROR(semanticErrorHints[SUB_LEFT_VALUE_INVAILD]);
        return left;
    }
    if(left_address){
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
    if(TypeUtil::is_address_value(left) || TypeUtil::is_address_value(right)){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": 地址不能参与乘除运算");
        return left;
    }
    LexicalType tgrType = TK_INT;
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),tgrType, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_MUL,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_div(Variable *left, Variable *right) {
    if(TypeUtil::is_address_value(right)){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": 地址不能参与乘除运算");
        return left;
    }
    LexicalType tgrType = TK_INT;
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),tgrType, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_DIV,tmpStore,left,right));
    return tmpStore;
}

Variable *IRGenerator::generate_mod(Variable *left, Variable *right) {
    if(TypeUtil::is_address_value(left) || TypeUtil::is_address_value(right)){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": 地址不能参与乘除运算");
        return left;
    }
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
    Variable * value = value_of(var);
    if(!value->is_basic_type()){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE]);
        return var;
    }
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),TK_INT, false);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_MINUS,tmpStore,value));
    return tmpStore;
}

Variable *IRGenerator::generate_dec_first(Variable *var) {
    if(!check_modifyable(var)){
        return var;
    }
    return modify_by(var,OP_SUB);
}

Variable *IRGenerator::generate_inc_first(Variable *var) {
    if(!check_modifyable(var)){
        return var;
    }
    return modify_by(var,OP_ADD);
}

Variable *IRGenerator::generate_dec_last(Variable *var) {
    if(!check_modifyable(var)){
        return var;
    }
    // 后置自减先取原值快照，再写回递减后的结果
    Variable * before = copy_of(var);
    modify_by(var,OP_SUB);
    return before;
}

Variable *IRGenerator::generate_inc_last(Variable *var) {
    if(!check_modifyable(var)){
        return var;
    }
    Variable * before = copy_of(var);
    modify_by(var,OP_ADD);
    return before;
}

Variable *IRGenerator::modify_by(Variable *var,InterCodeOperator op) {
    Variable * step = TypeUtil::get_offset_step(var);
    if(!var -> is_cell() && var -> is_basic_type()){
        // 普通变量在原址上加减，不需要额外的临时变量
        symbolTable -> add_inter_instruct(new IntermediateInstruct(op,var,var,step));
        return var;
    }
    // 指针与索引/解引用单元要先取值，再把结果写回原位置
    Variable * current = value_of(var);
    Variable * next = op == OP_ADD ? generate_add(current,step) : generate_sub(current,step);
    return generate_assign(var,next);
}

bool IRGenerator::check_modifyable(Variable *var) {
    if(var -> is_arr()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR] + ": 数组名不可修改");
        return false;
    }
    if(!var -> is_left_var()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR]);
        return false;
    }
    return true;
}

Variable *IRGenerator::value_of(Variable *var) {
    if(var == NULL){
        return NULL;
    }
    if(var -> is_cell()){
        Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),var -> get_type(), false);
        symbolTable -> add_variable(tmpStore);
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_PTR_GET,tmpStore,var -> get_pointer()));
        return tmpStore;
    }
    if(var -> is_arr()){
        // 数组名在表达式中退化为首元素地址
        return address_of(var);
    }
    return var;
}

Variable *IRGenerator::copy_of(Variable *var) {
    if(var -> is_basic_type()){
        Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),var);
        symbolTable -> add_variable(tmpStore);
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_ASSIGN,tmpStore,var));
        return tmpStore;
    }
    return value_of(var);
}

Variable *IRGenerator::address_of(Variable *var) {
    if(var -> is_cell()){
        // &a[i] 与 &*p 就是单元本身定位出的地址
        return var -> get_pointer();
    }
    Variable * tmpStore = new Variable(symbolTable->get_current_scope_path(),var -> get_type(), true);
    symbolTable -> add_variable(tmpStore);
    symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_LEA,tmpStore,var));
    return tmpStore;
}

Variable *IRGenerator::make_cell(LexicalType element_type,Variable *address) {
    Variable * cell = new Variable(symbolTable->get_current_scope_path(),element_type, true);
    cell -> set_pointer(address);
    // 单元指向真实的存储位置，可以出现在赋值号左边
    cell -> set_is_left(true);
    symbolTable -> add_variable(cell);
    return cell;
}

Variable *IRGenerator::generate_lea(Variable *var) {
    if(var == NULL){
        return NULL;
    }
    if(var -> get_type() == TK_VOID){
        SE_ERROR(semanticErrorHints[VARIABLE_CAN_NOT_BE_VOID]);
        return var;
    }
    if(var -> is_cell()){
        return var -> get_pointer();
    }
    if(!var -> is_left_var() || var -> is_literal_value()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR] + ": '&' 只能取变量、数组元素或指针所指单元的地址");
        return var;
    }
    return address_of(var);
}

Variable *IRGenerator::generate_deref(Variable *var) {
    if(var == NULL){
        return NULL;
    }
    if(var -> get_type() == TK_VOID){
        SE_ERROR(semanticErrorHints[VARIABLE_CAN_NOT_BE_VOID]);
        return var;
    }
    if(var -> is_arr()){
        // a[0] 与 *a 等价
        return make_cell(var -> get_type(),address_of(var));
    }
    if(!var -> is_ptr()){
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": '*' 只能用于指针或数组");
        return var;
    }
    if(var -> is_cell()){
        // 语言里没有二级指针的声明方式，单元里存的是普通值，不能再次解引用
        SE_ERROR(semanticErrorHints[EXPR_NOT_BASIC_TYPE] + ": '*' 不能连续解引用");
        return var;
    }
    // 指针变量的值就是要访问的地址；&x 的结果是存放地址的临时变量，同样直接解引用
    return make_cell(var -> get_type(),var);
}

Variable *IRGenerator::generate_index(Variable *base,Variable *index) {
    if(base == NULL || index == NULL){
        return NULL;
    }
    if(base -> get_type() == TK_VOID){
        SE_ERROR(semanticErrorHints[VARIABLE_CAN_NOT_BE_VOID]);
        return base;
    }
    if(!base -> is_arr() && !base -> is_ptr()){
        SE_ERROR(semanticErrorHints[CAN_NOT_APPLY_ARRAY_INDEX] + ":" + base -> get_variable_name());
        return base;
    }
    if(base -> is_cell()){
        SE_ERROR(semanticErrorHints[CAN_NOT_APPLY_ARRAY_INDEX] + ": 只有数组和指针支持下标");
        return base;
    }
    // 数组名退化为首元素地址，指针变量的值本身就是地址
    Variable * base_address = base -> is_arr() ? address_of(base) : base;
    Variable * index_value = value_of(index);
    if(!index_value -> is_basic_type()){
        SE_ERROR(semanticErrorHints[ARRAY_INDEX_TYPE_ERROR]);
        return base;
    }
    return make_cell(base -> get_type(),generate_add(base_address,index_value));
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
        conditions = value_of(conditions);
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
        symbolTable -> add_inter_instruct(new IntermediateInstruct(OP_JMP_FALSE,if_to_else,value_of(cond)));
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
    Function *fun = symbolTable->get_current_function();
    if(fun == NULL){
        return;
    }
    IntermediateInstruct* returnPoint=fun->get_return_point();
    if(return_var == NULL || return_var->is_void() ){
        if(fun->get_type()!=TK_VOID){
            SE_ERROR(semanticErrorHints[RETURN_TYPE_NOT_MATCH]);
        }
        symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_RETURN,returnPoint));
    }else{
        if(fun->get_type()==TK_VOID){
            SE_ERROR(semanticErrorHints[RETURN_TYPE_NOT_MATCH]);
        }
        symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_RETURN_WITH_VAL,returnPoint,value_of(return_var)));
    }
}

void IRGenerator::generate_read(Variable * target) {
    if(target == NULL){
        return;
    }
    if(target->get_type() == TK_VOID){
        SE_ERROR(semanticErrorHints[VARIABLE_CAN_NOT_BE_VOID]);
        return;
    }
    if(target -> is_arr() || !target->is_left_var()){
        SE_ERROR(semanticErrorHints[INVAILD_LEFT_EXPR] + ": input 的目标必须是可写的存储位置");
        return;
    }
    symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_READ,target,NULL,NULL));
}

void IRGenerator::generate_print(Variable * value) {
    if(value == NULL || value -> get_type() == TK_VOID){
        SE_ERROR(semanticErrorHints[EXPR_RETURN_VOID]);
        return;
    }
    symbolTable -> add_inter_instruct( new IntermediateInstruct(OP_PRINT,(Variable *) NULL,value_of(value),NULL));
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
