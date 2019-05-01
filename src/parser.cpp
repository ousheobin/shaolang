//
// Created by Ou Sheobin on 2019/2/28.
//

#include "commons/argument.h"

#include "parser.h"
#include "commons/error.h"

#include <sstream>

#define SYNTAX_ERROR(err_msg) Error::showMessage(ErrorType::Error,err_msg);

#define IS(C) (current_token->type == C)
#define OR(C) || (current_token->type == C)

#define ALL_TYPES IS(TK_INT)OR(TK_CHAR)OR(TK_VOID)
#define ALL_COMPARE_SYMBOL IS(GT)OR(GE)OR(LT)OR(LE)OR(EQ)OR(NEQ)
#define ALL_RIGHT_OPERATORS (ALL_COMPARE_SYMBOL)OR(OR)OR(AND)OR(ADD)OR(SUB)OR(MUL)OR(DIV)OR(MOD)
#define ALL_LEFT_OPERATORS (ALL_RIGHT_OPERATORS)OR(INC)OR(DEC)
#define ALL_EXPR_KW IS(L_PARENTHESE)OR(C_INTEGER)OR(C_CHAR)OR(C_STRING)OR(ID)OR(NOT)OR(SUB)OR(INC)OR(DEC)
#define ALL_STATEMENT_KW (ALL_EXPR_KW)OR(SEMICOLON)OR(K_WHILE)OR(K_IF)OR(K_CONTINUE)OR(K_RETURN)

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
 * <SEGMENT>-><TYPE><VALUE_DEFINITION> | <SUB> <L_BRACKET><TYPE><FUNTION_DEFINITION>
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
 * <TYPE>->KW_INT  | KW_VOID | KW_CHAR
 * @return Type
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
 * <VALUE_DEFINITION>-><ID> <INIT><DEFINE_LIST>
 * @param type
 * @return
 */
Variable *Parser::value_definition(LexicalType type) {
    string var_name = "";
    Variable * var = NULL;
    if(IS(ID)){
        var_name = ((IDToken *) current_token) -> name;
        move();
        var = init(type,var_name);
        define_list(type);
    }else{
        recovery(IS(L_BRACKET)OR(ASSIGN)OR(COMMA)OR(SEMICOLON),VALUE_DEFINE_ERROR,VALUE_DEFINE_ERROR);
    }
    return var;
}

/**
 * <DEFINE_LIST>-><COMMA><VALUE_DEFINITION>| SEMICOLON
 * @param type
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
 * @param type
 * @param var_name
 * @return
 */
Variable *Parser::init(LexicalType type, string var_name) {
    Variable * initVariable = NULL;
    if(check_and_move(ASSIGN)){
        initVariable = expr();
    }
    return new Variable(var_name,symbolTable->get_current_scope_path(),type,initVariable);
}

/**
 * <FUNTION_DEFINITION>-><R_BRACKET><ID><L_PARENTHESE><PARAMETER><R_PARENTHESE><FUNCTION_CONTENT>
 * @param type
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
 * <PARAMETER>-><PARA_DATA><PARA_LIST> | ε
 * @param parameters
 */
void Parser::parameter(vector<Variable *> &parameters) {
    if(ALL_TYPES){
        Variable * var = para_data();
        symbolTable -> add_variable(var);
        para_list(parameters);
        parameters.push_back(var);
    }
}

/**
 * <PARA_LIST>-><COMMA><PARA_DATA><PARA_LIST>| ε
 * @param parameters
 */
void Parser::para_list(vector<Variable *> &parameters) {
    if(check_and_move(COMMA)){
        Variable * var = para_data();
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
 * <PARA_DATA> -> <TYPE><ID>
 * @return
 */
Variable* Parser::para_data() {
    string varName = "";
    LexicalType paramType = type();
    if(!IS(ID)){
        recovery(IS(COMMA)OR(R_PARENTHESE)OR(L_BRACKET),ID_LOST,ID_ERROR);
    }
    varName = ((IDToken *)current_token) -> name;
    move();
    return new Variable(varName,symbolTable->get_current_scope_path(),paramType,NULL);
}

/**
 * <FUNCTION_CONTENT>-><BLOCK> | SEMICOLON
 * @param fun
 */
void Parser::function_content(Function *fun) {
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
 * <BLOCK>-><L_BRACE> <SUB_PROGRAM> <R_BRACE>
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
        LexicalType lexicalType = type();
        Variable * val = value_definition(lexicalType);
        symbolTable -> add_variable(val);
    }
}

/**
 * <STATEMENT>	->	<ID> <ID_ASSIGN>
 *               |   <WHILE_STATEMENT>
 *               |   <IF_STATEMENT>
 *               |   <RETURN_STATEMENT>
 *               |   <SEMICOLON>
 *               |   <KW_BREAK><SEMICOLON>
 *               |   <KW_CONTINUE> <SEMICOLON>
 */
void Parser::statement() {
    string var_name = "";
    switch (current_token->type){
        case ID:
            var_name = ((IDToken *) current_token)->name;
            move();
            id_assgin(var_name);
            break;
        case K_WHILE:
            while_statement();
            break;
        case K_IF:
            if_statement();
            break;
        case K_BREAK:
            move();
            irGenerator -> generate_break();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        case K_CONTINUE:
            move();
            irGenerator -> generate_continue();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        case K_RETURN:
            move();
            irGenerator -> generate_return(all_expr());
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        case K_OUTPUT:
            move();
            if(!check_and_move(K_OUTPUT)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),NONE,NONE);
            }
            all_expr();
            if(!check_and_move(SEMICOLON)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
            }
            break;
        case K_INPUT:
            move();
            if(!check_and_move(K_OUTPUT)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),NONE,NONE);
            }
            if(!IS(ID)){
                recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),ID_LOST,ID_ERROR);
            }
            var_name = ((IDToken *)current_token)->name;
            move();
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
    IntermediateInstruct * while_enter, * while_exit;
    irGenerator -> generate_while_head(while_enter,while_exit);
    if(!check_and_move(K_WHILE)){
        recovery(IS(L_PARENTHESE),NONE,NONE);
    }
    if(!check_and_move(L_PARENTHESE)){
        recovery(ALL_RIGHT_OPERATORS,L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }
    Variable * cond = all_expr();
    irGenerator -> generate_while_conditions(cond,while_exit);
    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }
    block();
    irGenerator -> generate_while_tail(while_enter,while_exit);
    symbolTable->exit_current_scope();
}

/**
 * <IF_STATEMENT> -> <KW_IF> <L_PARENTHESE><EXPR><R_PARENTHESE><BLOCK><ELSE_STATEMENT>
 */
void Parser::if_statement() {
    symbolTable->enter_new_scope();
    if(!check_and_move(K_IF)){
        recovery(IS(L_PARENTHESE),NONE,NONE);
    }
    IntermediateInstruct * if_to_else, * if_exit;

    if(!check_and_move(L_PARENTHESE)){
        recovery(ALL_EXPR_KW,L_PARENTHESE_LOST,L_PARENTHESE_ERROR);
    }

    Variable * conditions = expr();
    irGenerator -> generate_if_head(conditions,if_to_else);

    if(!check_and_move(R_PARENTHESE)){
        recovery(IS(L_BRACE),R_PARENTHESE_LOST,R_PARENTHESE_ERROR);
    }

    if(IS(L_BRACE)){
        block();
    }else{
        statement();
    }

    irGenerator -> generate_else_head(if_to_else,if_exit);
    symbolTable -> exit_current_scope();
    else_statement();
    irGenerator -> generate_else_tail(if_exit);
}

/**
 * <ELSE_STATEMENT> -> <KW_ELSE><BLOCK>| ε
 */
void Parser::else_statement() {
    if(check_and_move(K_ELSE)){
        symbolTable->enter_new_scope();
        if(IS(L_BRACE)){
            block();
        }else{
            statement();
        }
        symbolTable->exit_current_scope();
    }

}
/**
 * <ALL_EXPR>-><EXPR>| ε
 * @return
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
 * @return
 */
Variable * Parser::expr() {
    return assign_expr();
}

/**
 * <ASSIGN_EXPR>-><OR_EXPR><ASSIGN_EXTEND>
 * @return
 */
Variable * Parser::assign_expr() {
    Variable * leftVar = or_expr();
    return assign_extend(leftVar);
}

/**
 * <ASSIGN_EXTEND>->KW_ASSIGN <ASSIGN_EXPR> | ε
 * @return
 */
Variable * Parser::assign_extend(Variable * left_val) {
    if(check_and_move(ASSIGN)){
        Variable * rightVar = assign_expr();
        Variable * result = irGenerator -> generate_two_value_op(left_val,ASSIGN,rightVar);
        return assign_extend(result);
    }
    return left_val;
}

/**
 * <OR_EXPR>-><AND_EXPR><OR_EXTEND>
 * @return
 */
Variable * Parser::or_expr() {
    Variable * leftVar = and_expr();
    return or_extend(leftVar);
}

/**
 * <OR_EXTEND>-><KW_OR> <AND_EXPR><OR_EXTEND> | ε
 * @return
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
 * @return
 */
Variable * Parser::and_expr() {
    Variable * leftVar = compare_expr();
    return and_extend(leftVar);
}

/**
 * <AND_EXTEND>-><KW_AND> <COMPARE_EXPR> <AND_EXTEND> | ε
 * @return
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
 * @return
 */
Variable * Parser::compare_expr() {
    Variable * leftVar = arithmetic_expr();
    return compare_extend(leftVar);
}

/**
 * <COMPARE_EXTEND>-><COMPARE_SYMBOL><ARITHMETIC_EXPR><COMPARE_EXTEND>| ε
 * @return
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
 * @return
 */
LexicalType Parser::compare_symbol() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <ARITHMETIC_EXPR> -><ITEM><ARITHMETIC_EXTEND>
 * @return
 */
Variable * Parser::arithmetic_expr() {
    Variable * leftVar = item();
    return arithmetic_extend(leftVar);
}

/**
 * <ARITHMETIC_EXTEND>-><ADD_AND_SUB><ITEM><ARITHMETIC_EXTEND>| ε
 * @return
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
 * @return
 */
LexicalType Parser::add_and_sub() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <ITEM>-><FACTOR><ITEM_EXTEND>
 * @return
 */
Variable * Parser::item() {
    Variable * leftVar = factor();
    return item_extend(leftVar);
}

/**
 * <ITEM_EXTEND>-><MUL_DIV_MOD><FACTOR><ITEM_EXTEND>| ε
 * @return
 */
Variable * Parser::item_extend(Variable * left_val) {
    if(IS(MUL)OR(MOD)OR(DIV)){
        LexicalType op = mul_div_mod();
        Variable * rightVar = factor();
        Variable * result = irGenerator -> generate_two_value_op(left_val,op,rightVar);
        return item_extend(result);
    }
    return left_val;
}

/**
 * <MUL_DIV_MOD>-><KW_MUL>|<KW_DIV>|<KW_MOD>
 * @return
 */
LexicalType Parser::mul_div_mod() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <FACTOR>-><OTHER_OPERATORS><FACTOR>|<AUTO_INC_DEC>
 * @return
 */
Variable * Parser::factor() {
    if(IS(NOT)OR(SUB)OR(INC)OR(DEC)){
        LexicalType op = other_operators();
        Variable * left = factor();
        Variable * result = irGenerator -> generate_one_value_op(left,op,NULL);
        return result;
    }else{
        return auto_inc_dec();
    }
}

/**
 * <OTHER_OPERATORS> -><KW_NOT>|<KW_SUB>|<KW_INC>|<KW_DEC>
 * @return
 */
LexicalType Parser::other_operators() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <AUTO_INC_DEC>	-><ELEMENT><AUTO_INC_DEC_OP>
 * @return
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
 * <AUTO_INC_DEC_OP> -><KW_INC>|<KW_DEC>
 * @return
 */
LexicalType Parser::auto_inc_dec_op() {
    LexicalType type = current_token -> type;
    move();
    return type;
}

/**
 * <ELEMENT>->ID<ID_EXTEND>|<L_PARENTHESE><EXPR><R_PARENTHESE>|<CONSTRANT>
 * @return
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
 * <CONSTRANT>->C_INTEGER | C_STRING | C_CHAR
 * @return
 */
Variable * Parser::constraint() {
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
 * <ID_EXTEND>-><L_PARENTHESE><INIT_FUN_ARG><R_PARENTHESE>| ε
 */
Variable * Parser::id_extend(string val_name) {
    Variable * variable = NULL;
    if(check_and_move(L_PARENTHESE)){
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
 * <ID_ASSIGN> -> <ASSIGN><EXPR><SEMICOLON>
 * @return
 */
Variable* Parser::id_assgin(string var_name) {
    Variable * var = NULL;
    if(check_and_move(ASSIGN)){
        var = symbolTable->get_variable(var_name);
        if(var != NULL){
            Variable * initVariable = expr();
            var -> set_init_value(initVariable);
        }
    }
    if(!check_and_move(SEMICOLON)){
        recovery(ALL_TYPES||ALL_STATEMENT_KW||IS(R_BRACE),SEMICOLON_LOST,SEMICOLON_ERROR);
    }
    return var;
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
