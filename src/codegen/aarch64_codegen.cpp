#include "aarch64_codegen.h"

#include "commons/error.h"
#include "commons/type_util.h"

#include <sstream>

namespace {

// 表达式暂存寄存器：x0 左值 / x1 右值 / x2 结果 / x3 余数暂存
const char * LEFT_REG = "x0";
const char * RIGHT_REG = "x1";
const char * RESULT_REG = "x2";
const char * REMAINDER_REG = "x3";
// 地址计算与栈实参搬运的内联暂存寄存器，避开 x0~x7 的传参寄存器
const char * ADDRESS_REG = "x10";
const char * STACK_ARG_REG = "x11";
const char * VALUE_REGISTERS[] = { "x0","x1","x2","x3","x4","x5","x6","x7" };

const int REGISTER_ARGUMENT_AMOUNT = 8;
const int SLOT_SIZE = 8;
const int STACK_ALIGN = 16;
// 可变参数函数在 sp 处的调用者窗口，覆盖 x0~x7 八个槽位
const int VARARG_WINDOW_SIZE = 64;

string to_dec(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

}

AArch64CodeGenerator::AArch64CodeGenerator(SymbolTable *symTable, ostream &output,
                                           const CompileTarget &asm_target)
        : out(output) {
    this -> symbolTable = symTable;
    this -> target = asm_target;
    this -> frame_size = 0;
    this -> epilogue_emitted = false;
}

void AArch64CodeGenerator::line(const string & text) {
    out << text << std::endl;
}

int AArch64CodeGenerator::align_up(int value,int alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void AArch64CodeGenerator::generate() {
    vector<Function *> functions = symbolTable -> get_defined_functions();
    line("\t.text");
    if(target.platform == SystemPlatform::MacOS){
        // 告诉汇编器每个符号都能自成数据段，静态符号无需手工分段
        line("\t.subsections_via_symbols");
    }
    for(unsigned long i = 0 ; i < functions.size() ; i ++ ){
        generate_function(functions[i]);
    }
    emit_static_data();
}

void AArch64CodeGenerator::generate_function(Function *function) {
    vector<IntermediateInstruct *> codes = function -> get_emitted_code();
    pending_args.clear();
    allocate_frame(function,codes);

    string symbol = function_symbol(function -> get_function_name());
    line("\t.p2align 2");
    line("\t.globl " + symbol);
    if(target.platform != SystemPlatform::MacOS){
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
    if(target.platform != SystemPlatform::MacOS){
        line("\t.size " + symbol + ",.-" + symbol);
    }
    line("");
}

void AArch64CodeGenerator::allocate_frame(Function *function, vector<IntermediateInstruct *> &codes) {
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

void AArch64CodeGenerator::emit_prologue() {
    line("\tstp x29, x30, [sp, #-16]!");
    line("\tmov x29, sp");
    if(frame_size > 0){
        if(frame_size <= 4095){
            line("\tsub sp, sp, #" + to_dec(frame_size));
        }else{
            // add/sub 的立即数只有 12 位无符号范围，超大栈帧先取到暂存寄存器
            load_immediate(ADDRESS_REG,frame_size);
            line("\tsub sp, sp, " + string(ADDRESS_REG));
        }
    }
}

void AArch64CodeGenerator::emit_arguments(Function *function) {
    vector<Variable *> parameters = function -> get_parameters();
    int amount = (int)parameters.size();
    int register_amount = amount < REGISTER_ARGUMENT_AMOUNT ? amount : REGISTER_ARGUMENT_AMOUNT;
    for(int index = 0 ; index < register_amount ; index ++ ){
        int offset = parameters[index] -> get_frame_offset();
        line("\tstr " + string(VALUE_REGISTERS[index]) + ", " + slot_ref(offset));
    }
    for(int index = REGISTER_ARGUMENT_AMOUNT ; index < amount ; index ++ ){
        int offset = parameters[index] -> get_frame_offset();
        // 第 9 个起的实参由调用者放在栈上，相对保存后的 fp 位置固定
        line("\tldur " + string(STACK_ARG_REG) + ", " + slot_ref(16 + SLOT_SIZE * (index - REGISTER_ARGUMENT_AMOUNT)));
        line("\tstr " + string(STACK_ARG_REG) + ", " + slot_ref(offset));
    }
}

void AArch64CodeGenerator::emit_call(Function *function) {
    int amount = (int)pending_args.size();
    int stack_amount = amount <= REGISTER_ARGUMENT_AMOUNT ? 0 : amount - REGISTER_ARGUMENT_AMOUNT;
    int stack_size = align_up(SLOT_SIZE * stack_amount,STACK_ALIGN);
    if(stack_size > 0){
        line("\tsub sp, sp, #" + to_dec(stack_size));
    }
    for(int index = 0 ; index < amount ; index ++ ){
        // pending_args 记录的是中间指令顺序，即源程序实参的逆序
        Variable * argument = pending_args[amount - 1 - index];
        if(index < REGISTER_ARGUMENT_AMOUNT){
            load_value(argument,VALUE_REGISTERS[index]);
        }else{
            load_value(argument,STACK_ARG_REG);
            line("\tstr " + string(STACK_ARG_REG) + ", [sp, #"
                 + to_dec(SLOT_SIZE * (index - REGISTER_ARGUMENT_AMOUNT)) + "]");
        }
    }
    if(function != NULL){
        line("\tbl " + function_symbol(function -> get_function_name()));
    }
    if(stack_size > 0){
        line("\tadd sp, sp, #" + to_dec(stack_size));
    }
    pending_args.clear();
}

void AArch64CodeGenerator::emit_epilogue() {
    line("\tmov sp, x29");
    line("\tldp x29, x30, [sp], #16");
    line("\tret");
}

void AArch64CodeGenerator::emit_body(vector<IntermediateInstruct *> &codes) {
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
                emit_two_value_op(instruct,"add");
                break;
            case OP_SUB:
                emit_two_value_op(instruct,"sub");
                break;
            case OP_MUL:
                emit_two_value_op(instruct,"mul");
                break;
            case OP_DIV:
                emit_two_value_op(instruct,"sdiv");
                break;
            case OP_MOD:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tsdiv " + string(REMAINDER_REG) + ", x0, x1");
                line("\tmsub " + string(RESULT_REG) + ", x3, x1, x0");
                store_value(result,RESULT_REG);
                break;
            case OP_MINUS:
                load_value(left,LEFT_REG);
                line("\tsub " + string(RESULT_REG) + ", xzr, x0");
                store_value(result,RESULT_REG);
                break;
            case OP_NOT:
                load_value(left,LEFT_REG);
                line("\tcmp x0, #0");
                line("\tcset " + string(RESULT_REG) + ", eq");
                store_value(result,RESULT_REG);
                break;
            case OP_GT:
                emit_compare(instruct,"gt");
                break;
            case OP_GE:
                emit_compare(instruct,"ge");
                break;
            case OP_LT:
                emit_compare(instruct,"lt");
                break;
            case OP_LE:
                emit_compare(instruct,"le");
                break;
            case OP_EQ:
                emit_compare(instruct,"eq");
                break;
            case OP_NEQ:
                emit_compare(instruct,"ne");
                break;
            case OP_AND:
                emit_logic(instruct,"and");
                break;
            case OP_OR:
                emit_logic(instruct,"orr");
                break;
            case OP_JMP:
            case OP_RETURN:
                line("\tb " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_TRUE:
                load_value(left,LEFT_REG);
                line("\tcbnz x0, " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_FALSE:
                load_value(left,LEFT_REG);
                line("\tcbz x0, " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_NEQ:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tcmp x0, x1");
                line("\tb.ne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_ARG:
                pending_args.push_back(left);
                break;
            case OP_CALL:
                emit_call(instruct -> getFunction());
                store_value(result,LEFT_REG);
                break;
            case OP_PROC:
                emit_call(instruct -> getFunction());
                break;
            case OP_RETURN_WITH_VAL:
                load_value(left,LEFT_REG);
                line("\tb " + local_label(instruct -> getJumpTarget() -> getLabel()));
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
                line("\tldr " + string(RESULT_REG) + ", [x0]");
                store_value(result,RESULT_REG);
                break;
            case OP_PTR_SET:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tstr x1, [x0]");
                break;
            case OP_READ: {
                string format = read_format(result);
                if(result -> is_cell()){
                    // input a[i] / input *p：写入的地址就是单元里存的指针值
                    load_value(result -> get_pointer(),RIGHT_REG);
                }else{
                    load_address(result,RIGHT_REG);
                }
                if(format == "%c"){
                    // scanf 只写低字节，先清零避免栈槽高位残留
                    line("\tstr xzr, [x1]");
                }
                emit_vararg_call(format,"scanf");
                break;
            }
            case OP_PRINT:
                load_value(left,RIGHT_REG);
                emit_vararg_call(print_format(left),"printf");
                break;
            default:
                Error::showMessage(ErrorType::Error,
                        "内部错误：后端无法翻译指令 " + interOpHints[op]);
                break;
        }
    }
}

void AArch64CodeGenerator::emit_two_value_op(IntermediateInstruct *instruct,const char * op) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\t" + string(op) + " " + string(RESULT_REG) + ", x0, x1");
    store_value(instruct -> getResultVar(),RESULT_REG);
}

void AArch64CodeGenerator::emit_compare(IntermediateInstruct *instruct,const char * condition) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tcmp x0, x1");
    line("\tcset " + string(RESULT_REG) + ", " + string(condition));
    store_value(instruct -> getResultVar(),RESULT_REG);
}

void AArch64CodeGenerator::emit_logic(IntermediateInstruct *instruct,const char * op) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    // 与 C 的 && || 语义一致，两个操作数都先归一化成 0 / 1
    line("\tcmp x0, #0");
    line("\tcset x0, ne");
    line("\tcmp x1, #0");
    line("\tcset x1, ne");
    line("\t" + string(op) + " " + string(RESULT_REG) + ", x0, x1");
    store_value(instruct -> getResultVar(),RESULT_REG);
}

void AArch64CodeGenerator::emit_vararg_call(const string & format,const char * callee) {
    // Apple ARM64 要求调用者在 sp 处预留 64 字节窗口，由被调用者从窗口取回可变参数；
    // AAPCS64 (Linux) 直接使用 x1，所以同一个值要写入两个位置。
    line("\tsub sp, sp, #" + to_dec(VARARG_WINDOW_SIZE));
    line("\tstr x1, [sp]");
    load_format(format.c_str(),LEFT_REG);
    line("\tmov w8, #0");
    line("\tbl " + function_symbol(callee));
    line("\tadd sp, sp, #" + to_dec(VARARG_WINDOW_SIZE));
}

AArch64CodeGenerator::StorageKind AArch64CodeGenerator::storage_of(Variable *variable) {
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

string AArch64CodeGenerator::slot_ref(int offset) {
    if(offset >= -256 && offset <= 255){
        return "[x29, #" + to_dec(offset) + "]";
    }
    slot_address(ADDRESS_REG,offset);
    return "[" + string(ADDRESS_REG) + "]";
}

void AArch64CodeGenerator::load_immediate(const char * dest,long value) {
    // AArch64 的 mov 立即数只能是 16 位段模式，超出范围要用 movz / movk 拼出来
    if(value >= -65536 && value <= 65535){
        line("\tmov " + string(dest) + ", #" + to_dec((int) value));
        return;
    }
    unsigned long bits = (unsigned long) value;
    unsigned long low = bits & 0xffffUL;
    line("\tmovz " + string(dest) + ", #" + to_dec((int) low));
    for(int shift = 16 ; shift <= 48 ; shift += 16){
        unsigned long part = (bits >> shift) & 0xffffUL;
        if(part != 0){
            line("\tmovk " + string(dest) + ", #" + to_dec((int) part) + ", lsl #" + to_dec(shift));
        }
    }
}

void AArch64CodeGenerator::slot_address(const char * dest,int offset) {
    // x29 相对寻址的立即数只覆盖 [-256,255]，超出栈帧深度时要先算出槽地址再访存
    bool below = offset < 0;
    int span = below ? -offset : offset;
    if(span <= 4095){
        line(string(below ? "\tsub " : "\tadd ") + string(dest) + ", x29, #" + to_dec(span));
        return;
    }
    load_immediate(dest,span);
    line(string(below ? "\tsub " : "\tadd ") + string(dest) + ", x29, " + string(dest));
}

void AArch64CodeGenerator::load_value(Variable *variable,const char * dest) {
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
        line("\tldur " + string(dest) + ", " + slot_ref(variable -> get_frame_offset()));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_address(variable,ADDRESS_REG);
        line("\tldr " + string(dest) + ", [" + string(ADDRESS_REG) + "]");
        return;
    }
    line("\tmov " + string(dest) + ", xzr");
}

void AArch64CodeGenerator::load_address(Variable *variable,const char * dest) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        slot_address(dest,variable -> get_frame_offset());
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
    line("\tmov " + string(dest) + ", xzr");
}

void AArch64CodeGenerator::store_value(Variable *variable,const char * src) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tstr " + string(src) + ", " + slot_ref(variable -> get_frame_offset()));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_address(variable,ADDRESS_REG);
        line("\tstr " + string(src) + ", [" + string(ADDRESS_REG) + "]");
    }
}

void AArch64CodeGenerator::load_symbol_address(const string & symbol,const char * dest) {
    line("\tadrp " + string(dest) + ", " + page_ref(symbol));
    line("\tadd " + string(dest) + ", " + string(dest) + ", " + page_off_ref(symbol));
}

void AArch64CodeGenerator::load_format(const char * format,const char * dest) {
    load_symbol_address(string_symbol(format),dest);
}

void AArch64CodeGenerator::emit_static_data() {
    if(!string_contents.empty()){
        if(target.platform == SystemPlatform::MacOS){
            line("\t.section __TEXT,__cstring,cstring_literals");
        }else{
            line("\t.section .rodata");
        }
        for(unsigned long i = 0 ; i < string_contents.size() ; i ++ ){
            string symbol = string_symbol(string_contents[i]);
            if(target.platform == SystemPlatform::MacOS){
                // adrp 需要一个真正的符号，而不是仅在本文件可见的临时标号
                line("\t.private_extern " + symbol);
            }
            line(symbol + ":");
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
    line(target.platform == SystemPlatform::MacOS ? "\t.section __DATA,__data" : "\t.data");
    for(unsigned long i = 0 ; i < globals.size() ; i ++ ){
        Variable * variable = globals[i];
        if(variable -> is_void() || variable -> get_type() == C_STRING || variable -> is_literal_value()){
            continue;
        }
        string symbol = global_symbol(variable);
        if(target.platform == SystemPlatform::MacOS){
            line("\t.private_extern " + symbol);
        }else{
            line("\t.hidden " + symbol);
            line("\t.type " + symbol + ",@object");
        }
        line("\t.p2align 3");
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

string AArch64CodeGenerator::local_label(const string & label) {
    // Mach-O 里以 '.' 开头的标号会被当作外部符号，条件跳转只认 L 开头的局部标号
    if(target.platform == SystemPlatform::MacOS && label.size() > 1 && label[0] == '.'){
        return label.substr(1);
    }
    return label;
}

string AArch64CodeGenerator::function_symbol(const string & name) {
    // Mach-O 的符号带下划线前缀，ELF 保持原名
    return target.platform == SystemPlatform::MacOS ? "_" + name : name;
}

string AArch64CodeGenerator::global_symbol(Variable *variable) {
    // 加前缀避免与运行时库的同名符号冲突
    bool is_macho = target.platform == SystemPlatform::MacOS;
    return (is_macho ? "_slg_" : "slg_") + variable -> get_variable_name();
}

string AArch64CodeGenerator::string_symbol(const string & content) {
    bool is_macho = target.platform == SystemPlatform::MacOS;
    std::map<string,int>::iterator found = string_labels.find(content);
    if(found == string_labels.end()){
        int index = (int)string_contents.size();
        string_contents.push_back(content);
        string_labels[content] = index;
        found = string_labels.find(content);
    }
    return (is_macho ? "_sls_" : "sls_") + to_dec(found -> second);
}

string AArch64CodeGenerator::page_ref(const string & symbol) {
    return target.platform == SystemPlatform::MacOS ? symbol + "@PAGE" : symbol;
}

string AArch64CodeGenerator::page_off_ref(const string & symbol) {
    return target.platform == SystemPlatform::MacOS ? symbol + "@PAGEOFF" : ":lo12:" + symbol;
}

string AArch64CodeGenerator::read_format(Variable *variable) {
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return "%lld";
}

string AArch64CodeGenerator::print_format(Variable *variable) {
    if(variable != NULL && variable -> get_type() == C_STRING){
        return "%s";
    }
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return "%lld";
}

string AArch64CodeGenerator::escape(const string & content) {
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
