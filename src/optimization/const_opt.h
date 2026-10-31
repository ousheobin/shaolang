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
    const vector<Variable * > * globalVariables;

    vector<long> boundsValueSet;
    vector<long> initValueSet;

    static bool is_int_literal(Variable * variable);
    static long initial_lattice_value(Variable * variable);
    void register_variable(Variable * variable, long lattice_value, int & index);

    long ux(long left,long right);
    long lattice_value_of(Variable * variable, IntermediateInstruct * instruct, bool use_in_vals);
    void union_blocks(Block*block);

    void fx(IntermediateInstruct * instruct,vector<long>& in_vals,vector<long>& out_vals);
    bool transform_block(Block*block);

    void kill_globals(vector<long> & out_vals);
    static bool passes_address(Function * function);

    void do_analyse();

    void algebra_simplify();
    void condition_jump_simplify();
public:
    ConstPropagation(DataFlowGraph * dfg,SymbolTable * symbolTable,vector<Variable*> & parameterVars);
    void optimize();

};

#endif //SHAOLANG_CONST_OPT_H
