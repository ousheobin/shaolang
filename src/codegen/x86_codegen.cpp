#include "x86_codegen.h"

#include "commons/error.h"
#include "commons/type_util.h"

#include <sstream>

namespace {

// 每个值是 64 位整数，放一对 32 位寄存器：低位在前
const char * LEFT_LOW = "eax";
const char * LEFT_HIGH = "edx";
const char * RIGHT_LOW = "ecx";
const char * RIGHT_HIGH = "ebx";

const int SLOT_SIZE = 8;
// 序言里 pushl %ebp + pushl %ebx 占掉 8 字节，槽位从第 2 格起分配
const int RESERVED_SLOTS = 1;
const int STACK_ALIGN = 16;

string to_dec(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

// setcc / movzbl 用的是 32 位寄存器的低 8 位，%al、%cl 这种形态
string byte_of(const char * reg) {
    string name(reg);
    if(name == "eax"){
        return "al";
    }
    if(name == "ebx"){
        return "bl";
    }
    if(name == "ecx"){
        return "cl";
    }
    if(name == "edx"){
        return "dl";
    }
    return name;
}

}

X86CodeGenerator::X86CodeGenerator(SymbolTable *symTable, ostream &output,
                                   const CompileTarget &asm_target)
        : out(output) {
    this -> symbolTable = symTable;
    this -> target = asm_target;
    this -> is_macho = asm_target.platform == SystemPlatform::MacOS;
    this -> is_linux = asm_target.platform == SystemPlatform::Linux;
    this -> frame_size = 0;
    this -> label_seq = 0;
    this -> epilogue_emitted = false;
    this -> needs_divmod = false;
}

void X86CodeGenerator::line(const string & text) {
    out << text << std::endl;
}

int X86CodeGenerator::align_up(int value,int alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

string X86CodeGenerator::split_lo(long value) {
    // 字面量是 int32（词法层保证），低位字就是它本身
    return to_dec((int)value);
}

string X86CodeGenerator::split_hi(long value) {
    return to_dec(value < 0 ? -1 : 0);
}

void X86CodeGenerator::generate() {
    vector<Function *> functions = symbolTable -> get_defined_functions();
    line("\t.text");
    if(is_macho){
        line("\t.subsections_via_symbols");
    }
    for(unsigned long i = 0 ; i < functions.size() ; i ++ ){
        generate_function(functions[i]);
    }
    if(needs_divmod){
        emit_divmod_helper();
    }
    emit_static_data();
    if(is_linux){
        // 不声明这一段的话，链接器会把栈当成可执行并给出警告
        line("\t.section .note.GNU-stack,\"\",@progbits");
    }
}

void X86CodeGenerator::generate_function(Function *function) {
    vector<IntermediateInstruct *> codes = function -> get_emitted_code();
    pending_args.clear();
    allocate_frame(function,codes);

    string symbol = function_symbol(function -> get_function_name());
    line("\t.p2align 4");
    line("\t.globl " + symbol);
    if(is_linux){
        line("\t.type " + symbol + ",@function");
    }
    line(symbol + ":");
    emit_prologue();
    emit_arguments(function);
    epilogue_emitted = false;
    emit_body(codes);
    if(!epilogue_emitted){
        // 出口块被优化掉时补齐退栈序列，否则函数会直接落入下一个函数
        emit_epilogue();
    }
    if(is_linux){
        line("\t.size " + symbol + ",.-" + symbol);
    }
    line("");
}

void X86CodeGenerator::allocate_frame(Function *function, vector<IntermediateInstruct *> &codes) {
    int slots = RESERVED_SLOTS;
    vector<Variable *> parameters = function -> get_parameters();
    for(unsigned long i = 0 ; i < parameters.size() ; i ++ ){
        // 形参的声明指令在进入函数体之前产生，不会出现在指令序列里，需要单独分配
        parameters[i] -> set_frame_offset(-SLOT_SIZE * (++slots));
    }
    for(unsigned long i = 0 ; i < codes.size() ; i ++ ){
        IntermediateInstruct * instruct = codes[i];
        if(!instruct -> is_declare()){
            continue;
        }
        Variable * variable = instruct -> getLeftArg();
        // 全局变量使用静态数据，字面量与 void 表达式不占栈槽
        if(variable == NULL || variable -> is_void() || variable -> is_literal_value()
           || variable -> get_type() == C_STRING || variable -> get_frame_offset() != 0
           || variable -> get_scope_path().size() == 1){
            continue;
        }
        // 数组按元素连续占用栈槽，首元素取最低的槽位，a[i] 就是基址加上 i 个槽
        int length = variable -> is_arr() ? variable -> get_array_length() : 1;
        variable -> set_frame_offset(-SLOT_SIZE * (slots + length));
        slots += length;
    }
    frame_size = align_up(SLOT_SIZE * (slots + 1),STACK_ALIGN);
}

void X86CodeGenerator::emit_prologue() {
    line("\tpushl %ebp");
    line("\tmovl %esp, %ebp");
    // %ebx 用来装右操作数的高位字，是 callee-saved，进函数先保存
    line("\tpushl %ebx");
    if(frame_size > 0){
        line("\tsubl $" + to_dec(frame_size) + ", %esp");
    }
    // cdecl 调用点保持 16 字节对齐，printf 一类库函数内部的 SSE 访存才不会错位
    line("\tandl $-16, %esp");
}

void X86CodeGenerator::emit_arguments(Function *function) {
    vector<Variable *> parameters = function -> get_parameters();
    for(unsigned long index = 0 ; index < parameters.size() ; index ++ ){
        int source = (8 + SLOT_SIZE * (int)index);
        int offset = parameters[index] -> get_frame_offset();
        line("\tmovl " + to_dec(source) + "(%ebp), %" + string(LEFT_LOW));
        line("\tmovl %" + string(LEFT_LOW) + ", " + frame_ref(offset));
        line("\tmovl " + to_dec(source + 4) + "(%ebp), %" + string(LEFT_LOW));
        line("\tmovl %" + string(LEFT_LOW) + ", " + frame_ref(offset + 4));
    }
}

void X86CodeGenerator::emit_call(Function *function) {
    int amount = (int)pending_args.size();
    int size = align_up(SLOT_SIZE * amount,STACK_ALIGN);
    if(size > 0){
        line("\tsubl $" + to_dec(size) + ", %esp");
    }
    for(int index = 0 ; index < amount ; index ++ ){
        // pending_args 记录的是中间指令顺序，即源程序实参的逆序
        Variable * argument = pending_args[amount - 1 - index];
        load_pair(argument,LEFT_LOW,LEFT_HIGH);
        line("\tmovl %" + string(LEFT_LOW) + ", " + to_dec(SLOT_SIZE * index) + "(%esp)");
        line("\tmovl %" + string(LEFT_HIGH) + ", " + to_dec(SLOT_SIZE * index + 4) + "(%esp)");
    }
    if(function != NULL){
        line("\tcall " + function_symbol(function -> get_function_name()));
    }
    if(size > 0){
        line("\taddl $" + to_dec(size) + ", %esp");
    }
    pending_args.clear();
}

void X86CodeGenerator::emit_epilogue() {
    line("\tmovl %ebp, %esp");
    line("\tpopl %ebx");
    line("\tpopl %ebp");
    line("\tret");
}

void X86CodeGenerator::emit_body(vector<IntermediateInstruct *> &codes) {
    for(unsigned long i = 0 ; i < codes.size() ; i ++ ){
        IntermediateInstruct * instruct = codes[i];
        if(instruct -> getLabel() != ""){
            line(local_label(instruct -> getLabel()) + ":");
        }
        InterCodeOperator op = instruct -> getInterCodeOperator();
        Variable * result = instruct -> getResultVar();
        Variable * left = instruct -> getLeftArg();
        Variable * right = instruct -> getRightArg();
        switch (op){
            case OP_EMPTY_INST:
            case OP_ENTER:
                break;
            case OP_DECLARE:
                // 字面量初值记录在变量符号上，前端不会为它生成赋值指令
                if(left != NULL && left -> is_init() && left -> get_frame_offset() != 0){
                    load_pair(left,LEFT_LOW,LEFT_HIGH);
                    store_pair(left,LEFT_LOW,LEFT_HIGH);
                }
                break;
            case OP_ASSIGN:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_ADD:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                emit_add();
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_SUB:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                emit_sub();
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_MUL:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                emit_mul();
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_DIV:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                emit_div(instruct);
                break;
            case OP_MOD:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                emit_mod(instruct);
                break;
            case OP_MINUS:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\tnotl %" + string(LEFT_LOW));
                line("\tnotl %" + string(LEFT_HIGH));
                line("\taddl $1, %" + string(LEFT_LOW));
                line("\tadcl $0, %" + string(LEFT_HIGH));
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_NOT:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\torl %" + string(LEFT_HIGH) + ", %" + string(LEFT_LOW));
                line("\tsete %" + byte_of(LEFT_LOW));
                line("\tmovzbl %" + byte_of(LEFT_LOW) + ", %" + string(LEFT_LOW));
                line("\txorl %" + string(LEFT_HIGH) + ", %" + string(LEFT_HIGH));
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_GT:
                emit_compare(instruct,"setg","seta");
                break;
            case OP_GE:
                emit_compare(instruct,"setge","setae");
                break;
            case OP_LT:
                emit_compare(instruct,"setl","setb");
                break;
            case OP_LE:
                emit_compare(instruct,"setle","setbe");
                break;
            case OP_EQ:
                emit_compare(instruct,"sete","sete");
                break;
            case OP_NEQ:
                emit_compare(instruct,"setne","setne");
                break;
            case OP_AND:
                emit_logic(instruct,"andl");
                break;
            case OP_OR:
                emit_logic(instruct,"orl");
                break;
            case OP_JMP:
            case OP_RETURN:
                line("\tjmp " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_TRUE:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\torl %" + string(LEFT_HIGH) + ", %" + string(LEFT_LOW));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_FALSE:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\torl %" + string(LEFT_HIGH) + ", %" + string(LEFT_LOW));
                line("\tje " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_NEQ:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                line("\tcmpl %" + string(RIGHT_HIGH) + ", %" + string(LEFT_HIGH));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                line("\tcmpl %" + string(RIGHT_LOW) + ", %" + string(LEFT_LOW));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_ARG:
                pending_args.push_back(left);
                break;
            case OP_CALL:
                emit_call(instruct -> getFunction());
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_PROC:
                emit_call(instruct -> getFunction());
                break;
            case OP_RETURN_WITH_VAL:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\tjmp " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_EXIT:
                emit_epilogue();
                epilogue_emitted = true;
                break;
            case OP_LEA:
                load_address_pair(left,LEFT_LOW,LEFT_HIGH);
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_PTR_GET:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\tmovl (%" + string(LEFT_LOW) + "), %" + string(RIGHT_LOW));
                line("\tmovl 4(%" + string(LEFT_LOW) + "), %" + string(RIGHT_HIGH));
                store_pair(result,RIGHT_LOW,RIGHT_HIGH);
                break;
            case OP_PTR_SET:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                line("\tmovl %" + string(RIGHT_LOW) + ", (%" + string(LEFT_LOW) + ")");
                line("\tmovl %" + string(RIGHT_HIGH) + ", 4(%" + string(LEFT_LOW) + ")");
                break;
            case OP_READ: {
                string format = read_format(result);
                if(result -> is_cell()){
                    // input a[i] / input *p：写入的地址就是单元里存的指针值
                    load_pair(result -> get_pointer(),LEFT_LOW,LEFT_HIGH);
                }else{
                    load_address_pair(result,LEFT_LOW,LEFT_HIGH);
                }
                if(format == "%c"){
                    // scanf 只写低字节，先清零避免槽里高位残留
                    line("\tmovl $0, (%" + string(LEFT_LOW) + ")");
                    line("\tmovl $0, 4(%" + string(LEFT_LOW) + ")");
                }
                emit_vararg_call(format,"scanf",true);
                break;
            }
            case OP_PRINT:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                emit_vararg_call(print_format(left),"printf",false);
                break;
            default:
                Error::showMessage(ErrorType::Error,
                        "内部错误：后端无法翻译指令 " + interOpHints[op]);
                break;
        }
    }
}

void X86CodeGenerator::emit_add() {
    line("\taddl %" + string(RIGHT_LOW) + ", %" + string(LEFT_LOW));
    line("\tadcl %" + string(RIGHT_HIGH) + ", %" + string(LEFT_HIGH));
}

void X86CodeGenerator::emit_sub() {
    line("\tsubl %" + string(RIGHT_LOW) + ", %" + string(LEFT_LOW));
    line("\tsbbl %" + string(RIGHT_HIGH) + ", %" + string(LEFT_HIGH));
}

void X86CodeGenerator::emit_mul() {
    // 只保留低 64 位：a0*b0 用 mull 拿到完整积，交叉项 a0*b1 / a1*b0 各取低 32 位补进高位字
    line("\tpushl %" + string(LEFT_HIGH));
    line("\tpushl %" + string(LEFT_LOW));
    line("\tmull %" + string(RIGHT_LOW));
    line("\tpushl %eax");
    line("\tmovl 4(%esp), %eax");
    line("\timull %" + string(RIGHT_HIGH) + ", %eax");
    line("\taddl %eax, %" + string(LEFT_HIGH));
    line("\tmovl 8(%esp), %eax");
    line("\timull %" + string(RIGHT_LOW) + ", %eax");
    line("\taddl %eax, %" + string(LEFT_HIGH));
    line("\tpopl %ecx");
    line("\taddl $8, %esp");
    line("\tmovl %ecx, %eax");
}

void X86CodeGenerator::emit_div(IntermediateInstruct *instruct) {
    // 64/64 除法没有单条指令，商在 edx:eax，余数在 ecx:ebx
    needs_divmod = true;
    line("\tcall sl_divmod");
    store_pair(instruct -> getResultVar(),LEFT_LOW,LEFT_HIGH);
}

void X86CodeGenerator::emit_mod(IntermediateInstruct *instruct) {
    needs_divmod = true;
    line("\tcall sl_divmod");
    store_pair(instruct -> getResultVar(),RIGHT_LOW,RIGHT_HIGH);
}

void X86CodeGenerator::emit_compare(IntermediateInstruct *instruct,const char * hi_cc,const char * lo_cc) {
    load_pair(instruct -> getLeftArg(),LEFT_LOW,LEFT_HIGH);
    load_pair(instruct -> getRightArg(),RIGHT_LOW,RIGHT_HIGH);
    string hi_label = next_local_label("cmp_hi");
    string end_label = next_local_label("cmp_end");
    line("\tcmpl %" + string(RIGHT_HIGH) + ", %" + string(LEFT_HIGH));
    line("\tjne " + hi_label);
    line("\tcmpl %" + string(RIGHT_LOW) + ", %" + string(LEFT_LOW));
    line("\t" + string(lo_cc) + " %" + byte_of(LEFT_LOW));
    line("\tmovzbl %" + byte_of(LEFT_LOW) + ", %" + string(LEFT_LOW));
    line("\tjmp " + end_label);
    line(hi_label + ":");
    // 高位字是带符号比较；EQ/NEQ 走到这里时两个操作数的高位字不相等，集合条件取反即可
    line("\t" + string(hi_cc) + " %" + byte_of(LEFT_LOW));
    line("\tmovzbl %" + byte_of(LEFT_LOW) + ", %" + string(LEFT_LOW));
    line(end_label + ":");
    line("\txorl %" + string(LEFT_HIGH) + ", %" + string(LEFT_HIGH));
    store_pair(instruct -> getResultVar(),LEFT_LOW,LEFT_HIGH);
}

void X86CodeGenerator::emit_logic(IntermediateInstruct *instruct,const char * op) {
    // 与 C 的 && || 语义一致，两个操作数都先归一化成 0 / 1
    load_pair(instruct -> getLeftArg(),LEFT_LOW,LEFT_HIGH);
    line("\torl %" + string(LEFT_HIGH) + ", %" + string(LEFT_LOW));
    line("\tsetne %" + byte_of(LEFT_LOW));
    line("\tmovzbl %" + byte_of(LEFT_LOW) + ", %" + string(LEFT_LOW));
    load_pair(instruct -> getRightArg(),RIGHT_LOW,RIGHT_HIGH);
    line("\torl %" + string(RIGHT_HIGH) + ", %" + string(RIGHT_LOW));
    line("\tsetne %" + byte_of(RIGHT_LOW));
    line("\tmovzbl %" + byte_of(RIGHT_LOW) + ", %" + string(RIGHT_LOW));
    line("\t" + string(op) + " %" + string(RIGHT_LOW) + ", %" + string(LEFT_LOW));
    line("\txorl %" + string(LEFT_HIGH) + ", %" + string(LEFT_HIGH));
    store_pair(instruct -> getResultVar(),LEFT_LOW,LEFT_HIGH);
}

void X86CodeGenerator::emit_vararg_call(const string & format,const char * callee,bool address_value) {
    // cdecl：格式串在 [esp]，值或指针随后；16 字节窗口保证对齐
    line("\tmovl $" + string_symbol(format) + ", %" + string(RIGHT_LOW));
    line("\tsubl $16, %esp");
    line("\tmovl %" + string(RIGHT_LOW) + ", (%esp)");
    if(address_value){
        // 目标地址已经在左值寄存器对里
        line("\tmovl %" + string(LEFT_LOW) + ", 4(%esp)");
    }else{
        line("\tmovl %" + string(LEFT_LOW) + ", 4(%esp)");
        line("\tmovl %" + string(LEFT_HIGH) + ", 8(%esp)");
    }
    line("\tcall " + function_symbol(callee));
    line("\taddl $16, %esp");
}

X86CodeGenerator::StorageKind X86CodeGenerator::storage_of(Variable *variable) {
    if(variable == NULL || variable -> is_void()){
        return STORAGE_NOWHERE;
    }
    if(variable -> get_type() == C_STRING){
        return STORAGE_STRING;
    }
    if(variable -> is_literal_value()){
        return STORAGE_IMMEDIATE;
    }
    if(variable -> get_frame_offset() != 0){
        return STORAGE_FRAME;
    }
    if(variable -> get_scope_path().size() == 1){
        return STORAGE_STATIC;
    }
    return STORAGE_NOWHERE;
}

string X86CodeGenerator::frame_ref(int offset) {
    return to_dec(offset) + "(%ebp)";
}

void X86CodeGenerator::load_pair_immediate(long value,const char * low_reg,const char * high_reg) {
    line("\tmovl $" + split_lo(value) + ", %" + string(low_reg));
    line("\tmovl $" + split_hi(value) + ", %" + string(high_reg));
}

void X86CodeGenerator::load_pair(Variable *variable,const char * low_reg,const char * high_reg) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_IMMEDIATE){
        load_pair_immediate(variable -> get_int_value(),low_reg,high_reg);
        return;
    }
    if(kind == STORAGE_STRING){
        // 字符串的值就是它的静态数据地址
        load_address_pair(variable,low_reg,high_reg);
        return;
    }
    if(kind == STORAGE_FRAME){
        line("\tmovl " + frame_ref(variable -> get_frame_offset()) + ", %" + string(low_reg));
        line("\tmovl " + frame_ref(variable -> get_frame_offset() + 4) + ", %" + string(high_reg));
        return;
    }
    if(kind == STORAGE_STATIC){
        string symbol = global_symbol(variable);
        line("\tmovl " + symbol + ", %" + string(low_reg));
        line("\tmovl " + symbol + "+4, %" + string(high_reg));
        return;
    }
    line("\txorl %" + string(low_reg) + ", %" + string(low_reg));
    line("\txorl %" + string(high_reg) + ", %" + string(high_reg));
}

void X86CodeGenerator::load_address_pair(Variable *variable,const char * low_reg,const char * high_reg) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tleal " + frame_ref(variable -> get_frame_offset()) + ", %" + string(low_reg));
        line("\txorl %" + string(high_reg) + ", %" + string(high_reg));
        return;
    }
    if(kind == STORAGE_STRING){
        load_symbol_address(string_symbol(variable -> get_string_value()),low_reg);
        line("\txorl %" + string(high_reg) + ", %" + string(high_reg));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),low_reg);
        line("\txorl %" + string(high_reg) + ", %" + string(high_reg));
        return;
    }
    // 字面量与 void 表达式没有存储位置，取地址只可能出现在已报错的表达式里
    line("\txorl %" + string(low_reg) + ", %" + string(low_reg));
    line("\txorl %" + string(high_reg) + ", %" + string(high_reg));
}

void X86CodeGenerator::store_pair(Variable *variable,const char * low_reg,const char * high_reg) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tmovl %" + string(low_reg) + ", " + frame_ref(variable -> get_frame_offset()));
        line("\tmovl %" + string(high_reg) + ", " + frame_ref(variable -> get_frame_offset() + 4));
        return;
    }
    if(kind == STORAGE_STATIC){
        string symbol = global_symbol(variable);
        line("\tmovl %" + string(low_reg) + ", " + symbol);
        line("\tmovl %" + string(high_reg) + ", " + symbol + "+4");
    }
}

void X86CodeGenerator::load_symbol_address(const string & symbol,const char * dest) {
    // 32 位目标上静态数据直接取绝对地址
    line("\tmovl $" + symbol + ", %" + string(dest));
}

void X86CodeGenerator::emit_divmod_helper() {
    // 64/64 带符号除法的逐位长除子程序。
    // 入参：被除数 edx:eax，除数 ebx:ecx（高位字在前）；
    // 返回：商 edx:eax，余数 ebx:ecx；破坏 esi/edi，不碰 callee-saved。
    // 除数为 0 时商取 0、余数取被除数，与 ARM64 后端 sdiv + msub 的行为一致。
    line("\t.p2align 4");
    line("sl_divmod:");
    line("\tsubl $16, %esp");
    line("\txorl %esi, %esi");
    line("\txorl %edi, %edi");
    line("\ttestl %ecx, %ecx");
    line("\tjne sl_divmod_abs");
    line("\ttestl %ebx, %ebx");
    line("\tjne sl_divmod_abs");
    line("\tmovl %eax, (%esp)");
    line("\tmovl %edx, 4(%esp)");
    line("\txorl %eax, %eax");
    line("\txorl %edx, %edx");
    line("\tmovl (%esp), %ecx");
    line("\tmovl 4(%esp), %ebx");
    line("\taddl $16, %esp");
    line("\tret");
    line("sl_divmod_abs:");
    line("\tmovl $0, (%esp)");
    line("\tmovl $0, 4(%esp)");
    line("\ttestl %edx, %edx");
    line("\tjns sl_divmod_abs_d");
    line("\tnotl %eax");
    line("\tnotl %edx");
    line("\taddl $1, %eax");
    line("\tadcl $0, %edx");
    line("\tmovl $1, (%esp)");
    line("sl_divmod_abs_d:");
    line("\ttestl %ebx, %ebx");
    line("\tjns sl_divmod_loop_init");
    line("\tnotl %ecx");
    line("\tnotl %ebx");
    line("\taddl $1, %ecx");
    line("\tadcl $0, %ebx");
    line("\tmovl $1, 4(%esp)");
    line("sl_divmod_loop_init:");
    line("\txorl %esi, %esi");
    line("\txorl %edi, %edi");
    line("\tmovl $64, 8(%esp)");
    line("sl_divmod_loop:");
    line("\taddl %eax, %eax");
    line("\tadcl %edx, %edx");
    line("\tadcl %esi, %esi");
    line("\tadcl %edi, %edi");
    line("\tcmpl %ebx, %edi");
    line("\tja sl_divmod_sub");
    line("\tjb sl_divmod_next");
    line("\tcmpl %ecx, %esi");
    line("\tjb sl_divmod_next");
    line("sl_divmod_sub:");
    line("\tsubl %ecx, %esi");
    line("\tsbbl %ebx, %edi");
    line("\torl $1, %eax");
    line("sl_divmod_next:");
    line("\tdecl 8(%esp)");
    line("\tjne sl_divmod_loop");
    line("\tmovl (%esp), %ecx");
    line("\txorl 4(%esp), %ecx");
    line("\tje sl_divmod_sign_r");
    line("\tnotl %eax");
    line("\tnotl %edx");
    line("\taddl $1, %eax");
    line("\tadcl $0, %edx");
    line("sl_divmod_sign_r:");
    line("\tmovl (%esp), %ecx");
    line("\ttestl %ecx, %ecx");
    line("\tje sl_divmod_out");
    line("\tnotl %esi");
    line("\tnotl %edi");
    line("\taddl $1, %esi");
    line("\tadcl $0, %edi");
    line("sl_divmod_out:");
    line("\tmovl %esi, %ecx");
    line("\tmovl %edi, %ebx");
    line("\taddl $16, %esp");
    line("\tret");
    line("");
}

void X86CodeGenerator::emit_static_data() {
    if(!string_contents.empty()){
        if(is_macho){
            line("\t.section __TEXT,__cstring,cstring_literals");
        }else if(is_linux){
            line("\t.section .rodata");
        }else{
            line("\t.section .rdata,\"dr\"");
        }
        for(unsigned long i = 0 ; i < string_contents.size() ; i ++ ){
            line(string_symbol(string_contents[i]) + ":");
            line("\t.asciz \"" + escape(string_contents[i]) + "\"");
        }
    }
    const vector<Variable *> & globals = symbolTable -> get_global_variables();
    bool has_data = false;
    for(unsigned long i = 0 ; i < globals.size() ; i ++ ){
        Variable * variable = globals[i];
        if(!variable -> is_void() && variable -> get_type() != C_STRING && !variable -> is_literal_value()){
            has_data = true;
            break;
        }
    }
    if(!has_data){
        return;
    }
    line(is_macho ? "\t.section __DATA,__data" : "\t.data");
    for(unsigned long i = 0 ; i < globals.size() ; i ++ ){
        Variable * variable = globals[i];
        if(variable -> is_void() || variable -> get_type() == C_STRING || variable -> is_literal_value()){
            continue;
        }
        line("\t.p2align 3");
        if(is_linux){
            // 与 aarch64 后端一致：标成对象类型并限制可见性，别撞进 libc 的同名符号
            string name = global_symbol(variable);
            line("\t.hidden " + name);
            line("\t.type " + name + ",@object");
        }
        line(global_symbol(variable) + ":");
        if(variable -> is_init()){
            line("\t.quad " + to_dec(variable -> get_int_value()));
        }else{
            // 数组在静态区里连续占用长度个槽位
            int length = variable -> is_arr() ? variable -> get_array_length() : 1;
            line("\t.zero " + to_dec(SLOT_SIZE * length));
        }
    }
}

string X86CodeGenerator::local_label(const string & label) {
    // Mach-O 里以 '.' 开头的标号会被当作外部符号，条件跳转只认 L 开头的局部标号
    if(is_macho && label.size() > 1 && label[0] == '.'){
        return label.substr(1);
    }
    return label;
}

string X86CodeGenerator::next_local_label(const char * tag) {
    label_seq ++;
    // 'L' 前缀在 Mach-O 与 COFF 里都是文件内标号，不会进符号表
    return string("L_") + tag + to_dec(label_seq);
}

string X86CodeGenerator::c_symbol(const string & name) {
    // win32 与 Mach-O 的 i386 都给 C 符号加下划线前缀，ELF 不加
    return is_linux ? name : "_" + name;
}

string X86CodeGenerator::function_symbol(const string & name) {
    return c_symbol(name);
}

string X86CodeGenerator::global_symbol(Variable *variable) {
    // 加前缀避免与运行时库的同名符号冲突
    return c_symbol("slg_" + variable -> get_variable_name());
}

string X86CodeGenerator::string_symbol(const string & content) {
    std::map<string,int>::iterator found = string_labels.find(content);
    if(found == string_labels.end()){
        int index = (int)string_contents.size();
        string_contents.push_back(content);
        string_labels[content] = index;
        found = string_labels.find(content);
    }
    return c_symbol("sls_" + to_dec(found -> second));
}

string X86CodeGenerator::int_format() {
    // msvcrt 家族只认 %I64d，glibc 与 Mach-O 的 libc 用 %lld
    return target.platform == SystemPlatform::Windows ? "%I64d" : "%lld";
}

string X86CodeGenerator::read_format(Variable *variable) {
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X86CodeGenerator::print_format(Variable *variable) {
    if(variable != NULL && variable -> get_type() == C_STRING){
        return "%s";
    }
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X86CodeGenerator::escape(const string & content) {
    string result = "";
    for(unsigned long i = 0 ; i < content.size() ; i ++ ){
        unsigned char ch = (unsigned char)content[i];
        if(ch == '"' || ch == '\\'){
            result += '\\';
            result += (char)ch;
        }else if(ch >= ' ' && ch < 0x7f){
            result += (char)ch;
        }else{
            // 固定三位八进制转义，避免与后续数字粘连
            result += '\\';
            result += (char)('0' + (ch / 64));
            result += (char)('0' + ((ch / 8) % 8));
            result += (char)('0' + (ch % 8));
        }
    }
    return result;
}
