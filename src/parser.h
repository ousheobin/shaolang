//
// Created by Ou Sheobin on 2019/2/28.
//

#include "lexer.h"

#ifndef SHAOLANG_PARSER_H
#define SHAOLANG_PARSER_H

class Parser
{
public:
    Parser(Lexer * lexer);
    void do_anlayse();

private:
    Lexer * lexer;
    Token * current_token;

    void move(); // 移进 Move
    bool check_and_move(LexicalType type); //匹配与规约 Reduce
    void recovery(bool in_follow_set,syntaxError symbol_lost,syntaxError symbol_error);
    void recovery(bool in_follow_set,syntaxError symbol_lost,syntaxError symbol_error,bool delete_all); //错误恢复

    // Global Syntax
    void program();
    void segment();
    void type();
    // Value Definition Syntax
    void value_definition();
    void val_or_arr_def();
    void define_list();
    void init();
    // Function Definition Syntax
    void function_extend();
    void function_definiition();
    void para_data();
    void para_data_extend();
    void para_list();
    void parameter();
    void function_content();
    // Sub-program Syntax
    void block();
    void sub_program();
    // Local Variable Syntax
    void local_var_def();
    // Control Statement Syntax
    void statement();
    void while_statement();
    void do_while_statement();
    void for_statement();
    void for_initial();
    void if_statement();
    void if_extend();
    void else_statement();
    void switch_statement();
    void case_statemnet();
    void case_label();
    // Expression Syntax
    void all_expr();
    void expr();
    void assign_expr();
    void assign_extend();
    void or_expr();
    void or_extend();
    void and_expr();
    void and_extend();
    void compare_expr();
    void compare_extend();
    void compare_symbol();
    void arithmetic_expr();
    void arithmetic_extend();
    void add_and_sub();
    void item();
    void item_extend();
    void mul_div_mod();
    void factor();
    void other_operators();
    void auto_inc_dec();
    void auto_inc_dec_op();
    void element();
    void enumable_constraint();
    void constraint();
    void id_extend();
    void init_fun_arg();
    void arg_list();
    void arg_expr();


};

#endif //SHAOLANG_PARSER_H
