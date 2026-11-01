#ifndef SHAOLANG_X86_MASM_CODEGEN_H
#define SHAOLANG_X86_MASM_CODEGEN_H

#include <map>
#include <ostream>
#include <vector>

#include "symbols/symbol_table.h"
#include "symbols/function.h"
#include "symbols/variable.h"
#include "ir/inter_code.h"

/**
 * 中间指令到 windows-x86 MASM 汇编的转换器，输出 ml.exe 能装配、link.exe 能链接的
 * COFF 文本（.model flat, C，cdecl 调用约定，实参全部压栈，返回值放 edx:eax）。
 *
 * 与 GAS 版的 i386 后端是同一套降序：每个 64 位值放进一对 32 位寄存器 eax:edx（低位在前），
 * 栈上一个值占一个 8 字节槽（两个相邻字），加减乘用 32 位指令拼出，64 位带符号除法由文件
 * 末尾按需发射的 sl_divmod 子程序逐位算出。表达式暂存用 eax:edx（左）与 ecx:ebx（右），
 * %ebx 是 callee-saved，序言统一压栈保存。区别只在语法：MASM 是 Intel 形式，目的在前，
 * 寄存器不带前缀，立即数不带 $，访存写成 DWORD PTR [ebp-8]。
 */
class X86MasmCodeGenerator {

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

    std::map<string,int> string_labels;
    vector<string> string_contents;
    vector<Variable *> pending_args;

    int frame_size;
    int label_seq;
    int function_seq;
    bool epilogue_emitted;
    bool needs_divmod;

    void generate_function(Function * function);
    void allocate_frame(Function * function,vector<IntermediateInstruct *> & codes);
    void emit_body(vector<IntermediateInstruct *> & codes);
    void emit_static_data();
    void emit_divmod_helper();

    void emit_prologue();
    void emit_arguments(Function * function);
    void emit_call(Function * function);
    void emit_epilogue();

    void emit_add();
    void emit_sub();
    void emit_mul();
    void emit_div(IntermediateInstruct * instruct);
    void emit_mod(IntermediateInstruct * instruct);
    void emit_compare(IntermediateInstruct * instruct,const char * hi_cc,const char * lo_cc);
    void emit_logic(IntermediateInstruct * instruct,const char * op);
    void emit_vararg_call(const string & format,const char * callee,bool address_value);

    StorageKind storage_of(Variable * variable);
    void load_pair(Variable * variable,const char * low_reg,const char * high_reg);
    void load_pair_immediate(long value,const char * low_reg,const char * high_reg);
    void load_address_pair(Variable * variable,const char * low_reg,const char * high_reg);
    void store_pair(Variable * variable,const char * low_reg,const char * high_reg);
    void load_symbol_address(const string & symbol,const char * dest);

    string slot_ref(int offset);
    string slot_addr(int offset);
    string stack_ref(int offset);
    string word_at(const string & address);
    string c_symbol(const string & name);
    string function_symbol(const string & name);
    string local_label(const string & label);
    string next_local_label(const char * tag);
    string global_symbol(Variable * variable);
    string string_symbol(const string & content);
    string int_format();
    string read_format(Variable * variable);
    string print_format(Variable * variable);
    string byte_list(const string & content);
    static int align_up(int value,int alignment);
    static string split_hi(long value);
    static string split_lo(long value);

    void line(const string & text);

public:

    X86MasmCodeGenerator(SymbolTable * symTable,ostream & output);

    void generate();

};

#endif //SHAOLANG_X86_MASM_CODEGEN_H
