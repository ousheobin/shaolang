//
// Created by Ou Sheobin on 2019/2/28.
//

#include "lexer.h"
#include "symbols/function.h"
#include "symbols/symbol_table.h"
#include "ir_generator.h"

#ifndef SHAOLANG_PARSER_H
#define SHAOLANG_PARSER_H

class Parser
{
public:
    Parser(Lexer * lexer,SymbolTable * symTable,IRGenerator * irGen);
    void do_anlayse();

private:
    Lexer * lexer;
    Token * current_token;
    SymbolTable * symbolTable;
    IRGenerator * irGenerator;

    void move(); // 移进 Move
    bool check_and_move(LexicalType type); //匹配与规约 Reduce
    void recovery(bool in_follow_set,syntaxError symbol_lost,syntaxError symbol_error);
    void recovery(bool in_follow_set,syntaxError symbol_lost,syntaxError symbol_error,bool delete_all); //错误恢复

    // Global Grammar
    void program();
    void segment();
    LexicalType type();

    // Variables Grammar
    Variable * value_definition(LexicalType type);
    void define_list(LexicalType type);
    Variable * init(LexicalType type,string var_name);

    // Function Grammar
    void function_definition(LexicalType type);
    void parameter(vector<Variable*> & parameters);
    void para_list(vector<Variable*> & parameters);
    Variable * para_data();
    void function_content(Function * fun);

    // Sub-program Grammar
    void block();
    void sub_program();

    // Local Variable Grammar
    void local_var_def();

    // Control Statement Grammar
    void statement();
    void while_statement();
    void if_statement();
    void else_statement();

    // Expression Grammar
    Variable * all_expr();
    Variable * expr();
    Variable * assign_expr();
    Variable * assign_extend(Variable * left_val);
    Variable * or_expr();
    Variable * or_extend(Variable * left_val);
    Variable * and_expr();
    Variable * and_extend(Variable * left_val);
    Variable * compare_expr();
    Variable * compare_extend(Variable * left_val);
    LexicalType compare_symbol();
    Variable * arithmetic_expr();
    Variable * arithmetic_extend(Variable * left_val);
    LexicalType add_and_sub();
    Variable * item();
    Variable * item_extend(Variable * left_val);
    LexicalType mul_div_mod();
    Variable * factor();
    LexicalType other_operators();
    Variable * auto_inc_dec();
    LexicalType auto_inc_dec_op();
    Variable * element();
    Variable * constraint();
    Variable * id_extend(string var_name);
    Variable * id_assgin(string var_name);

    // Function Argument Grammar
    void init_fun_arg(vector<Variable *> * args);
    void arg_list(vector<Variable *> * args);
    Variable * arg_expr();

};

#endif //SHAOLANG_PARSER_H
