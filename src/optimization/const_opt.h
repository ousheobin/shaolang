//
// Created by Ou Sheobin on 2019/5/19.
//

#ifndef SHAOLANG_CONST_OPT_H
#define SHAOLANG_CONST_OPT_H

#include <vector>

#include "../commons/common.h"
#include "../symbols/symbol_table.h"

using namespace std;

class ConstPropagation{

    SymbolTable * symbolTable;
    DataFlowGraph * dataFlowGraph;

    vector<Variable * > variables;
    vector<Variable * > * globalVariables;

    vector<long> boundsValueSet;
    vector<long> initValueSet;

    long ux(long left,long right);
    void union_blocks(Block*block);

    void fx(IntermediateInstruct * instruct,vector<long>& in_vals,vector<long>& out_vals);
    bool transform_block(Block*block);

    void do_analyse();

    void algebra_simplify();
    void condition_jump_simplify();
public:
    ConstPropagation(DataFlowGraph * dfg,SymbolTable * symbolTable,vector<Variable*> & parameterVars);
    void optimize();

};

#endif //SHAOLANG_CONST_OPT_H
