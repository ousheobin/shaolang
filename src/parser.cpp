//
// Created by Ou Sheobin on 2019/2/28.
//

#include "argument.h"

#include "parser.h"
#include "error.h"

#include <sstream>

#define SYNTAX_ERROR(err_msg) Error::showMessage(ErrorType::Error,err_msg);

#define IS(C) (current_token->type == C)
#define OR(C) || (current_token->type == C)

#define ALL_TYPES IS(TK_INT)OR(TK_CHAR)OR(TK_DOUBLE)OR(TK_VOID)
#define ALL_COMPARE_SYMBOL IS(GT)OR(GE)OR(LT)OR(LE)OR(EQ)OR(NEQ)
#define ALL_RIGHT_OPERATORS (ALL_COMPARE_SYMBOL)OR(OR)OR(AND)OR(ADD)OR(SUB)OR(MUL)OR(DIV)OR(MOD)
#define ALL_LEFT_OPERATORS (ALL_RIGHT_OPERATORS)OR(INC)OR(DEC)
#define ALL_EXPR_KW IS(L_PARENTHESE)OR(C_INTEGER)OR(C_FLOAT)OR(C_CHAR)OR(C_STRING)OR(ID)OR(NOT)OR(SUB)OR(LEA)OR(MUL)OR(INC)OR(DEC)
#define ALL_STATEMENT_KW (ALL_EXPR_KW)OR(SEMICOLON)OR(K_WHILE)OR(K_FOR)OR(K_DO)OR(K_IF)OR(K_SWITCH)OR(K_BREAK)OR(K_CONTINUE)OR(K_RETURN)

Parser::Parser(Lexer * lex, SymbolTable * symTable,IRGenerator * irGen){
    symbolTable = symTable;
    lexer = lex;
    current_token = NULL;
    irGenerator = irGen;
}

void Parser::move() {
    if( lexer != NULL ){
        current_token = lexer->next_token();
        if(CompilerArguments::showLexicalResult){
            cout<<current_token->to_string()<<endl;
        }
    }
}

void Parser::do_anlayse() {
    move();
    program();
}

void Parser::recovery(bool in_follow_set, syntaxError symbol_lost, syntaxError symbol_error) {
    recovery(in_follow_set,symbol_lost,symbol_error, false);
}

void Parser::recovery(bool in_follow_set,syntaxError symbol_lost,syntaxError symbol_error,bool delete_all) {

    if(in_follow_set){
        // Missing
        SYNTAX_ERROR(syntaxErrorHints[symbol_lost]);
    }else{
        // Error
        SYNTAX_ERROR(syntaxErrorHints[symbol_error]);
        if(delete_all){
            while (!(IS(R_PARENTHESE)OR(R_BRACE)OR(SEMICOLON)OR(END))){
                move();
            }
        }else{
            move();
        }

    }

}

bool Parser::check_and_move(LexicalType type) {
    if(current_token != NULL &&type == current_token->type){
        move();
        return true;
    }
    return false;
}

/**
 * <PROGRAM>-><SEGMENT><PROGRAM>| ε
 */
void Parser::program() {
    if(check_and_move(END)){
        return;
    }else{
        segment();
        program();
    }
}


/**
 * <SEGMENT>-><TYPE><VALUE_DEFINITION>
 *          | <SUB> <L_BRACKET><TYPE><FUNCTION_EXTEND>
 */
void Parser::segment() {
    if(ALL_TYPES){
        LexicalType varType = type();
        symbolTable->add_variable(value_definition(varType));
    }else if(check_and_move(SUB)){
        if(!check_and_move(L_BRACKET)){
            recovery(ALL_TYPES,L_BRACKET_LOST,L_BRACKET_ERROR);
        }
        LexicalType varType =type();
        function_definition(varType);
    }else{
        if(IS(ID)){
            recovery(0,TYPE_LOST,TYPE_ERROR);
            value_definition(LexicalType::TK_VOID);
        }else if(IS(L_BRACKET)){
            recovery(0,TYPE_LOST,TYPE_ERROR);
            function_definition(TK_VOID);
        }
    }
}

/**
 * <TYPE>->KW_INT | KW_FLOAT | KW_VOID | KW_CHAR
 */
LexicalType Parser::type() {

    LexicalType current_type = TK_INT;

    if(ALL_TYPES){
        current_type = current_token -> type;
        move();
    }else{
        recovery(IS(ID)OR(MUL),TYPE_LOST,TYPE_ERROR);
    }

    return current_type;

}

/**
 * <VALUE_DEFINITION>-><ID> <VAL_OR_ARR_DEF> <DEFINE_LIST>
 *                    | MUL <ID> <INIT>
 */
Variable * Parser::value_definition(LexicalType type) {
    string var_name = "";
    Variable * var = NULL;
    if(IS(ID)){
        var_name = ((IDToken *) current_token) -> name;
        move();
        var = val_or_arr_def(type,false,var_name);
        define_list(type);
    }else if(check_and_move(MUL)){
        if(IS(ID)){
            var_name = ((IDToken *) current_token) -> name;
            move();
            var = init(type,true,var_name);
            define_list(type);
        } else{
            recovery(IS(COMMA)OR(SEMICOLON)OR(ASSIGN),ID_LOST,ID_ERROR);
        }
    }else{
        recovery(IS(L_BRACKET)OR(ASSIGN)OR(COMMA)OR(SEMICOLON),VALUE_DEFINE_ERROR,VALUE_DEFINE_ERROR);
    }
    return var;
}

/**
 * <VAL_OR_ARR_DEF>-><L_BRACKET><C_INTEGER><R_BRACKET> | <INIT>
 */
Variable * Parser::val_or_arr_def(LexicalType type,bool is_ptr,string var_name) {
    Variable * variable = NULL;
    if(check_and_move(L_BRACKET)){
        int arrayLength = 0;
        if(IS(C_INTEGER)){
            arrayLength = ((IntegerToken *)current_token)->value;
            move();
        }else{
            recovery(IS(R_BRACKET),L_BRACKET_LOST,L_BRACKET_ERROR);
        }
        variable = new Variable(var_name,symbolTable->get_current_scope_path(),type,arrayLength);
        if(!check_and_move(R_BRACKET)){
            recovery(IS(COMMA)OR(SEMICOLON),R_BRACKET_LOST,R_BRACKET_ERROR);
        }
    }else{
        variable = init(type,is_ptr,var_name);
    }
    return variable;
}

/**
 * <DEFINE_LIST>-><COMMA><VALUE_DEFINITION>| SEMICOLON
 */
void Parser::define_list(LexicalType type) {
    if(check_and_move(COMMA)){
        symbolTable->add_variable(value_definition(type));
    }else if(!check_and_move(SEMICOLON)){
        if(IS(ID)OR(MUL)){
            recovery(1,COMMA_LOST,COMMA_ERROR);
            symbolTable->add_variable(value_definition(type));
        }else{
            recovery(ALL_TYPES||(IS(SUB)OR(R_BRACE))||ALL_STATEMENT_KW,VALUE_DEFINE_ERROR,VALUE_DEFINE_ERROR);
        }
    }
}

/**
 * <INIT>-><ASSIGN> <EXPR> | ε
 */
Variable * Parser::init(LexicalType type,bool is_ptr,string var_name) {
    Variable * initVariable = NULL;
    if(check_and_move(ASSIGN)){
        initVariable = expr();
    }
    return new Variable(var_name,symbolTable->get_current_scope_path(),type,is_ptr,initVariable);
}

/**
 * <FUNTION_DEFINITION>-><R_BRACKET><ID> <L_PARENTHESE><PARAMETER><R_PARENTHESE><FUNCTION_CONTENT>
 */
void Parser::function_definition(LexicalType type) {
    string functionName = "";
    vector<Variable*> parameters;
    if(!check_and_move(R_BRACKET)){
        recovery(IS(ID),R_BRACKET_LOST,R_BRACKET_ERROR);
    }
    if(!IS(ID)){
        recovery(IS(L_PARENTHESE),ID_LOST,ID_ERROR);
    }else{
        functionName = ((IDToken *)current_token) -> name;
        move();
    }
    if(!check_and_move(L_PARENTHESE)){
        recovery(IS(R_PARENTHESE)||ALL_TYPES,L_PARENTHESE_LOST,L_PARENTHESE_LOST);
    }
    symbolTable->enter_new_scope();
    parameter(parameters);
    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE)OR(SEMICOLON),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }
    Function * fun = new Function(type,functionName,parameters);
    function_content(fun);
    symbolTable->exit_current_scope();
}

/**
 * <PARA_DATA>-><MUL> <ID> | ID <PARA_DATA_EXTEND>
 */
Variable * Parser::para_data(LexicalType type) {
    string varName = "";
    Variable * variable = NULL;
    if(IS(ID)){
        varName = ((IDToken *)current_token) -> name;
        move();
        variable = para_data_extend(varName,type);
    }else if(check_and_move(MUL)){
        if(IS(ID)){
            varName = ((IDToken *)current_token) -> name;
            move();
            variable = new Variable(varName,symbolTable->get_current_scope_path(),type,true,NULL);
        }else{
            recovery(IS(COMMA)OR(R_PARENTHESE),ID_LOST,ID_ERROR);
        }
    }else{
        recovery(IS(COMMA)OR(R_PARENTHESE)OR(L_BRACKET),ID_LOST,ID_ERROR);
    }
    return variable;
}

/**
 * <PARA_DATA_EXTEND>-><L_BRACKET> <C_INTEGER> <R_BRACKET> | ε
 */
Variable * Parser::para_data_extend(string var_name,LexicalType type) {
    if(check_and_move(L_BRACKET)){
        int length = 0;
        if(IS(C_INTEGER)){
            length = ((IntegerToken *)current_token)->value;
            move();
        }
        if(!check_and_move(R_BRACKET)){
            recovery(IS(COMMA)OR(R_PARENTHESE),R_BRACKET_LOST,R_BRACKET_ERROR);
        }
        return new Variable(var_name,symbolTable->get_current_scope_path(),type,length);
    }
    return new Variable(var_name,symbolTable->get_current_scope_path(),type,false,NULL);
}

/**
 * <PARA_LIST>-><COMMA><TYPE><PARA_DATA> <PARA_LIST>| ε
 */
void Parser::para_list(vector<Variable*> & parameters) {
    if(check_and_move(COMMA)){
        LexicalType t = type();
        Variable * var = para_data(t);
        symbolTable->add_variable(var);
        parameters.push_back(var);
        para_list(parameters);
    }else{
        if(IS(ID)){
            recovery(1,TYPE_LOST,NONE);
            move();
            para_list(parameters);
        }
    }
}

/**
 * <PARAMETER>-><TYPE><PARA_DATA> <PARA_LIST> | ε
 */
void Parser::parameter(vector<Variable*> & parameters) {
    if(ALL_TYPES){
        LexicalType paramType = type();
        Variable * val = para_data(paramType);
        symbolTable -> add_variable(val);
        para_list(parameters);
        parameters.push_back(val);
    }
}

/**
 * <FUNCTION_CONTENT>-><BLOCK> | SEMICOLON
 */
void Parser::function_content(Function * fun) {
    if(check_and_move(SEMICOLON)){
        symbolTable -> declare_function(fun);
        return;
    }else{
        symbolTable -> define_function(fun);
        block();
        symbolTable -> end_define_function();
    }
}

/**
 * <BLOCK>-><L_BRACE><SUB_PROGRAM> <R_BRACE>
 */
void Parser::block() {
    if(!check_and_move(L_BRACE)){
        recovery(ALL_TYPES||ALL_STATEMENT_KW,L_BRACE_LOST,L_BRACE_ERROR);
    }
    sub_program();
    if(!check_and_move(R_BRACE)){
        recovery(ALL_TYPES||IS(SUB)OR(END),R_BRACE_LOST,R_BRACE_ERROR);
    }
}

/**
 * <SUB_PROGRAM>-><LOCAL_VAR_DEF><SUB_PROGRAM>|<STATEMENT><SUB_PROGRAM>| ε
 */
void Parser::sub_program() {
    if(ALL_TYPES){
        local_var_def();
        sub_program();
    }else if(ALL_STATEMENT_KW){
        statement();
        sub_program();
    }
}

/**
 * <LOCAL_VAR_DEF>-><TYPE><VALUE_DEFINITION>|<VALUE_DEFINITION>
 */
void Parser::local_var_def() {
    if(ALL_TYPES){
        LexicalType lexicalType = type();
        Variable * val = value_definition(lexicalType);
        symbolTable -> add_variable(val);
    }else if(IS(ID)){
        string name = ((IDToken*)current_token)->name;
        Variable * var = symbolTable->get_variable(name);
        if(var != NULL){
            value_definition(var->get_type());
        }
    }

}

/**
 * <STATEMENT>	-> <ALL_EXPR><SEMICOLON>
 *                  |<WHILE_STATEMENT>|<FOR_STATEMENT>|<DO_WHILE_STATEMENT>
 *                  |<IF_STATEMENT>|<SWITCH_STATEMENT>
 *                  |<KW_BREAK><SEMICOLON>
 *                  |<KW_CONTINYE> <SEMICOLON>
 *                  |<KW_RETURN><ALL_E XPR><SEMICOLON>
 */
void Parser::statement() {
    switch (current_token->type){
        case K_WHILE:
            while_statement();
            break;
        case K_FOR:
            for_statement();
            break;
        case K_DO:
            do_while_statement();
            break;
        case K_IF:
            if_statement();
            break;
        case K_SWITCH:
            switch_statement();
            break;
        case K_BREAK:
            move();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        case K_CONTINUE:
            move();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        case K_RETURN:
            move();
            all_expr();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        default:
            all_expr();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
    }
}

/**
 * <WHILE_STATEMENT>-><KW_WHILE> <L_PARENTHESE><ALL_EXPR><R_PARENTHESE><BLOCK>
 */
void Parser::while_statement() {
    symbolTable->enter_new_scope();
    if(!check_and_move(K_WHILE)){
        recovery(IS(L_PARENTHESE),NONE,NONE);
    }
    if(!check_and_move(L_PARENTHESE)){
        recovery(ALL_RIGHT_OPERATORS,L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }
    all_expr();
    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }
    block();
    symbolTable->exit_current_scope();
}

/**
 * <DO_WHILE_STATEMENT>-><KW_DO> <BLOCK> <KW_WHILE> <L_PARENTHESE><ALL_EXPR><R_PARENTHESE> <SEMICOLON>
 */
void Parser::do_while_statement() {
    symbolTable->enter_new_scope();
    if(!check_and_move(K_DO)){
        recovery(IS(L_BRACE),NONE,NONE);
    }
    block();
    if(!check_and_move(K_WHILE)){
        recovery(IS(L_PARENTHESE),KW_WHILE_LOST,KW_WHILE_ERROR);
    }
    if(!check_and_move(L_PARENTHESE)){
        recovery(IS(ALL_EXPR_KW),L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }
    symbolTable->exit_current_scope();
    all_expr();
    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(SEMICOLON),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }
    if(!check_and_move(SEMICOLON)){
        recovery(IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
    }
}

/**
 * <FOR_STATEMENT> -><KW_FOR> <L_PARENTHESE> <FOR_INITIAL> <ALL_EXPR> <SEMICOLON> <ALL_EXPR> <R_PARENTHESE> <BLOCK>
 */
void Parser::for_statement() {
    symbolTable->enter_new_scope();
    if(!check_and_move(K_FOR)){
        recovery(IS(L_PARENTHESE),NONE,NONE);
    }
    if(!check_and_move(L_PARENTHESE)){
        recovery(ALL_TYPES||ALL_EXPR_KW||IS(SEMICOLON),L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }
    for_initial();
    all_expr();
    if(!check_and_move(SEMICOLON)){
        recovery(R_PARENTHESE,SEMICOLON_LOST,SEMICOLON_ERROR);
    }
    all_expr();
    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }
    block();
    symbolTable->exit_current_scope();
}

/**
 * <FOR_INITIAL>-><LOCAL_VAR_DEF> | <ALL_EXPR><SEMICOLON>
 */
void Parser::for_initial() {
    if(ALL_TYPES){
        local_var_def();
    }else{
        all_expr();
        if(!check_and_move(SEMICOLON)){
            recovery(ALL_EXPR_KW,SEMICOLON_LOST,SEMICOLON_ERROR);
        }
    }
}

/**
 * <IF_STATEMENT>-><KW_IF> <L_PARENTHESE><EXPR><R_PARENTHESE>
 */
void Parser::if_statement() {

    symbolTable->enter_new_scope();
    if(!check_and_move(K_IF)){
        recovery(IS(L_PARENTHESE),NONE,NONE);
    }

    if(!check_and_move(L_PARENTHESE)){
        recovery(ALL_EXPR_KW,L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }

    expr();

    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }

    if_extend();
    symbolTable -> exit_current_scope();
    else_statement();

}

/**
 * <IF_EXTEND>-><BLOCK><ELSE_STATEMENT>
 */
void Parser::if_extend() {
    if(IS(L_BRACE)){
        block();
    }else{
        statement();
    }
}

/**
 * <ELSE_STATEMENT>-><KW_ELSE><BLOCK>| ε
 */
void Parser::else_statement() {
    if(check_and_move(K_ELSE)){
        symbolTable->enter_new_scope();
        block();
        symbolTable->exit_current_scope();
    }
}

/**
 * <SWITCH_STATEMENT>-><KW_SWITCH> <L_PARENTHESE> <EXPR> <R_PARENTHESE> <L_BRACE> <CASE_STATEMENT> <R_BRACE>
 */
void Parser::switch_statement() {
    symbolTable->enter_new_scope();
    if(!check_and_move(K_SWITCH)){
        recovery(IS(L_PARENTHESE),NONE,NONE);
    }

    if(!check_and_move(L_PARENTHESE)){
        recovery(ALL_RIGHT_OPERATORS,L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }

    expr();

    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }

    if(!check_and_move(L_BRACE)){
        recovery(IS(L_BRACE),L_BRACE_LOST,L_BRACKET_ERROR);
    }

    case_statemnet();

    if(!check_and_move(R_BRACE)){
        recovery(IS(K_CASE)OR(K_DEFAULT),R_BRACE_LOST,R_BRACE_ERROR);
    }
    symbolTable->exit_current_scope();
}

/**
 * <CASE_STATEMENT> -><KW_CASE> <CASE_LABEL> <COLON> <SUB_PROGRAM><CASE_STATEMENT>
 *                   |<KW_DEFAULT> <COLON> <SUB_PROGRAM>
 */
void Parser::case_statemnet() {
    if(check_and_move(K_CASE)){
        case_label();
        if(!check_and_move(COLON)){
            recovery(ALL_TYPES||ALL_STATEMENT_KW,COLON_LOST,COLON_ERROR);
        }
        symbolTable->enter_new_scope();
        sub_program();
        symbolTable->exit_current_scope();
        case_statemnet();
    }else if(check_and_move(K_DEFAULT)){
        if(!check_and_move(COLON)){
            recovery(ALL_TYPES||ALL_STATEMENT_KW,COLON_LOST,COLON_ERROR);
        }
        symbolTable->enter_new_scope();
        sub_program();
        symbolTable->exit_current_scope();
    }
}

/**
 * <CASE_LABEL>-><CONSTRANT>
 */
Variable * Parser::case_label() {
    return constraint();
}

/**
 * <ALL_EXPR>-><EXPR>| ε
 */
Variable * Parser::all_expr() {
    if(ALL_EXPR_KW){
        return expr();
    }
    // TO-DO: VOID EXPR
    return NULL;
}

/**
 * <EXPR>-><ASSIGN_EXPR>
 */
Variable * Parser::expr() {
    return assign_expr();
}

/**
 * <ASSIGN_EXPR>-><OR_EXPR><ASSIGN_EXTEND>
 */
Variable * Parser::assign_expr() {
    Variable * leftVar = or_expr();
    return assign_extend(leftVar);
}

/**
 * <ASSIGN_EXTEND>->KW_ASSIGN <ASSIGN_EXPR> | ε
 */
Variable * Parser::assign_extend(Variable * left_val) {
    if(check_and_move(ASSIGN)){
        Variable * rightVar = assign_expr();
        Variable * result = irGenerator -> generate_two_value_op(left_val,ASSIGN,rightVar);
        assign_extend(result);
    }
    return left_val;
}

/**
 * <OR_EXPR>-><AND_EXPR><OR_EXTEND>
 */
Variable * Parser::or_expr() {
    Variable * leftVar = and_expr();
    return or_extend(leftVar);
}

/**
 * <OR_EXTEND>-><KW_OR> <AND_EXPR><OR_EXTEND> | ε
 */
Variable * Parser::or_extend(Variable * left_val) {
    if(check_and_move(OR)){
        Variable * rightVar = and_expr();
        Variable * result = irGenerator -> generate_two_value_op(left_val,OR,rightVar);
        return or_extend(result);
    }
    return left_val;
}

/**
 * <AND_EXPR> -><COMPARE_EXPR><AND_EXTEND>
 */
Variable * Parser::and_expr() {
    Variable * leftVar = compare_expr();
    return and_extend(leftVar);
}

/**
 * <AND_EXTEND>-><KW_AND> <COMPARE_EXPR> <AND_EXTEND> | ε
 */
Variable * Parser::and_extend(Variable * left_val) {
    if(check_and_move(AND)){
        Variable * rightVar = compare_expr();
        Variable * result = irGenerator -> generate_two_value_op(left_val,AND,rightVar);
        return and_extend(result);
    }
    return left_val;
}

/**
 * <COMPARE_EXPR>-><ARITHMETIC_EXPR><COMPARE_EXTEND>
 */
Variable * Parser::compare_expr() {
    Variable * leftVar = arithmetic_expr();
    return compare_extend(leftVar);
}

/**
 * <COMPARE_EXTEND>-><COMPARE_SYMBOL><ARITHMETIC_EXPR><COMPARE_EXTEND>| ε
 */
Variable * Parser::compare_extend(Variable * left_val) {
    if(ALL_COMPARE_SYMBOL){
        LexicalType symbol = compare_symbol();
        Variable * rightVar = arithmetic_expr();
        Variable * result = irGenerator -> generate_two_value_op(left_val,symbol,rightVar);
        return compare_extend(result);
    }
    return left_val;
}

/**
 * <COMPARE_SYMBOL>->KW_GT | KW_GE | KW_LT | KW_LE | KW_EQ | KW_NEQ
 */
LexicalType Parser::compare_symbol() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <ARITHMETIC_EXPR> -><ITEM><ARITHMETIC_EXTEND>
 */
Variable * Parser::arithmetic_expr() {
    Variable * leftVar = item();
    return arithmetic_extend(leftVar);
}

/**
 * <ARITHMETIC_EXTEND>-><ADD_AND_SUB><ITEM><ARITHMETIC_EXTEND>| ε
 */
Variable * Parser::arithmetic_extend(Variable * left_val) {
    if(IS(ADD)OR(SUB)){
        LexicalType op = add_and_sub();
        Variable * rightVar = item();
        Variable * result = irGenerator -> generate_two_value_op(left_val,op,rightVar);
        return arithmetic_extend(result);
    }
    return left_val;
}

/**
 * <ADD_AND_SUB>-><KW_ADD> | <KW_SUB>
 */
LexicalType Parser::add_and_sub() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <ITEM>-><FACTOR><ITEM_EXTEND>
 */
Variable * Parser::item() {
    Variable * leftVar = factor();
    return item_extend(leftVar);
}

/**
 * <ITEM_EXTEND>-><MUL_DIV_MOD><FACTOR><ITEM_EXTEND>| ε
 */
Variable * Parser::item_extend(Variable * left_val) {
    if(IS(MUL)OR(MOD)OR(DIV)){
        LexicalType op = mul_div_mod();
        Variable * rightVar = factor();
        Variable * result = irGenerator -> generate_two_value_op(left_val,op,rightVar);
        item_extend(result);
    }
    return left_val;
}

/**
 * <MUL_DIV_MOD>-><KW_MUL>|<KW_DIV>|<KW_MOD>
 */
LexicalType Parser::mul_div_mod() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <FACTOR>-><OTHER_OPERATORS><FACTOR>|<AUTO_INC_DEC>
 */
Variable * Parser::factor() {
    if(IS(NOT)OR(SUB)OR(LEA)OR(MUL)OR(INC)OR(DEC)){
        LexicalType op = other_operators();
        Variable * left = factor();
        Variable * result = irGenerator -> generate_one_value_op(left,op,NULL);
        return result;
    }else{
        return auto_inc_dec();
    }
}

/**
 * <OTHER_OPERATORS> -><KW_NOT>|<KW_SUB>|<KW_LEA>|<KW_MUL>|<KW_INCR>|<KW_DECR>
 */
LexicalType Parser::other_operators() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <AUTO_INC_DEC>	-><ELEMENT><AUTO_INC_DEC_OP>
 */
Variable * Parser::auto_inc_dec() {
    Variable * right = element();
    if(IS(INC)OR(DEC)){
        LexicalType op = auto_inc_dec_op();
        right = irGenerator -> generate_one_value_op(NULL,op,right);
    }
    return right;
}

/**
 * <AUTO_INC_DEC_OP>	-><KW_INCR>|<KW_DECR>
 */
LexicalType Parser::auto_inc_dec_op() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <ELEMENT>->ID<ID_EXTEND>|<L_PARENTHESE><EXPR><R_PARENTHESE>|<CONSTRANT>
 */
Variable * Parser::element() {
    if(IS(ID)){
        string elementName = ((IDToken *)current_token)->name;
        move();
        return id_extend(elementName);
    }else if(check_and_move(L_PARENTHESE)){
        Variable * var = expr();
        if(!check_and_move(R_PARENTHESE)){
            recovery(ALL_LEFT_OPERATORS,R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
        }
        return var;
    }else{
        return constraint();
    }
}

/**
 * <ENUMABLE_CONSTRANT>->C_INTEGER | C_STRING | C_CHAR
 */
Variable * Parser::enumable_constraint() {
    if(IS(C_INTEGER)OR(C_STRING)OR(C_CHAR)) {
        Variable * variable = new Variable(current_token);
        if(IS(C_STRING)){
            symbolTable -> add_string(variable);
        }else{
            symbolTable -> add_variable(variable);
        }
        move();
        return variable;
    }
    return NULL;
}

/**
 * <CONSTRANT>-> <ENUMABLE_CONSTRANT>|
 */
Variable * Parser::constraint() {
    Variable * variable = NULL;
    if(IS(C_INTEGER)OR(C_CHAR)OR(C_STRING)){
        variable = enumable_constraint();
    }else if(IS(C_FLOAT)){
        Variable * variable = new Variable(current_token);
        symbolTable -> add_variable(variable);
        move();
    } else{
        recovery(ALL_STATEMENT_KW,CONSTRANT_DEFINE_ERROR,CONSTRANT_DEFINE_ERROR);
    }
    return variable;
}

/**
 * <ID_EXTEND>->L_BRACKET <EXPR> R_BRACKET|<L_PARENTHESE><INIT_FUN_ARG><R_PARENTHESE>| ε
 */
Variable * Parser::id_extend(string val_name) {
    Variable * variable = NULL;
    if(check_and_move(L_BRACKET)){
        Variable * indexVar = expr();
        if(!check_and_move(R_BRACKET)){
            recovery(ALL_LEFT_OPERATORS,R_BRACKET_LOST,R_BRACKET_ERROR);
        }
        Variable * arrayDefine = symbolTable -> get_variable(val_name);
        variable = irGenerator -> generate_array(arrayDefine,indexVar);
    }else if(check_and_move(L_PARENTHESE)){
        vector<Variable *> * args = new vector<Variable *>();
        init_fun_arg(args);
        if(!check_and_move(R_PARENTHESE)){
            recovery(IS(INC)OR(DEC),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
        }
        Function * function = symbolTable -> get_function(val_name,* args);
        irGenerator -> generate_function_call(function,args);
    }else{
        variable = symbolTable -> get_variable(val_name);
    }
    return variable;
}

/**
 * <INIT_FUN_ARG>-><ARG_EXPR><ARG_LIST>| ε
 */
void Parser::init_fun_arg(vector<Variable*> * args) {
    if(ALL_EXPR_KW){
        args -> push_back(arg_expr());
        arg_list(args);
    }
}

/**
 * <ARG_LIST>-><KW_COMMA><ARG_EXPR><ARG_LIST>| ε
 */
void Parser::arg_list(vector<Variable*> * args) {
    if(check_and_move(COMMA)){
        args -> push_back(arg_expr());
        arg_list(args);
    }
}

/**
 * <ARG_EXPR> -><EXPR>
 */
Variable * Parser::arg_expr() {
    return expr();
}

