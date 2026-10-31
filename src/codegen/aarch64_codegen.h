#ifndef SHAOLANG_AARCH64_CODEGEN_H
#define SHAOLANG_AARCH64_CODEGEN_H

#include <map>
#include <ostream>
#include <vector>

#include "codegen/target.h"
#include "symbols/symbol_table.h"
#include "symbols/function.h"
#include "symbols/variable.h"
#include "ir/inter_code.h"

/**
 * 中间指令到 ARM64 (aarch64) 汇编的转换器，输出 Mach-O 与 ELF 两种汇编格式。
 *
 * 每个有存储的变量占一个 8 字节：局部变量与临时变量分配在栈帧中（相对 x29 的
 * 负偏移），全局变量分配在静态数据区；整型字面量使用立即数，字符串常量使用
 * 静态字符串符号。所有值都按 64 位有符号整数运算，x0~x3 用于表达式暂存，
 * x10/x11 只在地址计算与栈实参搬运时内联使用。
 */
class AArch64CodeGenerator {

private:

    enum StorageKind {
        STORAGE_IMMEDIATE,
        STORAGE_STRING,
        STORAGE_STATIC,
        STORAGE_FRAME,
        STORAGE_NOWHERE
    };

    ostream & out;
    SymbolTable * symbolTable;
    CompileTarget target;

    // 字符串常量按内容去重后分配静态符号
    std::map<string,int> string_labels;
    vector<string> string_contents;
    vector<Variable *> pending_args;

    int frame_size;
    bool epilogue_emitted;

    void generate_function(Function * function);
    void allocate_frame(Function * function,vector<IntermediateInstruct *> & codes);
    void emit_body(vector<IntermediateInstruct *> & codes);
    void emit_static_data();

    void emit_prologue();
    void emit_arguments(Function * function);
    void emit_call(Function * function);
    void emit_epilogue();

    void emit_two_value_op(IntermediateInstruct * instruct,const char * op);
    void emit_compare(IntermediateInstruct * instruct,const char * condition);
    void emit_logic(IntermediateInstruct * instruct,const char * op);
    void emit_vararg_call(const string & format,const char * callee);

    StorageKind storage_of(Variable * variable);
    string slot_ref(int offset);
    void slot_address(const char * dest,int offset);
    void load_immediate(const char * dest,long value);
    void load_value(Variable * variable,const char * dest);
    void load_address(Variable * variable,const char * dest);
    void store_value(Variable * variable,const char * src);
    void load_symbol_address(const string & symbol,const char * dest);
    void load_format(const char * format,const char * dest);

    string function_symbol(const string & name);
    string local_label(const string & label);
    string global_symbol(Variable * variable);
    string string_symbol(const string & content);
    string page_ref(const string & symbol);
    string page_off_ref(const string & symbol);
    static string read_format(Variable * variable);
    static string print_format(Variable * variable);
    static string escape(const string & content);
    static int align_up(int value,int alignment);

    void line(const string & text);

public:

    AArch64CodeGenerator(SymbolTable * symTable,ostream & output,const CompileTarget & asm_target);

    void generate();

};

#endif //SHAOLANG_AARCH64_CODEGEN_H
