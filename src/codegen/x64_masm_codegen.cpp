#include "x64_masm_codegen.h"

#include "commons/error.h"
#include "commons/type_util.h"

#include <sstream>

namespace {

// 表达式暂存寄存器：r10 左值 / r11 右值，结果都落在 r10
const char * LEFT_REG = "r10";
const char * RIGHT_REG = "r11";
// Microsoft x64：前 4 个实参走 rcx/rdx/r8/r9，第 5 个起放栈上，调用点另有 32 字节影子空间
const char * ARGUMENT_REGISTERS[] = { "rcx","rdx","r8","r9" };
const int REGISTER_ARGUMENT_AMOUNT = 4;
const int SHADOW_SPACE = 32;

const int SLOT_SIZE = 8;
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

}

X64MasmCodeGenerator::X64MasmCodeGenerator(SymbolTable *symTable, ostream &output)
        : out(output) {
    this -> symbolTable = symTable;
    this -> frame_size = 0;
    this -> function_seq = 0;
    this -> epilogue_emitted = false;
}

void X64MasmCodeGenerator::line(const string & text) {
    out << text << std::endl;
}

int X64MasmCodeGenerator::align_up(int value,int alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void X64MasmCodeGenerator::generate() {
    line("EXTERN printf:PROC");
    line("EXTERN scanf:PROC");
    line(".code");
    vector<Function *> functions = symbolTable -> get_defined_functions();
    for(unsigned long i = 0 ; i < functions.size() ; i ++ ){
        generate_function(functions[i]);
    }
    emit_static_data();
    line("END");
}

void X64MasmCodeGenerator::generate_function(Function *function) {
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

void X64MasmCodeGenerator::allocate_frame(Function *function, vector<IntermediateInstruct *> &codes) {
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

void X64MasmCodeGenerator::emit_prologue() {
    line("\tpush rbp");
    line("\tmov rbp, rsp");
    if(frame_size > 0){
        line("\tsub rsp, " + to_dec(frame_size));
    }
}

void X64MasmCodeGenerator::emit_arguments(Function *function) {
    vector<Variable *> parameters = function -> get_parameters();
    int amount = (int)parameters.size();
    int register_amount = amount < REGISTER_ARGUMENT_AMOUNT ? amount : REGISTER_ARGUMENT_AMOUNT;
    for(int index = 0 ; index < register_amount ; index ++ ){
        int offset = parameters[index] -> get_frame_offset();
        line("\tmov " + slot_ref(offset) + ", " + string(ARGUMENT_REGISTERS[index]));
    }
    for(int index = REGISTER_ARGUMENT_AMOUNT ; index < amount ; index ++ ){
        int offset = parameters[index] -> get_frame_offset();
        // 寄存器装不下的实参由调用者放栈上：返回地址 8 + 已保存的 rbp 8 + 影子空间
        int source = 16 + SHADOW_SPACE + SLOT_SIZE * (index - REGISTER_ARGUMENT_AMOUNT);
        line("\tmov " + string(RIGHT_REG) + ", " + slot_ref(source));
        line("\tmov " + slot_ref(offset) + ", " + string(RIGHT_REG));
    }
}

void X64MasmCodeGenerator::emit_call(Function *function) {
    int amount = (int)pending_args.size();
    int stack_amount = amount <= REGISTER_ARGUMENT_AMOUNT ? 0 : amount - REGISTER_ARGUMENT_AMOUNT;
    int size = align_up(SHADOW_SPACE + SLOT_SIZE * stack_amount,STACK_ALIGN);
    if(size > 0){
        line("\tsub rsp, " + to_dec(size));
    }
    for(int index = 0 ; index < amount ; index ++ ){
        // pending_args 记录的是中间指令顺序，即源程序实参的逆序
        Variable * argument = pending_args[amount - 1 - index];
        if(index < REGISTER_ARGUMENT_AMOUNT){
            load_value(argument,ARGUMENT_REGISTERS[index]);
        }else{
            load_value(argument,RIGHT_REG);
            line("\tmov " + stack_ref(SHADOW_SPACE + SLOT_SIZE * (index - REGISTER_ARGUMENT_AMOUNT))
                 + ", " + string(RIGHT_REG));
        }
    }
    if(function != NULL){
        line("\tcall " + function_symbol(function -> get_function_name()));
    }
    if(size > 0){
        line("\tadd rsp, " + to_dec(size));
    }
    pending_args.clear();
}

void X64MasmCodeGenerator::emit_epilogue() {
    line("\tmov rsp, rbp");
    line("\tpop rbp");
    line("\tret");
}

void X64MasmCodeGenerator::emit_body(vector<IntermediateInstruct *> &codes) {
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
                emit_two_value_op(instruct,"imul");
                break;
            case OP_DIV:
                emit_div(instruct);
                break;
            case OP_MOD:
                emit_mod(instruct);
                break;
            case OP_MINUS:
                load_value(left,LEFT_REG);
                line("\tneg " + string(LEFT_REG));
                store_value(result,LEFT_REG);
                break;
            case OP_NOT:
                load_value(left,LEFT_REG);
                line("\ttest " + string(LEFT_REG) + ", " + string(LEFT_REG));
                line("\tsete " + string(LEFT_REG) + "b");
                line("\tmovzx " + string(LEFT_REG) + "d, " + string(LEFT_REG) + "b");
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
                load_value(left,LEFT_REG);
                line("\ttest " + string(LEFT_REG) + ", " + string(LEFT_REG));
                line("\tjne " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_FALSE:
                load_value(left,LEFT_REG);
                line("\ttest " + string(LEFT_REG) + ", " + string(LEFT_REG));
                line("\tje " + local_label(instruct -> getJumpTarget() -> getLabel()));
                break;
            case OP_JMP_NEQ:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tcmp " + string(LEFT_REG) + ", " + string(RIGHT_REG));
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
                line("\tmov " + string(LEFT_REG) + ", " + word_at(string(LEFT_REG)));
                store_value(result,LEFT_REG);
                break;
            case OP_PTR_SET:
                load_value(left,LEFT_REG);
                load_value(right,RIGHT_REG);
                line("\tmov " + word_at(string(LEFT_REG)) + ", " + string(RIGHT_REG));
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
                    line("\tmov " + word_at(string(address_reg)) + ", 0");
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

void X64MasmCodeGenerator::emit_two_value_op(IntermediateInstruct *instruct,const char * op) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\t" + string(op) + " " + string(LEFT_REG) + ", " + string(RIGHT_REG));
    store_value(instruct -> getResultVar(),LEFT_REG);
}

void X64MasmCodeGenerator::emit_compare(IntermediateInstruct *instruct,const char * condition) {
    load_value(instruct -> getLeftArg(),LEFT_REG);
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tcmp " + string(LEFT_REG) + ", " + string(RIGHT_REG));
    line("\tset" + string(condition) + " " + string(LEFT_REG) + "b");
    line("\tmovzx " + string(LEFT_REG) + "d, " + string(LEFT_REG) + "b");
    store_value(instruct -> getResultVar(),LEFT_REG);
}

void X64MasmCodeGenerator::emit_logic(IntermediateInstruct *instruct,const char * op) {
    // 与 C 的 && || 语义一致，两个操作数都先归一化成 0 / 1
    load_value(instruct -> getLeftArg(),LEFT_REG);
    line("\ttest " + string(LEFT_REG) + ", " + string(LEFT_REG));
    line("\tsetne " + string(LEFT_REG) + "b");
    line("\tmovzx " + string(LEFT_REG) + "d, " + string(LEFT_REG) + "b");
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\ttest " + string(RIGHT_REG) + ", " + string(RIGHT_REG));
    line("\tsetne " + string(RIGHT_REG) + "b");
    line("\tmovzx " + string(RIGHT_REG) + "d, " + string(RIGHT_REG) + "b");
    line("\t" + string(op) + " " + string(LEFT_REG) + ", " + string(RIGHT_REG));
    store_value(instruct -> getResultVar(),LEFT_REG);
}

void X64MasmCodeGenerator::emit_div(IntermediateInstruct *instruct) {
    // 被除数走 rdx:rax，idiv 只认一个寄存器操作数，除数放 r11
    load_value(instruct -> getLeftArg(),"rax");
    line("\tcqo");
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tidiv " + string(RIGHT_REG));
    store_value(instruct -> getResultVar(),"rax");
}

void X64MasmCodeGenerator::emit_mod(IntermediateInstruct *instruct) {
    load_value(instruct -> getLeftArg(),"rax");
    line("\tcqo");
    load_value(instruct -> getRightArg(),RIGHT_REG);
    line("\tidiv " + string(RIGHT_REG));
    store_value(instruct -> getResultVar(),"rdx");
}

void X64MasmCodeGenerator::emit_vararg_call(const string & format,const char * callee) {
    line("\tsub rsp, " + to_dec(SHADOW_SPACE));
    load_symbol_address(string_symbol(format),ARGUMENT_REGISTERS[0]);
    // 可变参数调用前清零 eax，浮点实参的个数由它告诉被调方
    line("\txor eax, eax");
    // 运行库的符号按原名引用，只有源程序里定义的函数才需要避开助记符
    line("\tcall " + string(callee));
    line("\tadd rsp, " + to_dec(SHADOW_SPACE));
}

X64MasmCodeGenerator::StorageKind X64MasmCodeGenerator::storage_of(Variable *variable) {
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

string X64MasmCodeGenerator::slot_ref(int offset) {
    return "QWORD PTR [rbp" + disp_text(offset) + "]";
}

string X64MasmCodeGenerator::slot_addr(int offset) {
    return "[rbp" + disp_text(offset) + "]";
}

string X64MasmCodeGenerator::stack_ref(int offset) {
    return "QWORD PTR [rsp" + disp_text(offset) + "]";
}

string X64MasmCodeGenerator::word_at(const string & address) {
    return "QWORD PTR [" + address + "]";
}

void X64MasmCodeGenerator::load_immediate(const char * dest,long value) {
    // 字面量是 int32（词法层保证），mov 的 32 位立即数符号扩展就够了
    line("\tmov " + string(dest) + ", " + to_dec((int)value));
}

void X64MasmCodeGenerator::load_value(Variable *variable,const char * dest) {
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
        line("\tmov " + string(dest) + ", " + slot_ref(variable -> get_frame_offset()));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),RIGHT_REG);
        line("\tmov " + string(dest) + ", " + word_at(string(RIGHT_REG)));
        return;
    }
    line("\tmov " + string(dest) + ", 0");
}

void X64MasmCodeGenerator::load_address(Variable *variable,const char * dest) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tlea " + string(dest) + ", " + slot_addr(variable -> get_frame_offset()));
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
    line("\tmov " + string(dest) + ", 0");
}

void X64MasmCodeGenerator::store_value(Variable *variable,const char * src) {
    StorageKind kind = storage_of(variable);
    if(kind == STORAGE_FRAME){
        line("\tmov " + slot_ref(variable -> get_frame_offset()) + ", " + string(src));
        return;
    }
    if(kind == STORAGE_STATIC){
        load_symbol_address(global_symbol(variable),RIGHT_REG);
        line("\tmov " + word_at(string(RIGHT_REG)) + ", " + string(src));
    }
}

void X64MasmCodeGenerator::load_symbol_address(const string & symbol,const char * dest) {
    // x64 取静态数据地址：OFFSET 让链接器给出 64 位绝对地址，需要时由基址重定位修正
    line("\tmov " + string(dest) + ", OFFSET " + symbol);
}

void X64MasmCodeGenerator::emit_static_data() {
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
        line("ALIGN 8");
        int length = variable -> is_arr() ? variable -> get_array_length() : 1;
        // 一个槽就是一个 8 字节单元，数组在静态区里连续占用长度个槽位
        line(global_symbol(variable) + " DQ " + (variable -> is_init()
            ? to_dec((int)variable -> get_int_value())
            : to_dec(length) + " DUP (0)"));
    }
}

string X64MasmCodeGenerator::local_label(const string & label) {
    // 中间标号形如 .L1，MASM 不认开头的点；同名标号在两个函数里会撞，所以带上函数序号
    string name = (!label.empty() && label[0] == '.') ? label.substr(1) : label;
    return "f" + to_dec(function_seq) + "_" + name;
}

string X64MasmCodeGenerator::function_symbol(const string & name) {
    // x64 的 C 符号不带下划线前缀；源程序里的函数名会直接变成 MASM 标号，而 add、sub、
    // cmp 这些助记符在 MASM 里是保留字，做不了标号，入口 main 要留给 CRT，其余统一加前缀
    return name == "main" ? name : "slf_" + name;
}

string X64MasmCodeGenerator::global_symbol(Variable *variable) {
    // 加前缀避免与运行时库的同名符号冲突
    return "slg_" + variable -> get_variable_name();
}

string X64MasmCodeGenerator::string_symbol(const string & content) {
    std::map<string,int>::iterator found = string_labels.find(content);
    if(found == string_labels.end()){
        int index = (int)string_contents.size();
        string_contents.push_back(content);
        string_labels[content] = index;
        found = string_labels.find(content);
    }
    return "sls_" + to_dec(found -> second);
}

string X64MasmCodeGenerator::int_format() {
    // msvcrt 家族只认 %I64d
    return "%I64d";
}

string X64MasmCodeGenerator::read_format(Variable *variable) {
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X64MasmCodeGenerator::print_format(Variable *variable) {
    if(variable != NULL && variable -> get_type() == C_STRING){
        return "%s";
    }
    if(variable != NULL && TypeUtil::canonical_type(variable -> get_type()) == TK_CHAR){
        return "%c";
    }
    return int_format();
}

string X64MasmCodeGenerator::byte_list(const string & content) {
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
