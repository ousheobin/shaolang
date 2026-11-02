#include "x86_masm_codegen.h"

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
// 序言里 push ebp + push ebx 占掉 8 字节，槽位从第 2 格起分配
const int RESERVED_SLOTS = 1;
const int STACK_ALIGN = 16;

string to_dec(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

// 访存位移写成 MASM 的中缀形式：-8 变成 -8，8 变成 +8
string disp_text(int offset) {
    std::stringstream ss;
    ss << (offset < 0 ? "-" : "+") << (offset < 0 ? -offset : offset);
    return ss.str();
}

// setcc / movzx 用的是 32 位寄存器的低 8 位，al、cl 这种形态
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

X86MasmCodeGenerator::X86MasmCodeGenerator(SymbolTable *symTable, ostream &output)
        : out(output) {
    this -> symbolTable = symTable;
    this -> frame_size = 0;
    this -> label_seq = 0;
    this -> function_seq = 0;
    this -> epilogue_emitted = false;
    this -> needs_divmod = false;
}

void X86MasmCodeGenerator::line(const string & text) {
    out << text << std::endl;
}

int X86MasmCodeGenerator::align_up(int value,int alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

string X86MasmCodeGenerator::split_lo(long value) {
    // 字面量是 int32（词法层保证），低位字就是它本身
    return to_dec((int)value);
}

string X86MasmCodeGenerator::split_hi(long value) {
    return to_dec(value < 0 ? -1 : 0);
}

void X86MasmCodeGenerator::generate() {
    line(".486");
    line(".model flat, C");
    // 只用得到这两个库函数，声明在这里由 MASM 按 C 约定补下划线前缀
    line("EXTRN printf:PROC");
    line("EXTRN scanf:PROC");
    line(".code");
    vector<Function *> functions = symbolTable -> get_defined_functions();
    for(unsigned long i = 0 ; i < functions.size() ; i ++ ){
        generate_function(functions[i]);
    }
    if(needs_divmod){
        emit_divmod_helper();
    }
    emit_static_data();
    line("END");
}

void X86MasmCodeGenerator::generate_function(Function *function) {
    vector<IntermediateInstruct *> codes = function -> get_emitted_code();
    pending_args.clear();
    function_seq ++;
    allocate_frame(function,codes);

    string symbol = function_symbol(function -> get_function_name());
    line("PUBLIC " + symbol);
    line(symbol + " PROC");
    emit_prologue();
    emit_arguments(function);
    epilogue_emitted = false;
    emit_body(codes);
    if(!epilogue_emitted){
        // 出口块被优化掉时补齐退栈序列，否则函数会直接落入下一个函数
        emit_epilogue();
    }
    line(symbol + " ENDP");
    line("");
}

void X86MasmCodeGenerator::allocate_frame(Function *function, vector<IntermediateInstruct *> &codes) {
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

void X86MasmCodeGenerator::emit_prologue() {
    line("\tpush ebp");
    line("\tmov ebp, esp");
    // ebx 用来装右操作数的高位字，是 callee-saved，进函数先保存
    line("\tpush ebx");
    if(frame_size > 0){
        line("\tsub esp, " + to_dec(frame_size));
    }
    // cdecl 调用点保持 16 字节对齐，printf 一类库函数内部的 SSE 访存才不会错位；
    // 掩码直接写成 0FFFFFFF0h，省得各家汇编器对负立即数的处理不一致
    line("\tand esp, 0FFFFFFF0h");
}

void X86MasmCodeGenerator::emit_arguments(Function *function) {
    vector<Variable *> parameters = function -> get_parameters();
    for(unsigned long index = 0 ; index < parameters.size() ; index ++ ){
        int source = (8 + SLOT_SIZE * (int)index);
        int offset = parameters[index] -> get_frame_offset();
        line("\tmov " + string(LEFT_LOW) + ", " + slot_ref(source));
        line("\tmov " + slot_ref(offset) + ", " + string(LEFT_LOW));
        line("\tmov " + string(LEFT_LOW) + ", " + slot_ref(source + 4));
        line("\tmov " + slot_ref(offset + 4) + ", " + string(LEFT_LOW));
    }
}

void X86MasmCodeGenerator::emit_call(Function *function) {
    int amount = (int)pending_args.size();
    int size = align_up(SLOT_SIZE * amount,STACK_ALIGN);
    if(size > 0){
        line("\tsub esp, " + to_dec(size));
    }
    for(int index = 0 ; index < amount ; index ++ ){
        // pending_args 记录的是中间指令顺序，即源程序实参的逆序
        Variable * argument = pending_args[amount - 1 - index];
        load_pair(argument,LEFT_LOW,LEFT_HIGH);
        line("\tmov " + stack_ref(SLOT_SIZE * index) + ", " + string(LEFT_LOW));
        line("\tmov " + stack_ref(SLOT_SIZE * index + 4) + ", " + string(LEFT_HIGH));
    }
    if(function != NULL){
        line("\tcall " + function_symbol(function -> get_function_name()));
    }
    if(size > 0){
        line("\tadd esp, " + to_dec(size));
    }
    pending_args.clear();
}

void X86MasmCodeGenerator::emit_epilogue() {
    // ebx 是在 ebp 建立之后压栈的，落在 [ebp-4]，不能直接按 [ebp] 取
    line("\tlea esp, [ebp-4]");
    line("\tpop ebx");
    line("\tpop ebp");
    line("\tret");
}

void X86MasmCodeGenerator::emit_body(vector<IntermediateInstruct *> &codes) {
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
                    load_pair_immediate(left -> get_int_value(),LEFT_LOW,LEFT_HIGH);
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
                line("\tnot " + string(LEFT_LOW));
                line("\tnot " + string(LEFT_HIGH));
                line("\tadd " + string(LEFT_LOW) + ", 1");
                line("\tadc " + string(LEFT_HIGH) + ", 0");
                store_pair(result,LEFT_LOW,LEFT_HIGH);
                break;
            case OP_NOT:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\tor " + string(LEFT_LOW) + ", " + string(LEFT_HIGH));
                line("\tsete " + byte_of(LEFT_LOW));
                line("\tmovzx " + string(LEFT_LOW) + ", " + byte_of(LEFT_LOW));
                line("\txor " + string(LEFT_HIGH) + ", " + string(LEFT_HIGH));
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
                emit_logic(instruct,"and");
                break;
            case OP_OR:
                emit_logic(instruct,"or");
                break;
            case OP_JMP:
            case OP_RETURN:
                line("\tjmp " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_TRUE:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\tor " + string(LEFT_LOW) + ", " + string(LEFT_HIGH));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_FALSE:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                line("\tor " + string(LEFT_LOW) + ", " + string(LEFT_HIGH));
                line("\tje " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_NEQ:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                line("\tcmp " + string(LEFT_HIGH) + ", " + string(RIGHT_HIGH));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                line("\tcmp " + string(LEFT_LOW) + ", " + string(RIGHT_LOW));
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
                line("\tmov " + string(RIGHT_LOW) + ", " + word_at(string(LEFT_LOW)));
                line("\tmov " + string(RIGHT_HIGH) + ", " + word_at(string(LEFT_LOW) + "+4"));
                store_pair(result,RIGHT_LOW,RIGHT_HIGH);
                break;
            case OP_PTR_SET:
                load_pair(left,LEFT_LOW,LEFT_HIGH);
                load_pair(right,RIGHT_LOW,RIGHT_HIGH);
                line("\tmov " + word_at(string(LEFT_LOW)) + ", " + string(RIGHT_LOW));
                line("\tmov " + word_at(string(LEFT_LOW) + "+4") + ", " + string(RIGHT_HIGH));
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
                    line("\tmov " + word_at(string(LEFT_LOW)) + ", 0");
                    line("\tmov " + word_at(string(LEFT_LOW) + "+4") + ", 0");
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

void X86MasmCodeGenerator::emit_add() {
    line("\tadd " + string(LEFT_LOW) + ", " + string(RIGHT_LOW));
    line("\tadc " + string(LEFT_HIGH) + ", " + string(RIGHT_HIGH));
}

void X86MasmCodeGenerator::emit_sub() {
    line("\tsub " + string(LEFT_LOW) + ", " + string(RIGHT_LOW));
    line("\tsbb " + string(LEFT_HIGH) + ", " + string(RIGHT_HIGH));
}

void X86MasmCodeGenerator::emit_mul() {
    // 只保留低 64 位：a0*b0 用 mul 拿到完整积，交叉项 a0*b1 / a1*b0 各取低 32 位补进高位字
    line("\tpush " + string(LEFT_HIGH));
    line("\tpush " + string(LEFT_LOW));
    line("\tmul " + string(RIGHT_LOW));
    line("\tpush eax");
    line("\tmov eax, " + stack_ref(4));
    line("\timul eax, " + string(RIGHT_HIGH));
    line("\tadd " + string(LEFT_HIGH) + ", eax");
    line("\tmov eax, " + stack_ref(8));
    line("\timul eax, " + string(RIGHT_LOW));
    line("\tadd " + string(LEFT_HIGH) + ", eax");
    line("\tpop ecx");
    line("\tadd esp, 8");
    line("\tmov eax, ecx");
}

void X86MasmCodeGenerator::emit_div(IntermediateInstruct *instruct) {
    // 64/64 除法没有单条指令，商在 edx:eax，余数在 ecx:ebx
    needs_divmod = true;
    line("\tcall sl_divmod");
    store_pair(instruct -> getResultVar(),LEFT_LOW,LEFT_HIGH);
}

void X86MasmCodeGenerator::emit_mod(IntermediateInstruct *instruct) {
    needs_divmod = true;
    line("\tcall sl_divmod");
    store_pair(instruct -> getResultVar(),RIGHT_LOW,RIGHT_HIGH);
}

void X86MasmCodeGenerator::emit_compare(IntermediateInstruct *instruct,const char * hi_cc,const char * lo_cc) {
    load_pair(instruct -> getLeftArg(),LEFT_LOW,LEFT_HIGH);
    load_pair(instruct -> getRightArg(),RIGHT_LOW,RIGHT_HIGH);
    string hi_label = next_local_label("cmp_hi");
    string end_label = next_local_label("cmp_end");
    line("\tcmp " + string(LEFT_HIGH) + ", " + string(RIGHT_HIGH));
    line("\tjne " + hi_label);
    line("\tcmp " + string(LEFT_LOW) + ", " + string(RIGHT_LOW));
    line("\t" + string(lo_cc) + " " + byte_of(LEFT_LOW));
    line("\tmovzx " + string(LEFT_LOW) + ", " + byte_of(LEFT_LOW));
    line("\tjmp " + end_label);
    line(hi_label + ":");
    // 高位字是带符号比较；EQ/NEQ 走到这里时两个操作数的高位字不相等，集合条件取反即可
    line("\t" + string(hi_cc) + " " + byte_of(LEFT_LOW));
    line("\tmovzx " + string(LEFT_LOW) + ", " + byte_of(LEFT_LOW));
    line(end_label + ":");
    line("\txor " + string(LEFT_HIGH) + ", " + string(LEFT_HIGH));
    store_pair(instruct -> getResultVar(),LEFT_LOW,LEFT_HIGH);
}

void X86MasmCodeGenerator::emit_logic(IntermediateInstruct *instruct,const char * op) {
    // 与 C 的 && || 语义一致，两个操作数都先归一化成 0 / 1
    load_pair(instruct -> getLeftArg(),LEFT_LOW,LEFT_HIGH);
    line("\tor " + string(LEFT_LOW) + ", " + string(LEFT_HIGH));
    line("\tsetne " + byte_of(LEFT_LOW));
    line("\tmovzx " + string(LEFT_LOW) + ", " + byte_of(LEFT_LOW));
    load_pair(instruct -> getRightArg(),RIGHT_LOW,RIGHT_HIGH);
    line("\tor " + string(RIGHT_LOW) + ", " + string(RIGHT_HIGH));
    line("\tsetne " + byte_of(RIGHT_LOW));
    line("\tmovzx " + string(RIGHT_LOW) + ", " + byte_of(RIGHT_LOW));
    line("\t" + string(op) + " " + string(LEFT_LOW) + ", " + string(RIGHT_LOW));
    line("\txor " + string(LEFT_HIGH) + ", " + string(LEFT_HIGH));
    store_pair(instruct -> getResultVar(),LEFT_LOW,LEFT_HIGH);
}

void X86MasmCodeGenerator::emit_vararg_call(const string & format,const char * callee,bool address_value) {
    // cdecl：格式串在 [esp]，值或指针随后；16 字节窗口保证对齐
    line("\tmov " + string(RIGHT_LOW) + ", OFFSET " + string_symbol(format));
    line("\tsub esp, 16");
    line("\tmov " + stack_ref(0) + ", " + string(RIGHT_LOW));
    if(address_value){
        // 目标地址已经在左值寄存器对里
        line("\tmov " + stack_ref(4) + ", " + string(LEFT_LOW));
    }else{
        line("\tmov " + stack_ref(4) + ", " + string(LEFT_LOW));
        line("\tmov " + stack_ref(8) + ", " + string(LEFT_HIGH));
    }
    // 运行库的符号按原名引用，只有源程序里定义的函数才需要避开助记符
    line("\tcall " + c_symbol(callee));
    line("\tadd esp, 16");
}

X86MasmCodeGenerator::StorageKind X86MasmCodeGenerator::storage_of(Variable *variable) {
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

string X86MasmCodeGenerator::slot_ref(int offset) {
    return "DWORD PTR [ebp" + disp_text(offset) + "]";
}

string X86MasmCodeGenerator::slot_addr(int offset) {
    return "[ebp" + disp_text(offset) + "]";
}

string X86MasmCodeGenerator::stack_ref(int offset) {
    return "DWORD PTR [esp" + disp_text(offset) + "]";
}

string X86MasmCodeGenerator::word_at(const string & address) {
    return "DWORD PTR [" + address + "]";
}

void X86MasmCodeGenerator::load_pair_immediate(long value,const char * low_reg,const char * high_reg) {
    line("\tmov " + string(low_reg) + ", " + split_lo(value));
    line("\tmov " + string(high_reg) + ", " + split_hi(value));
}

void X86MasmCodeGenerator::load_pair(Variable *variable,const char * low_reg,const char * high_reg) {
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
        line("\tmov " + string(low_reg) + ", " + slot_ref(variable -> get_frame_offset()));
        line("\tmov " + string(high_reg) + ", " + slot_ref(variable -> get_frame_offset() + 4));
        return;
    }
    if(kind == STORAGE_STATIC){
        string symbol = global_symbol(variable);
        line("\tmov " + string(low_reg) + ", " + word_at(symbol));
        line("\tmov " + string(high_reg) + ", " + word_at(symbol + "+4"));
        return;
    }
    line("\txor " + string(low_reg) + ", " + string(low_reg));
    line("\txor " + string(high_reg) + ", " + string(high_reg));
}

void X86MasmCodeGenerator::load_address_pair(Variable *variable,const char * low_reg,const char * high_reg) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tlea " + string(low_reg) + ", " + slot_addr(variable -> get_frame_offset()));
        line("\txor " + string(high_reg) + ", " + string(high_reg));
        return;
    }
    if(kind == STORAGE_STRING){
        load_symbol_address(string_symbol(variable -> get_string_value()),low_reg);
        line("\txor " + string(high_reg) + ", " + string(high_reg));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),low_reg);
        line("\txor " + string(high_reg) + ", " + string(high_reg));
        return;
    }
    // 字面量与 void 表达式没有存储位置，取地址只可能出现在已报错的表达式里
    line("\txor " + string(low_reg) + ", " + string(low_reg));
    line("\txor " + string(high_reg) + ", " + string(high_reg));
}

void X86MasmCodeGenerator::store_pair(Variable *variable,const char * low_reg,const char * high_reg) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tmov " + slot_ref(variable -> get_frame_offset()) + ", " + string(low_reg));
        line("\tmov " + slot_ref(variable -> get_frame_offset() + 4) + ", " + string(high_reg));
        return;
    }
    if(kind == STORAGE_STATIC){
        string symbol = global_symbol(variable);
        line("\tmov " + word_at(symbol) + ", " + string(low_reg));
        line("\tmov " + word_at(symbol + "+4") + ", " + string(high_reg));
    }
}

void X86MasmCodeGenerator::load_symbol_address(const string & symbol,const char * dest) {
    // 32 位平内存模型下静态数据直接取绝对地址
    line("\tmov " + string(dest) + ", OFFSET " + symbol);
}

void X86MasmCodeGenerator::emit_divmod_helper() {
    // 64/64 带符号除法的逐位长除子程序。
    // 入参：被除数 edx:eax，除数 ebx:ecx（高位字在前）；
    // 返回：商 edx:eax，余数 ebx:ecx。
    // esi/edi 是 callee-saved，子程序拿它们当累加器，所以先进后出保持原值；
    // 三个临时槽在 push 之后分配，位置仍是 [esp] / [esp+4] / [esp+8]。
    // 除数为 0 时商取 0、余数取被除数，与 ARM64 后端 sdiv + msub 的行为一致。
    line("sl_divmod PROC");
    line("\tpush esi");
    line("\tpush edi");
    line("\tsub esp, 16");
    line("\txor esi, esi");
    line("\txor edi, edi");
    line("\ttest ecx, ecx");
    line("\tjne sl_divmod_abs");
    line("\ttest ebx, ebx");
    line("\tjne sl_divmod_abs");
    line("\tmov DWORD PTR [esp], eax");
    line("\tmov DWORD PTR [esp+4], edx");
    line("\txor eax, eax");
    line("\txor edx, edx");
    line("\tmov ecx, DWORD PTR [esp]");
    line("\tmov ebx, DWORD PTR [esp+4]");
    line("\tadd esp, 16");
    line("\tpop edi");
    line("\tpop esi");
    line("\tret");
    line("sl_divmod_abs:");
    line("\tmov DWORD PTR [esp], 0");
    line("\tmov DWORD PTR [esp+4], 0");
    line("\ttest edx, edx");
    line("\tjns sl_divmod_abs_d");
    line("\tnot eax");
    line("\tnot edx");
    line("\tadd eax, 1");
    line("\tadc edx, 0");
    line("\tmov DWORD PTR [esp], 1");
    line("sl_divmod_abs_d:");
    line("\ttest ebx, ebx");
    line("\tjns sl_divmod_loop_init");
    line("\tnot ecx");
    line("\tnot ebx");
    line("\tadd ecx, 1");
    line("\tadc ebx, 0");
    line("\tmov DWORD PTR [esp+4], 1");
    line("sl_divmod_loop_init:");
    line("\txor esi, esi");
    line("\txor edi, edi");
    line("\tmov DWORD PTR [esp+8], 64");
    line("sl_divmod_loop:");
    line("\tadd eax, eax");
    line("\tadc edx, edx");
    line("\tadc esi, esi");
    line("\tadc edi, edi");
    line("\tcmp edi, ebx");
    line("\tja sl_divmod_sub");
    line("\tjb sl_divmod_next");
    line("\tcmp esi, ecx");
    line("\tjb sl_divmod_next");
    line("sl_divmod_sub:");
    line("\tsub esi, ecx");
    line("\tsbb edi, ebx");
    line("\tor eax, 1");
    line("sl_divmod_next:");
    line("\tdec DWORD PTR [esp+8]");
    line("\tjne sl_divmod_loop");
    line("\tmov ecx, DWORD PTR [esp]");
    line("\txor ecx, DWORD PTR [esp+4]");
    line("\tje sl_divmod_sign_r");
    line("\tnot eax");
    line("\tnot edx");
    line("\tadd eax, 1");
    line("\tadc edx, 0");
    line("sl_divmod_sign_r:");
    line("\tmov ecx, DWORD PTR [esp]");
    line("\ttest ecx, ecx");
    line("\tje sl_divmod_out");
    line("\tnot esi");
    line("\tnot edi");
    line("\tadd esi, 1");
    line("\tadc edi, 0");
    line("sl_divmod_out:");
    line("\tmov ecx, esi");
    line("\tmov ebx, edi");
    line("\tadd esp, 16");
    line("\tpop edi");
    line("\tpop esi");
    line("\tret");
    line("sl_divmod ENDP");
    line("");
}

void X86MasmCodeGenerator::emit_static_data() {
    if(!string_contents.empty()){
        line(".const");
        for(unsigned long i = 0 ; i < string_contents.size() ; i ++ ){
            line(string_symbol(string_contents[i]) + " BYTE " + byte_list(string_contents[i]) + ",0");
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
    line(".data");
    for(unsigned long i = 0 ; i < globals.size() ; i ++ ){
        Variable * variable = globals[i];
        if(variable -> is_void() || variable -> get_type() == C_STRING || variable -> is_literal_value()){
            continue;
        }
        line("ALIGN 4");
        line(global_symbol(variable) + " DWORD " + (variable -> is_init()
            ? split_lo(variable -> get_int_value()) + ", " + split_hi(variable -> get_int_value())
            // 数组在静态区里连续占用长度个槽位，一个槽两个字
            : to_dec(2 * (variable -> is_arr() ? variable -> get_array_length() : 1)) + " DUP (0)"));
    }
}

string X86MasmCodeGenerator::local_label(const string & label) {
    // 中间标号形如 .L1，MASM 不认开头的点；同名标号在两个函数里会撞，所以带上函数序号
    string name = (!label.empty() && label[0] == '.') ? label.substr(1) : label;
    return "f" + to_dec(function_seq) + "_" + name;
}

string X86MasmCodeGenerator::next_local_label(const char * tag) {
    label_seq ++;
    return "f" + to_dec(function_seq) + "_L_" + tag + to_dec(label_seq);
}

string X86MasmCodeGenerator::c_symbol(const string & name) {
    // .model flat, C 已经让 MASM 按 C 约定给外部符号补下划线，源文本里写原名
    return name;
}

string X86MasmCodeGenerator::function_symbol(const string & name) {
    // 源程序里的函数名会直接变成 MASM 标号，而 add、sub、cmp 这些助记符在 MASM 里是
    // 保留字，做不了标号；入口 main 要留给 CRT，其余统一加前缀
    return name == "main" ? name : c_symbol("slf_" + name);
}

string X86MasmCodeGenerator::global_symbol(Variable *variable) {
    // 加前缀避免与运行时库的同名符号冲突；静态数据是文件内标号，不参与 C 命名约定
    return "slg_" + variable -> get_variable_name();
}

string X86MasmCodeGenerator::string_symbol(const string & content) {
    std::map<string,int>::iterator found = string_labels.find(content);
    if(found == string_labels.end()){
        int index = (int)string_contents.size();
        string_contents.push_back(content);
        string_labels[content] = index;
        found = string_labels.find(content);
    }
    return "sls_" + to_dec(found -> second);
}

string X86MasmCodeGenerator::int_format() {
    // msvcrt 家族只认 %I64d
    return "%I64d";
}

string X86MasmCodeGenerator::read_format(Variable *variable) {
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X86MasmCodeGenerator::print_format(Variable *variable) {
    if(variable != NULL && variable -> get_type() == C_STRING){
        return "%s";
    }
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X86MasmCodeGenerator::byte_list(const string & content) {
    // 逐字节写十进制数值，绕开 MASM 字符串转义与格式串里 % 的冲突
    string result = "";
    for(unsigned long i = 0 ; i < content.size() ; i ++ ){
        unsigned char ch = (unsigned char)content[i];
        std::stringstream ss;
        ss << (int)ch;
        if(i > 0){
            result += ",";
        }
        result += ss.str();
    }
    if(content.empty()){
        result = "0";
    }
    return result;
}
