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


Parser::Parser(Lexer * lex) {
    lexer = lex;
    current_token = NULL;
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
        type();
        value_definition();
    }else if(check_and_move(SUB)){
        if(!check_and_move(L_BRACKET)){
            recovery(ALL_TYPES,L_BRACKET_LOST,L_BRACKET_ERROR);
        }
        type();
        function_extend();
    }else{
        if(IS(ID)){
            recovery(0,TYPE_LOST,TYPE_ERROR);
            value_definition();
        }else if(IS(L_BRACKET)){
            recovery(0,TYPE_LOST,TYPE_ERROR);
            function_extend();
        }
    }
}

/**
 * <TYPE>->KW_INT | KW_FLOAT | KW_VOID | KW_CHAR
 */
void Parser::type() {

    if(ALL_TYPES){
        move();
    }else{
        recovery(IS(ID)OR(MUL),TYPE_LOST,TYPE_ERROR);
    }

}

/**
 * <VALUE_DEFINITION>-><ID> <VAL_OR_ARR_DEF>
 *                    | MUL <ID> <INIT>
 */
void Parser::value_definition() {
    if(IS(ID)){
        move();
        val_or_arr_def();
        define_list();
    }else if(check_and_move(MUL)){
        if(IS(ID)){
            move();
            init();
            define_list();
        } else{
            recovery(IS(COMMA)OR(SEMICOLON)OR(ASSIGN),ID_LOST,ID_ERROR);
        }
    }else{
        recovery(IS(L_BRACKET)OR(ASSIGN)OR(COMMA)OR(SEMICOLON),VALUE_DEFINE_ERROR,VALUE_DEFINE_ERROR);
    }
}

/**
 * <VAL_OR_ARR_DEF>-><L_BRACKET><C_INTEGER><R_BRACKET> | <INIT>
 */
void Parser::val_or_arr_def() {
    if(check_and_move(L_BRACKET)){
        if(IS(C_INTEGER)){
            move();
        }else{
            recovery(IS(R_BRACKET),L_BRACKET_LOST,L_BRACKET_ERROR);
        }
        if(!check_and_move(R_BRACKET)){
            recovery(IS(COMMA)OR(SEMICOLON),R_BRACKET_LOST,R_BRACKET_ERROR);
        }
    }else{
        init();
    }
}

/**
 * <DEFINE_LIST>-><COMMA><VALUE_DEFINITION>| SEMICOLON
 */
void Parser::define_list() {
    if(check_and_move(COMMA)){
        value_definition();
    }else if(!check_and_move(SEMICOLON)){
        if(IS(ID)OR(MUL)){
            recovery(1,COMMA_LOST,COMMA_ERROR);
            value_definition();
        }else{
            recovery(ALL_TYPES||(IS(SUB)OR(R_BRACE))||ALL_STATEMENT_KW,VALUE_DEFINE_ERROR,VALUE_DEFINE_ERROR);
        }
    }
}

/**
 * <INIT>-><ASSIGN> <EXPR> | ε
 */
void Parser::init() {
    if(check_and_move(ASSIGN)){
        expr();
    }
}

/**
 * <FUNCTION_EXTEND>-><FUNTION_DEFINITION> |  MUL<FUNTION_DEFINITION>
 */
void Parser::function_extend() {
    if(IS(MUL)){
        move();
    }
    function_definiition();
}

/**
 * <FUNTION_DEFINITION>-><R_BRACKET><ID> <L_PARENTHESE><PARAMETER><R_PARENTHESE><FUNCTION_CONTENT>
 */
void Parser::function_definiition() {
    if(!check_and_move(R_BRACKET)){
        recovery(IS(ID),R_BRACKET_LOST,R_BRACKET_ERROR);
    }
    if(!IS(ID)){
        recovery(IS(L_PARENTHESE),ID_LOST,ID_ERROR);
    }
    move();
    if(!check_and_move(L_PARENTHESE)){
        recovery(IS(R_PARENTHESE)||ALL_TYPES,L_PARENTHESE_LOST,L_PARENTHESE_LOST);
    }
    parameter();
    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE)OR(SEMICOLON),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }
    function_content();
}

/**
 * <PARA_DATA>-><MUL> <ID> | ID <PARA_DATA_EXTEND>
 */
void Parser::para_data() {
    if(IS(ID)){
        move();
        para_data_extend();
    }else if(check_and_move(MUL)){
        if(IS(ID)){
            move();
        }else{
            recovery(IS(COMMA)OR(R_PARENTHESE),ID_LOST,ID_ERROR);
        }
    }else{
        recovery(IS(COMMA)OR(R_PARENTHESE)OR(L_BRACKET),ID_LOST,ID_ERROR);
    }
}

/**
 * <PARA_DATA_EXTEND>-><L_BRACKET> <C_INTEGER> <R_BRACKET> | ε
 */
void Parser::para_data_extend() {
    if(check_and_move(L_BRACKET)){
        if(IS(C_INTEGER)){
            move();
        }
        if(!check_and_move(R_BRACKET)){
            recovery(IS(COMMA)OR(R_PARENTHESE),R_BRACKET_LOST,R_BRACKET_ERROR);
        }
    }
}

/**
 * <PARA_LIST>-><COMMA><TYPE><PARA_DATA> <PARA_LIST>| ε
 */
void Parser::para_list() {
    if(check_and_move(COMMA)){
        type();
        para_data();
        para_list();
    }else{
        if(IS(ID)){
            recovery(1,TYPE_LOST,NONE);
            move();
            para_list();
        }
    }
}

/**
 * <PARAMETER>-><TYPE><PARA_DATA> <PARA_LIST> | ε
 */
void Parser::parameter() {
    if(ALL_TYPES){
        type();
        para_data();
        para_list();
    }
}

/**
 * <FUNCTION_CONTENT>-><BLOCK> | SEMICOLON
 */
void Parser::function_content() {
    if(check_and_move(SEMICOLON)){
        return;
    }else{
        block();
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
 * <LOCAL_VAR_DEF>-><TYPE><VALUE_DEFINITION>
 */
void Parser::local_var_def() {
    if(ALL_TYPES){
        type();
        value_definition();
    }else if(IS(ID)){
        value_definition();
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
}

/**
 * <DO_WHILE_STATEMENT>-><KW_DO> <BLOCK> <KW_WHILE> <L_PARENTHESE><ALL_EXPR><R_PARENTHESE> <SEMICOLON>
 */
void Parser::do_while_statement() {
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
        block();
    }
}

/**
 * <SWITCH_STATEMENT>-><KW_SWITCH> <L_PARENTHESE> <EXPR> <R_PARENTHESE> <L_BRACE> <CASE_STATEMENT> <R_BRACE>
 */
void Parser::switch_statement() {
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
        sub_program();
        case_statemnet();
    }else if(check_and_move(K_DEFAULT)){
        if(!check_and_move(COLON)){
            recovery(ALL_TYPES||ALL_STATEMENT_KW,COLON_LOST,COLON_ERROR);
        }
        sub_program();
    }
}

/**
 * <CASE_LABEL>-><CONSTRANT>
 */
void Parser::case_label() {
    constraint();
}

/**
 * <ALL_EXPR>-><EXPR>| ε
 */
void Parser::all_expr() {
    if(ALL_EXPR_KW){
        expr();
    }
}

/**
 * <EXPR>-><ASSIGN_EXPR>
 */
void Parser::expr() {
    assign_expr();
}

/**
 * <ASSIGN_EXPR>-><OR_EXPR><ASSIGN_EXTEND>
 */
void Parser::assign_expr() {
    or_expr();
    assign_extend();
}

/**
 * <ASSIGN_EXTEND>->KW_ASSIGN <ASSIGN_EXPR> | ε
 */
void Parser::assign_extend() {
    if(check_and_move(ASSIGN)){
        assign_expr();
        assign_extend();
    }
}

/**
 * <OR_EXPR>-><AND_EXPR><OR_EXTEND>
 */
void Parser::or_expr() {
    and_expr();
    or_extend();
}

/**
 * <OR_EXTEND>-><KW_OR> <AND_EXPR><OR_EXTEND> | ε
 */
void Parser::or_extend() {
    if(check_and_move(OR)){
        and_expr();
        or_extend();
    }
}

/**
 * <AND_EXPR> -><COMPARE_EXPR><AND_EXTEND>
 */
void Parser::and_expr() {
    compare_expr();
    and_extend();
}

/**
 * <AND_EXTEND>-><KW_AND> <COMPARE_EXPR> <AND_EXTEND> | ε
 */
void Parser::and_extend() {
    if(check_and_move(AND)){
        compare_expr();
        and_extend();
    }
}

/**
 * <COMPARE_EXPR>-><ARITHMETIC_EXPR><COMPARE_EXTEND>
 */
void Parser::compare_expr() {
    arithmetic_expr();
    compare_extend();
}

/**
 * <COMPARE_EXTEND>-><COMPARE_SYMBOL><ARITHMETIC_EXPR><COMPARE_EXTEND>| ε
 */
void Parser::compare_extend() {
    if(ALL_COMPARE_SYMBOL){
        compare_symbol();
        arithmetic_expr();
        compare_extend();
    }
}

/**
 * <COMPARE_SYMBOL>->KW_GT | KW_GE | KW_LT | KW_LE | KW_EQ | KW_NEQ
 */
void Parser::compare_symbol() {
    move();
}

/**
 * <ARITHMETIC_EXPR> -><ITEM><ARITHMETIC_EXTEND>
 */
void Parser::arithmetic_expr() {
    item();
    arithmetic_extend();
}

/**
 * <ARITHMETIC_EXTEND>-><ADD_AND_SUB><ITEM><ARITHMETIC_EXTEND>| ε
 */
void Parser::arithmetic_extend() {
    if(IS(ADD)OR(SUB)){
        add_and_sub();
        item();
        arithmetic_extend();
    }
}

/**
 * <ADD_AND_SUB>-><KW_ADD> | <KW_SUB>
 */
void Parser::add_and_sub() {
    move();
}

/**
 * <ITEM>-><FACTOR><ITEM_EXTEND>
 */
void Parser::item() {
    factor();
    item_extend();
}

/**
 * <ITEM_EXTEND>-><MUL_DIV_MOD><FACTOR><ITEM_EXTEND>| ε
 */
void Parser::item_extend() {
    if(IS(MUL)OR(MOD)OR(DIV)){
        mul_div_mod();
        factor();
        item_extend();
    }
}

/**
 * <MUL_DIV_MOD>-><KW_MUL>|<KW_DIV>|<KW_MOD>
 */
void Parser::mul_div_mod() {
    move();
}

/**
 * <FACTOR>-><OTHER_OPERATORS><FACTOR>|<AUTO_INC_DEC>
 */
void Parser::factor() {
    if(IS(NOT)OR(SUB)OR(LEA)OR(MUL)OR(INC)OR(DEC)){
        other_operators();
        factor();
    }else{
        auto_inc_dec();
    }
}

/**
 * <OTHER_OPERATORS> -><KW_NOT>|<KW_SUB>|<KW_LEA>|<KW_MUL>|<KW_INCR>|<KW_DECR>
 */
void Parser::other_operators() {
    move();
}

/**
 * <AUTO_INC_DEC>	-><ELEMENT><AUTO_INC_DEC_OP>
 */
void Parser::auto_inc_dec() {
    element();
    if(IS(INC)OR(DEC)){
        auto_inc_dec_op();
    }
}

/**
 * <AUTO_INC_DEC_OP>	-><KW_INCR>|<KW_DECR>
 */
void Parser::auto_inc_dec_op() {
    move();
}

/**
 * <ELEMENT>->ID<ID_EXTEND>|<L_PARENTHESE><EXPR><R_PARENTHESE>|<CONSTRANT>
 */
void Parser::element() {
    if(IS(ID)){
        move();
        id_extend();
    }else if(check_and_move(L_PARENTHESE)){
        expr();
        if(!check_and_move(R_PARENTHESE)){
            recovery(ALL_LEFT_OPERATORS,R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
        }
    }else{
        constraint();
    }
}

/**
 * <ENUMABLE_CONSTRANT>->C_INTEGER | C_STRING | C_CHAR
 */
void Parser::enumable_constraint() {
    move();
}

/**
 * <CONSTRANT>-> <ENUMABLE_CONSTRANT>|
 */
void Parser::constraint() {
    if(IS(C_INTEGER)OR(C_CHAR)OR(C_STRING)){
        enumable_constraint();
    }else if(IS(C_FLOAT)){
        move();
    } else{
        recovery(ALL_STATEMENT_KW,CONSTRANT_DEFINE_ERROR,CONSTRANT_DEFINE_ERROR);
    }
}

/**
 * <ID_EXTEND>->L_BRACKET <EXPR> R_BRACKET|<L_PARENTHESE><INIT_FUN_ARG><R_PARENTHESE>| ε
 */
void Parser::id_extend() {
    if(check_and_move(L_BRACKET)){
        expr();
        if(!check_and_move(R_BRACKET)){
            recovery(ALL_LEFT_OPERATORS,R_BRACKET_LOST,R_BRACKET_ERROR);
        }
    }else if(check_and_move(L_PARENTHESE)){
        init_fun_arg();
        if(!check_and_move(R_PARENTHESE)){
            recovery(IS(INC)OR(DEC),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
        }
    }
}

/**
 * <INIT_FUN_ARG>-><ARG_EXPR><ARG_LIST>| ε
 */
void Parser::init_fun_arg() {
    if(ALL_EXPR_KW){
        arg_expr();
        arg_list();
    }
}

/**
 * <ARG_LIST>-><KW_COMMA><ARG_EXPR><ARG_LIST>| ε
 */
void Parser::arg_list() {
    if(check_and_move(COMMA)){
        arg_expr();
        arg_list();
    }
}

/**
 * <ARG_EXPR> -><EXPR>
 */
void Parser::arg_expr() {
    expr();
}



