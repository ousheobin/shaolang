#ifndef SHAOLANG_X64_CODEGEN_H
#define SHAOLANG_X64_CODEGEN_H

#include <map>
#include <ostream>
#include <vector>

#include "symbols/symbol_table.h"
#include "symbols/function.h"
#include "symbols/variable.h"
#include "ir/inter_code.h"

/**
 * 中间指令到 X64 (amd64) 汇编的转换器，输出 linux-x64 (ELF)。
 * windows-x64 的 MASM 文本由 X64MasmCodeGenerator 负责。
 *
 * 值都是 64 位，一个栈槽装一个值，与 ARM64 后端同构。调用约定是 System V x64：
 * 前 6 个实参走 rdi/rsi/rdx/rcx/r8/r9，第 7 个起放栈上，没有影子空间。
 * 表达式暂存用 r10/r11（volatile），除法走 rdx:rax，可变参数调用前把 al 清 0
 * （它告诉被调方传出了几个 XMM 实参）。
 */
class X64CodeGenerator {

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
    void emit_div(IntermediateInstruct * instruct);
    void emit_mod(IntermediateInstruct * instruct);
    void emit_vararg_call(const string & format,const char * callee);

    StorageKind storage_of(Variable * variable);
    string frame_ref(int offset);
    void load_immediate(const char * dest,long value);
    void load_value(Variable * variable,const char * dest);
    void load_address(Variable * variable,const char * dest);
    void store_value(Variable * variable,const char * src);
    void load_symbol_address(const string & symbol,const char * dest);

    string function_symbol(const string & name);
    string local_label(const string & label);
    string global_symbol(Variable * variable);
    string string_symbol(const string & content);
    string int_format();
    string read_format(Variable * variable);
    string print_format(Variable * variable);
    string escape(const string & content);
    static int align_up(int value,int alignment);

    void line(const string & text);

public:

    X64CodeGenerator(SymbolTable * symTable,ostream & output);

    void generate();

};

#endif //SHAOLANG_X64_CODEGEN_H
