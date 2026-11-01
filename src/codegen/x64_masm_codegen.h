#ifndef SHAOLANG_X64_MASM_CODEGEN_H
#define SHAOLANG_X64_MASM_CODEGEN_H

#include <map>
#include <ostream>
#include <vector>

#include "symbols/symbol_table.h"
#include "symbols/function.h"
#include "symbols/variable.h"
#include "ir/inter_code.h"

/**
 * 中间指令到 windows-x64 MASM 汇编的转换器，输出 ml64.exe 能装配、link.exe 能链接的
 * COFF 文本，调用约定是 Microsoft x64：前 4 个实参走 rcx/rdx/r8/r9，第 5 个起放栈上，
 * 每个调用点预留 32 字节影子空间。
 *
 * 降序与 GAS 版的 x64 后端一致：一个值就是一个 64 位寄存器或一个 8 字节栈槽，
 * 表达式暂存用 r10（左）与 r11（右），除法走 rdx:rax 加 idiv。区别只在语法：
 * Intel 形式、目的在前、访存写 QWORD PTR [rbp-8]。
 */
class X64MasmCodeGenerator {

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
    int function_seq;
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
    void load_value(Variable * variable,const char * dest);
    void load_immediate(const char * dest,long value);
    void load_address(Variable * variable,const char * dest);
    void store_value(Variable * variable,const char * src);
    void load_symbol_address(const string & symbol,const char * dest);

    string slot_ref(int offset);
    string slot_addr(int offset);
    string stack_ref(int offset);
    string word_at(const string & address);
    string function_symbol(const string & name);
    string local_label(const string & label);
    string global_symbol(Variable * variable);
    string string_symbol(const string & content);
    string int_format();
    string read_format(Variable * variable);
    string print_format(Variable * variable);
    string byte_list(const string & content);
    static int align_up(int value,int alignment);

    void line(const string & text);

public:

    X64MasmCodeGenerator(SymbolTable * symTable,ostream & output);

    void generate();

};

#endif //SHAOLANG_X64_MASM_CODEGEN_H
