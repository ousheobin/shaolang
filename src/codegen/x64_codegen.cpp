#include "x64_codegen.h"

#include "commons/error.h"
#include "commons/type_util.h"

#include <sstream>

namespace {

// 表达式暂存寄存器：r10 左值 / r11 右值，结果都落在 r10
const char * LEFT_REG = "r10";
const char * RIGHT_REG = "r11";
// System V x64：前 6 个实参走 rdi/rsi/rdx/rcx/r8/r9，第 7 个起放栈上
const char * ARGUMENT_REGISTERS[] = { "rdi","rsi","rdx","rcx","r8","r9" };
const int REGISTER_ARGUMENT_AMOUNT = 6;

const int SLOT_SIZE = 8;
const int STACK_ALIGN = 16;

string to_dec(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

}

X64CodeGenerator::X64CodeGenerator(SymbolTable *symTable, ostream &output)
        : out(output) {
    this -> symbolTable = symTable;
    this -> frame_size = 0;
    this -> epilogue_emitted = false;
}

void X64CodeGenerator::line(const string & text) {
    out << text << std::endl;
}

int X64CodeGenerator::align_up(int value,int alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void X64CodeGenerator::generate() {
    vector<Function *> functions = symbolTable -> get_defined_functions();
    line("\t.text");
    for(unsigned long i = 0 ; i < functions.size() ; i ++ ){
        generate_function(functions[i]);
    }
    emit_static_data();
    // 不声明这一段的话，链接器会把栈当成可执行并给出警告
    line("\t.section .note.GNU-stack,\"\",@progbits");
}

void X64CodeGenerator::generate_function(Function *function) {
    vector<IntermediateInstruct *> codes = function -> get_emitted_code();
    pending_args.clear();
    allocate_frame(function,codes);

    string symbol = function_symbol(function -> get_function_name());
    line("\t.p2align 4");
    line("\t.globl " + symbol);
    line("\t.type " + symbol + ",@function");
    line(symbol + ":");
    emit_prologue();
    emit_arguments(function);
    epilogue_emitted = false;
    emit_body(codes);
    if(!epilogue_emitted){
        // 出口块被优化掉时补齐退栈序列，否则函数会直接落入下一个函数
        emit_epilogue();
    }
    line("\t.size " + symbol + ",.-" + symbol);
    line("");
}

void X64CodeGenerator::allocate_frame(Function *function, vector<IntermediateInstruct *> &codes) {
    int slots = 0;
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
    frame_size = align_up(SLOT_SIZE * slots,STACK_ALIGN);
}

void X64CodeGenerator::emit_prologue() {
    line("\tpushq %rbp");
    line("\tmovq %rsp, %rbp");
    if(frame_size > 0){
        line("\tsubq $" + to_dec(frame_size) + ", %rsp");
    }
}

void X64CodeGenerator::emit_arguments(Function *function) {
    vector<Variable *> parameters = function -> get_parameters();
    int amount = (int)parameters.size();
    int register_amount = amount < REGISTER_ARGUMENT_AMOUNT ? amount : REGISTER_ARGUMENT_AMOUNT;
    for(int index = 0 ; index < register_amount ; index ++ ){
        int offset = parameters[index] -> get_frame_offset();
        line("\tmovq %" + string(ARGUMENT_REGISTERS[index]) + ", " + frame_ref(offset));
    }
    for(int index = REGISTER_ARGUMENT_AMOUNT ; index < amount ; index ++ ){
        int offset = parameters[index] -> get_frame_offset();
        // 寄存器装不下的实参由调用者放栈上：返回地址 8 加上已保存的 rbp 8
        int source = 16 + SLOT_SIZE * (index - REGISTER_ARGUMENT_AMOUNT);
        line("\tmovq " + to_dec(source) + "(%rbp), %" + string(RIGHT_REG));
        line("\tmovq %" + string(RIGHT_REG) + ", " + frame_ref(offset));
    }
}

void X64CodeGenerator::emit_call(Function *function) {
    int amount = (int)pending_args.size();
    int stack_amount = amount <= REGISTER_ARGUMENT_AMOUNT ? 0 : amount - REGISTER_ARGUMENT_AMOUNT;
    int size = align_up(SLOT_SIZE * stack_amount,STACK_ALIGN);
    if(size > 0){
        line("\tsubq $" + to_dec(size) + ", %rsp");
    }
    for(int index = 0 ; index < amount ; index ++ ){
        // pending_args 记录的是中间指令顺序，即源程序实参的逆序
        Variable * argument = pending_args[amount - 1 - index];
        if(index < REGISTER_ARGUMENT_AMOUNT){
            load_value(argument,ARGUMENT_REGISTERS[index]);
        }else{
            load_value(argument,RIGHT_REG);
            line("\tmovq %" + string(RIGHT_REG) + ", "
                 + to_dec(SLOT_SIZE * (index - REGISTER_ARGUMENT_AMOUNT)) + "(%rsp)");
        }
    }
    if(function != NULL){
        line("\tcall " + function_symbol(function -> get_function_name()));
    }
    if(size > 0){
        line("\taddq $" + to_dec(size) + ", %rsp");
    }
    pending_args.clear();
}

void X64CodeGenerator::emit_epilogue() {
    line("\tmovq %rbp, %rsp");
    line("\tpopq %rbp");
    line("\tret");
}

void X64CodeGenerator::emit_body(vector<IntermediateInstruct *> &codes) {
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
                    load_immediate(LEFT_REG,left -> get_int_value());
                    store_value(left,LEFT_REG);
                }
                break;
            case OP_ASSIGN:
                load_value(left,LEFT_REG);
                store_value(result,LEFT_REG);
                break;
            case OP_ADD:
                emit_two_value_op(instruct,"addq");
                break;
            case OP_SUB:
                emit_two_value_op(instruct,"subq");
                break;
            case OP_MUL:
                emit_two_value_op(instruct,"imulq");
                break;
            case OP_DIV:
                emit_div(instruct);
                break;
            case OP_MOD:
                emit_mod(instruct);
                break;
            case OP_MINUS:
                load_value(left,LEFT_REG);
                line("\tnegq %" + string(LEFT_REG));
                store_value(result,LEFT_REG);
                break;
            case OP_NOT:
                load_value(left,LEFT_REG);
                line("\ttestq %" + string(LEFT_REG) + ", %" + string(LEFT_REG));
                line("\tsete %" + string(LEFT_REG) + "b");
                line("\tmovzbl %" + string(LEFT_REG) + "b, %" + string(LEFT_REG) + "d");
                store_value(result,LEFT_REG);
                break;
            case OP_GT:
                emit_compare(instruct,"g");
                break;
            case OP_GE:
                emit_compare(instruct,"ge");
                break;
            case OP_LT:
                emit_compare(instruct,"l");
                break;
            case OP_LE:
                emit_compare(instruct,"le");
                break;
            case OP_EQ:
                emit_compare(instruct,"e");
                break;
            case OP_NEQ:
                emit_compare(instruct,"ne");
                break;
            case OP_AND:
                emit_logic(instruct,"andq");
                break;
            case OP_OR:
                emit_logic(instruct,"orq");
                break;
            case OP_JMP:
            case OP_RETURN:
                line("\tjmp " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_TRUE:
                load_value(left,LEFT_REG);
                line("\ttestq %" + string(LEFT_REG) + ", %" + string(LEFT_REG));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_FALSE:
                load_value(left,LEFT_REG);
                line("\ttestq %" + string(LEFT_REG) + ", %" + string(LEFT_REG));
                line("\tje " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_NEQ:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tcmpq %" + string(RIGHT_REG) + ", %" + string(LEFT_REG));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_ARG:
                pending_args.push_back(left);
                break;
            case OP_CALL:
                emit_call(instruct -> getFunction());
                store_value(result,"rax");
                break;
            case OP_PROC:
                emit_call(instruct -> getFunction());
                break;
            case OP_RETURN_WITH_VAL:
                load_value(left,"rax");
                line("\tjmp " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_EXIT:
                emit_epilogue();
                epilogue_emitted = true;
                break;
            case OP_LEA:
                load_address(left,LEFT_REG);
                store_value(result,LEFT_REG);
                break;
            case OP_PTR_GET:
                load_value(left,LEFT_REG);
                line("\tmovq (%" + string(LEFT_REG) + "), %" + string(LEFT_REG));
                store_value(result,LEFT_REG);
                break;
            case OP_PTR_SET:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tmovq %" + string(RIGHT_REG) + ", (%" + string(LEFT_REG) + ")");
                break;
            case OP_READ: {
                string format = read_format(result);
                const char * address_reg = ARGUMENT_REGISTERS[1];
                if(result -> is_cell()){
                    // input a[i] / input *p：写入的地址就是单元里存的指针值
                    load_value(result -> get_pointer(),address_reg);
                }else{
                    load_address(result,address_reg);
                }
                if(format == "%c"){
                    // scanf 只写低字节，先清零避免栈槽高位残留
                    line("\tmovq $0, (%" + string(address_reg) + ")");
                }
                emit_vararg_call(format,"scanf");
                break;
            }
            case OP_PRINT:
                load_value(left,ARGUMENT_REGISTERS[1]);
                emit_vararg_call(print_format(left),"printf");
                break;
            default:
                Error::showMessage(ErrorType::Error,
                        "内部错误：后端无法翻译指令 " + interOpHints[op]);
                break;
        }
    }
}

void X64CodeGenerator::emit_two_value_op(IntermediateInstruct *instruct,const char * op) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\t" + string(op) + " %" + string(RIGHT_REG) + ", %" + string(LEFT_REG));
    store_value(instruct -> getResultVar(),LEFT_REG);
}

void X64CodeGenerator::emit_compare(IntermediateInstruct *instruct,const char * condition) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tcmpq %" + string(RIGHT_REG) + ", %" + string(LEFT_REG));
    line("\tset" + string(condition) + " %" + string(LEFT_REG) + "b");
    line("\tmovzbl %" + string(LEFT_REG) + "b, %" + string(LEFT_REG) + "d");
    store_value(instruct -> getResultVar(),LEFT_REG);
}

void X64CodeGenerator::emit_logic(IntermediateInstruct *instruct,const char * op) {
    // 与 C 的 && || 语义一致，两个操作数都先归一化成 0 / 1
    load_value(instruct -> getLeftArg(),LEFT_REG);
    line("\ttestq %" + string(LEFT_REG) + ", %" + string(LEFT_REG));
    line("\tsetne %" + string(LEFT_REG) + "b");
    line("\tmovzbl %" + string(LEFT_REG) + "b, %" + string(LEFT_REG) + "d");
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\ttestq %" + string(RIGHT_REG) + ", %" + string(RIGHT_REG));
    line("\tsetne %" + string(RIGHT_REG) + "b");
    line("\tmovzbl %" + string(RIGHT_REG) + "b, %" + string(RIGHT_REG) + "d");
    line("\t" + string(op) + " %" + string(RIGHT_REG) + ", %" + string(LEFT_REG));
    store_value(instruct -> getResultVar(),LEFT_REG);
}

void X64CodeGenerator::emit_div(IntermediateInstruct *instruct) {
    // 被除数走 rdx:rax，idivq 只认一个寄存器操作数，除数放 r11
    load_value(instruct -> getLeftArg(),"rax");
    line("\tcqto");
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tidivq %" + string(RIGHT_REG));
    store_value(instruct -> getResultVar(),"rax");
}

void X64CodeGenerator::emit_mod(IntermediateInstruct *instruct) {
    load_value(instruct -> getLeftArg(),"rax");
    line("\tcqto");
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tidivq %" + string(RIGHT_REG));
    store_value(instruct -> getResultVar(),"rdx");
}

void X64CodeGenerator::emit_vararg_call(const string & format,const char * callee) {
    load_symbol_address(string_symbol(format),ARGUMENT_REGISTERS[0]);
    line("\txorl %eax, %eax");
    line("\tcall " + function_symbol(callee));
}

X64CodeGenerator::StorageKind X64CodeGenerator::storage_of(Variable *variable) {
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

string X64CodeGenerator::frame_ref(int offset) {
    return to_dec(offset) + "(%rbp)";
}

void X64CodeGenerator::load_immediate(const char * dest,long value) {
    // 字面量是 int32（词法层保证），movq 的 32 位立即数符号扩展就够了
    line("\tmovq $" + to_dec((int)value) + ", %" + string(dest));
}

void X64CodeGenerator::load_value(Variable *variable,const char * dest) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_IMMEDIATE){
        load_immediate(dest,variable -> get_int_value());
        return;
    }
    if(kind == STORAGE_STRING){
        // 字符串的值就是它的静态数据地址
        load_address(variable,dest);
        return;
    }
    if(kind == STORAGE_FRAME){
        line("\tmovq " + frame_ref(variable -> get_frame_offset()) + ", %" + string(dest));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),RIGHT_REG);
        line("\tmovq (%" + string(RIGHT_REG) + "), %" + string(dest));
        return;
    }
    line("\tmovq $0, %" + string(dest));
}

void X64CodeGenerator::load_address(Variable *variable,const char * dest) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tleaq " + frame_ref(variable -> get_frame_offset()) + ", %" + string(dest));
        return;
    }
    if(kind == STORAGE_STRING){
        load_symbol_address(string_symbol(variable -> get_string_value()),dest);
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),dest);
        return;
    }
    // 字面量与 void 表达式没有存储位置，取地址只可能出现在已报错的表达式里
    line("\tmovq $0, %" + string(dest));
}

void X64CodeGenerator::store_value(Variable *variable,const char * src) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tmovq %" + string(src) + ", " + frame_ref(variable -> get_frame_offset()));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),RIGHT_REG);
        line("\tmovq %" + string(src) + ", (%" + string(RIGHT_REG) + ")");
    }
}

void X64CodeGenerator::load_symbol_address(const string & symbol,const char * dest) {
    // x64 取静态数据地址一律 RIP 相对，重定位由链接器负责
    line("\tleaq " + symbol + "(%rip), %" + string(dest));
}

void X64CodeGenerator::emit_static_data() {
    if(!string_contents.empty()){
        line("\t.section .rodata");
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
    line("\t.data");
    for(unsigned long i = 0 ; i < globals.size() ; i ++ ){
        Variable * variable = globals[i];
        if(variable -> is_void() || variable -> get_type() == C_STRING || variable -> is_literal_value()){
            continue;
        }
        line("\t.p2align 3");
        string symbol = global_symbol(variable);
        // 与 aarch64 后端一致：标成对象类型并限制可见性，别撞进 libc 的同名符号
        line("\t.hidden " + symbol);
        line("\t.type " + symbol + ",@object");
        line(symbol + ":");
        if(variable -> is_init()){
            line("\t.quad " + to_dec(variable -> get_int_value()));
        }else{
            // 数组在静态区里连续占用长度个槽位
            int length = variable -> is_arr() ? variable -> get_array_length() : 1;
            line("\t.zero " + to_dec(SLOT_SIZE * length));
        }
    }
}

string X64CodeGenerator::local_label(const string & label) {
    return label;
}

string X64CodeGenerator::function_symbol(const string & name) {
    // 两套 x64 ABI 的 C 符号都不带下划线前缀
    return name;
}

string X64CodeGenerator::global_symbol(Variable *variable) {
    // 加前缀避免与运行时库的同名符号冲突
    return "slg_" + variable -> get_variable_name();
}

string X64CodeGenerator::string_symbol(const string & content) {
    std::map<string,int>::iterator found = string_labels.find(content);
    if(found == string_labels.end()){
        int index = (int)string_contents.size();
        string_contents.push_back(content);
        string_labels[content] = index;
        found = string_labels.find(content);
    }
    return "sls_" + to_dec(found -> second);
}

string X64CodeGenerator::int_format() {
    // glibc 用 %lld；windows 走 MASM 后端，那边是 %I64d
    return "%lld";
}

string X64CodeGenerator::read_format(Variable *variable) {
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X64CodeGenerator::print_format(Variable *variable) {
    if(variable != NULL && variable -> get_type() == C_STRING){
        return "%s";
    }
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X64CodeGenerator::escape(const string & content) {
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
