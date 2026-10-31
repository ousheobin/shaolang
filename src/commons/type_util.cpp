//
// Created by Ou Sheobin on 2019/5/1.
//

#include "type_util.h"
#include "symbols/symbol_table.h"

LexicalType TypeUtil::canonical_type(LexicalType type) {
    switch (type){
        case C_INTEGER: return TK_INT;
        case C_CHAR:    return TK_CHAR;
        case C_STRING:  return TK_CHAR;
        default:        return type;
    }
}

bool TypeUtil::type_check(Variable * left,Variable * right) {
    if(left==NULL||right==NULL){
        return false;
    }
    // 索引与解引用单元按存放的内容比较，地址值之间必须元素类型一致，基本值之间允许 int / char 互转
    bool left_address = is_address_value(left);
    bool right_address = is_address_value(right);
    if(left_address != right_address){
        return false;
    }
    if(left_address){
        return canonical_type(left -> get_type()) == canonical_type(right -> get_type());
    }
    return true;
}

bool TypeUtil::is_address_value(Variable * var) {
    // 指针变量、数组名以及存放地址的临时变量；存储单元的值是它所指的内容，不算地址值
    return var != NULL && !var -> is_basic_type() && !var -> is_cell();
}

Variable* TypeUtil::get_offset_step(Variable *var) {
    // 指针加减一个元素等于加减一个槽位的字节数，索引单元和普通的值都按 1 递增
    return is_address_value(var) ? SymbolTable::eightVariable : SymbolTable::oneVariable;
}
