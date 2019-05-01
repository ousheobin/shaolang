//
// Created by Ou Sheobin on 2018/12/28.
//

#ifndef SHAOLANG_COMMON_H
#define SHAOLANG_COMMON_H

#include <string>

using namespace std;

enum LexicalType {
    END, ERR,
    ID,
    C_INTEGER, C_CHAR, C_STRING,
    TK_CHAR, TK_INT, TK_VOID,
    K_IF, K_ELSE, K_WHILE,
    K_BREAK, K_CONTINUE, K_RETURN,
    K_INPUT,K_OUTPUT,
    ADD, SUB, MUL, DIV, MOD,
    INC, DEC,
    NOT, AND, OR,
    GT, GE, LT, LE, EQ, NEQ,
    LEA, ASSIGN,
    COMMA, COLON, SEMICOLON,
    LEFT_STREAM,RIGHT_STREAM,
    L_PARENTHESE, R_PARENTHESE, L_BRACKET, R_BRACKET, L_BRACE, R_BRACE
};

const string typeName[] = {
        "语句结束符", "错误符号",
        "标识符",
        "整形类型", "字符类型", "字符串类型",
        "char", "int" ,"void",
        "if","else", "while",
        "break", "continue", "return",
        "input","output",
        "+" ,"-", "*" ,"/", "%",
        "++","--",
        "!" ,"&&" ,"||",
        ">",">=","<","<=","==","!=",
        "&","=",
        ",", ":",";",
        "<<",">>",
        "(",")","[","]","{","}"
};

enum syntaxError{
    NONE,
    TYPE_LOST,TYPE_ERROR,
    ID_LOST,ID_ERROR,
    L_PARENTHESE_LOST,L_PARENTHESE_ERROR,
    R_PARENTHESE_LOST,R_PARENTHESE_ERROR,
    L_BRACKET_LOST,L_BRACKET_ERROR,
    R_BRACKET_LOST,R_BRACKET_ERROR,
    L_BRACE_LOST,L_BRACE_ERROR,
    R_BRACE_LOST,R_BRACE_ERROR,
    COMMA_LOST,COMMA_ERROR,
    SEMICOLON_LOST,SEMICOLON_ERROR,
    COLON_LOST,COLON_ERROR,
    KW_WHILE_LOST,KW_WHILE_ERROR,
    VALUE_DEFINE_ERROR,CONSTRANT_DEFINE_ERROR
};

const string syntaxErrorHints[] ={
        "未知错误",
        "缺少类型定义","类型定义处于不正确位置，或无法识别",
        "缺少标识符定义","标识符定义处于不正确位置，或无法识别",
        "缺少'('符号","'('符号处于不正确位置，或无法识别",
        "缺少')'符号","')'符号处于不正确位置，或无法识别",
        "缺少'['符号","'['符号处于不正确位置，或无法识别",
        "缺少']'符号","']'符号处于不正确位置，或无法识别",
        "缺少'{'符号","'{'符号处于不正确位置，或无法识别",
        "缺少'}'符号","'}'符号处于不正确位置，或无法识别",
        "缺少逗号","逗号处于不正确位置，或无法识别",
        "缺少分号","分号处于不正确位置，或无法识别",
        "缺少':'符号","':'符号处于不正确位置，或无法识别",
        "缺少while关键字","while关键字处于不正确位置，或无法识别",
        "变量或函数定义不正确","常量定义处于不正确位置，或无法识别"
};

enum semanticError{
    VARIABLE_DUPLICATE,
    VARIABLE_NOT_DEFINE,
    VARIABLE_INIT_ERROR,
    VARIABLE_CAN_NOT_BE_VOID,
    GLOBAL_VARIABLE_INIT_ERROR,
    FUNCTION_DUPLICATE,
    FUNCTION_NOT_DEFINE,
    FUNCTION_PARAMETER_NOT_MATCH,
    CAN_NOT_INIT_WHILE_DECLARE,
    ARRAY_LENGTH_INVAILD,
    ARRAY_INDEX_TYPE_ERROR,
    CAN_NOT_APPLY_ARRAY_INDEX,
    INVAILD_LEFT_EXPR,
    ASSIGN_TYPE_NOT_MATCH,
    EXPR_RETURN_VOID,
    EXPR_NOT_BASIC_TYPE,
    SUB_LEFT_VALUE_INVAILD,
    BREAK_POSITION_INVAILD,
    CONTINUE_POSITION_INVAILD,
    RETURN_TYPE_NOT_MATCH,
    FUNCTION_DECLARE_CONFLICT_WARN,
    FUNCTION_TYPE_NOT_MATCH_WARN
};

const string semanticErrorHints[] = {
        "重复定义变量",
        "使用未定义变量",
        "变量无法赋初值",
        "变量类型不允许为'void'",
        "全局变量无法初始化",
        "重复定义函数",
        "使用未定义的函数",
        "函数的形参列表和实参列表不匹配",
        "不允许在声明中初始化",
        "数组长度不符合规范",
        "数组索引类型不正确",
        "不能使用索引访问普通变量",
        "左值表达式不正确",
        "赋值的类型不正确",
        "表达式使用了返回值为void的函数",
        "表达式使用了非基本类型进行操作",
        "不允许使用基本类型减非基本类型",
        "break关键字没有处于switch,while,do-while或for语句下",
        "continue关键字没有处于while,do-while或for语句下",
        "返回值与声明不匹配",
        "函数参数列表定义与声明冲突",
        "函数返回类型定义与声明冲突"
};

enum InterCodeOperator{
    OP_EMPTY_INST,
    OP_DECLARE, OP_ENTER, OP_EXIT,
    OP_ASSIGN,
    OP_ADD,OP_SUB,OP_MUL,OP_DIV,OP_MOD,OP_MINUS,
    OP_GT,OP_GE,OP_EQ,OP_NEQ,OP_LE,OP_LT,
    OP_NOT,OP_AND,OP_OR,
    OP_LEA,OP_PTR_SET,OP_PTR_GET,
    OP_JMP,
    OP_JMP_TRUE,OP_JMP_FALSE,OP_JMP_NEQ,
    OP_ARG,
    OP_PROC, OP_CALL, OP_RETURN, OP_RETURN_WITH_VAL
};

const string interOpHints[] = {
        "EMPTY_INST",
        "DECLARE", "ENTER", "EXIT",
        "ASSIGN",
        "ADD","SUB","MUL","DIV","MOD","MINUS",
        "GT","GE","EQ","NEQ","LE","LT",
        "NOT","AND","OR",
        "LEA","PTR_SET","PTR_GET",
        "JMP",
        "JMP_TRUE","JMP_FALSE","JMP_NEQ",
        "ARG",
        "PROC", "CALL", "RETURN", "RETURN_WITH_VAL"
};

class Variable;
class Function;
class SymbolTable;
class IRGenerator;

#endif //SHAOLANG_COMMON_H
