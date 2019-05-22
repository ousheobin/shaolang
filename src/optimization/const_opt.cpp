//
// Created by Ou Sheobin on 2019/5/19.
//

#include "const_opt.h"
#include "../dataflow/data_flow_graph.h"
#include "../dataflow/block.h"
#include "../symbols/symbol_table.h"

#include <climits>

#define UNDEF LONG_MAX
#define NAC   LONG_MIN

ConstPropagation::ConstPropagation(DataFlowGraph *dfg, SymbolTable *symbolTable, vector<Variable *> &parameterVars) {
    this -> dataFlowGraph = dfg;
    this -> symbolTable = symbolTable;

    globalVariables = symbolTable -> get_global_variables();
    int index = 0;
    for(unsigned long i = 0 ; i < globalVariables->size() ; i ++ ){
        Variable * variable = globalVariables->at(i);
        variable -> opt_index = index++;
        variables.push_back(variable);
        long value = UNDEF;
        if(!variable->is_basic_type()){
            value = NAC;
        }else if(variable->is_init()){
            value = variable -> get_integer_const_val();
        }
        boundsValueSet.push_back(value);
    }

    for(unsigned long i = 0 ; i < parameterVars.size() ; i ++ ){
        Variable * variable= parameterVars[i];
        variable -> opt_index = index++;
        variables.push_back(variable);
        boundsValueSet.push_back(NAC);
    }

    for(unsigned long i = 0 ; i < dfg->intercode.size() ; i ++ ){
        IntermediateInstruct * instruct = dfg->intercode[i];
        if(instruct->is_declare()){
            Variable * variable = instruct -> getLeftArg();
            variable -> opt_index = index ++;
            variables.push_back(variable);
            long value = UNDEF;
            if(!variable->is_basic_type()){
                value = NAC;
            }else if(variable->is_init()){
                value = variable -> get_integer_const_val();
            }
            boundsValueSet.push_back(value);
        }
    }

    for(int i = 0 ; i < index ; i ++ ){
        initValueSet.push_back(UNDEF);
    }

}

void ConstPropagation::optimize() {
    do_analyse();
    algebra_simplify();
     condition_jump_simplify();
}

void ConstPropagation::do_analyse() {

    dataFlowGraph -> all_blocks [0] -> set_out_vals(boundsValueSet);
    for(unsigned long i = 1 ;i< dataFlowGraph -> all_blocks.size(); i ++ ){
        dataFlowGraph->all_blocks[i]->set_in_vals(initValueSet);
        dataFlowGraph->all_blocks[i]->set_out_vals(initValueSet);
    }
    bool stopIter = false;
    while(!stopIter){
        stopIter=true;
        for(unsigned long i=1;i<dataFlowGraph->all_blocks.size();++i){
            union_blocks( dataFlowGraph -> all_blocks[i] );
            if( transform_block( dataFlowGraph -> all_blocks[i] ) ){
                stopIter = false;
            }
        }
    }

}

/**
 *
 * @param block
 */
void ConstPropagation::union_blocks(Block *block) {
    vector<Block *> prev_blocks = block -> get_prev_block();
    vector<long> in_values = block -> get_in_vals();
    int prev_block_cnt = prev_blocks.size();
    if(prev_block_cnt == 0){
        in_values = initValueSet;
    }else if(prev_block_cnt == 1){
        in_values = prev_blocks.front()->get_out_vals();
    }else{
        for(unsigned long index = 0 ; index < in_values.size() ; index ++ ){
            long val = UNDEF;
            for(unsigned long block_index = 0 ; block_index < prev_block_cnt ; block_index ++ ){
                val = ux(val,prev_blocks[block_index]->get_out_vals()[index]);
            }
            in_values[index] = val;
        }
    }
    block -> set_in_vals(in_values);
}

/**
 * F(x):
 *  NAC ^ X = NAC; UNDEF ^ C = C; C1 ^ C1 = C1 ; C1 ^ C2 = NAC;
 * @param left
 * @param right
 * @return
 */
long ConstPropagation::ux(long left, long right) {
    if(left == NAC || right == NAC ){
        return NAC;
    }else if(left == UNDEF){
        return right;
    }else if(right == UNDEF){
        return left;
    }else if( left == right){
        return left;
    }else{
        return NAC;
    }
}

bool ConstPropagation::transform_block(Block *block) {
    vector<long> inVals = block->get_in_vals();
    vector<long> outVals = block->get_out_vals();
    vector<IntermediateInstruct *> * instructs = block->get_inner_instructs();
    for(unsigned long index = 0 ; index < instructs -> size() ; index ++ ){
        IntermediateInstruct * instruct = instructs->at(index);
        fx(instruct,inVals,outVals);
        inVals = outVals;
    }
    bool has_change=false;
    vector<long> orignal_out_vals = block->get_out_vals();
    for(unsigned long index = 0; index < outVals.size(); index ++ ){
        if(orignal_out_vals[index]!=outVals[index]){
            has_change=true;
            break;
        }
    }
    block->set_out_vals(outVals);
    return has_change;
}

void ConstPropagation::fx(IntermediateInstruct * instruct, vector<long> & in_vals, vector<long> & out_vals) {
    out_vals = in_vals;
    InterCodeOperator op  = instruct -> getInterCodeOperator();
    Variable * storePoint = instruct -> getResultVar();
    Variable * left_arg   = instruct -> getLeftArg();
    Variable * right_arg  = instruct -> getRightArg();
    if(instruct->is_expr()){
        long tmp = NAC;
        if( op == OP_ASSIGN || op == OP_MINUS || op == OP_NOT ){
            if( left_arg -> opt_index == -1 ){
                if(left_arg->is_basic_type()){
                    tmp = left_arg->get_integer_const_val();
                }
            }else{
                tmp=in_vals[left_arg->opt_index];
            }
            if( tmp != UNDEF && tmp != NAC){
                if(op==OP_MINUS){
                    tmp=-tmp;
                } else if(op==OP_NOT){
                    tmp=!tmp;
                }
            }
        } else if( op >= OP_ADD && op <= OP_OR){
            long left_val = UNDEF ,right_val = UNDEF;
            if(left_arg -> opt_index == -1){
                if( left_arg -> is_basic_type()){
                    left_val = left_arg -> get_integer_const_val();
                }
            }else{
                left_val = in_vals[left_arg->opt_index];
            }
            if(right_arg->opt_index==-1){
                if(right_arg->is_basic_type()){
                    right_val = right_arg->get_integer_const_val();
                }
            }else{
                right_val = in_vals[right_arg->opt_index];
            }

            if( left_val == NAC|| right_val == NAC){
                tmp=NAC;
            } else if(left_val == UNDEF || right_val == UNDEF){
                tmp=UNDEF;
            } else{
                if(op==OP_ADD){
                    tmp = left_val + right_val;
                } else if(op==OP_SUB){
                    tmp = left_val - right_val;
                } else if(op==OP_MUL){
                    tmp = left_val * right_val;
                } else if(op==OP_DIV){
                    if(right_val != 0){
                        tmp = left_val / right_val;
                    }else{
                        tmp = NAC;
                    }
                }else if(op==OP_MOD){
                    if(right_val != 0){
                        tmp = left_val % right_val;
                    }else{
                        tmp = NAC;
                    }
                } else if(op==OP_GT){
                    tmp = left_val > right_val;
                } else if(op==OP_GE){
                    tmp = left_val >= right_val;
                } else if(op==OP_LT){
                    tmp = left_val < right_val;
                } else if(op==OP_LE){
                    tmp = left_val <= right_val;
                } else if(op==OP_EQ){
                    tmp = left_val == right_val;
                } else if(op==OP_NEQ){
                    tmp = left_val != right_val;
                } else if(op==OP_AND){
                    tmp = left_val && right_val;
                } else if(op==OP_OR){
                    tmp = left_val || right_val;
                }
            }
        } else if(op==OP_PTR_GET){
            tmp = NAC;
        }
        out_vals[storePoint->opt_index] = tmp;
    }
    else if( op==OP_PTR_SET || (op==OP_ARG && !left_arg->is_basic_type())){
        for(unsigned long i=0;i<out_vals.size(); i ++){
            out_vals[i]=NAC;
        }
    }
    else if(op==OP_PROC){
        for(unsigned long i=0;i<globalVariables->size();++i){
            out_vals[globalVariables->at(i)->opt_index]=NAC;
        }
    }
    else if(op==OP_CALL){//破坏运算call f()
        for(unsigned long i=0;i<globalVariables->size();++i){
            out_vals[globalVariables->at(i)->opt_index]=NAC;
        }
        out_vals[storePoint->opt_index]=NAC;
    }

    instruct->set_in_vals(in_vals);
    instruct->set_out_vals(out_vals);

}

void ConstPropagation::algebra_simplify() {
    for (unsigned long blockIndex = 0; blockIndex < dataFlowGraph -> all_blocks.size(); blockIndex ++) {
        vector<IntermediateInstruct *> * insts = dataFlowGraph->all_blocks[blockIndex]->get_inner_instructs();
        for(unsigned long instIndex = 0 ; instIndex < insts ->size();instIndex ++){
            IntermediateInstruct * instruct = insts -> at(instIndex);
            InterCodeOperator op = instruct ->getInterCodeOperator();
            if(instruct->is_expr()){
                Variable * storePoint = instruct -> getResultVar();
                Variable * left_arg   = instruct -> getLeftArg();
                Variable * right_arg  = instruct -> getRightArg();

                long result = instruct->get_out_vals()[storePoint->opt_index];
                if(result != UNDEF && result != NAC){
                    Variable * newValue = new Variable((int)result);
                    symbolTable->add_variable(newValue);
                    instruct->replace_inst(OP_ASSIGN , storePoint , newValue , NULL);
                } else if(op>=OP_ADD && op<=OP_OR && (op!=OP_ASSIGN && op != OP_MINUS && op != OP_NOT)){
                    long left_val = UNDEF ,right_val = UNDEF;

                    if(left_arg -> opt_index == -1 && left_arg -> is_basic_type()){
                        left_val = left_arg -> get_integer_const_val();
                    }else{
                        left_val = instruct->get_in_vals()[left_arg->opt_index];
                    }
                    if(right_arg->opt_index==-1 && right_arg->is_basic_type()){
                        right_val = right_arg->get_integer_const_val();
                    }else{
                        right_val = instruct->get_in_vals()[right_arg->opt_index];
                    }

                    bool process_left = false, process_right = false;
                    if(left_val != UNDEF && left_val != NAC){
                        process_left  = true;
                    } else if(right_val != UNDEF && right_val != NAC){
                        process_right = true;
                    } else {
                        continue;
                    }

                    Variable * new_left_arg  = NULL;
                    Variable * new_right_arg = NULL;

                    InterCodeOperator new_op = OP_ASSIGN;
                    if(op==OP_ADD){
                        // Y = X + 0 => Y = X
                        if( process_left && left_val == 0){
                            new_left_arg = right_arg;
                        }else if( process_right && right_val == 0){
                            new_left_arg = left_arg;
                        }
                    } else if(op==OP_SUB){
                        if( process_left && left_val == 0){
                            // Y = 0 - X => Y = -X
                            new_op = OP_MINUS;
                            new_left_arg = right_arg;
                        }else if( process_right && right_val == 0){
                            // Y = X - 0 => Y = X
                            new_left_arg = left_arg;
                        }
                    } else if(op==OP_MUL){
                        // Y = X * 0 => Y = 0
                        if( (process_left && left_val == 0 ) || ( process_right && right_val == 0 )){
                            new_left_arg = SymbolTable::zeroVariable;
                        } else if(process_left && left_val == 1 ){
                            new_left_arg = right_arg;
                        } else if(process_right && right_val == 1 ){
                            new_left_arg = left_arg;
                        }
                    } else if(op==OP_DIV){
                        if(process_left && left_val == 0){
                            // Y = 0 / X => Y = 0
                           new_left_arg = SymbolTable::zeroVariable;
                        }else if( process_right && right_val ==1 ){
                            // Y  = X / 1 => Y = X
                            new_left_arg=left_arg;
                        }
                    } else if(op==OP_MOD){
                        if((process_left && left_val == 0) || (process_right && right_val == 1) ){
                            // Y = X % 0 , Y = X % 1 => Y = 0
                            new_left_arg = SymbolTable::zeroVariable;
                        }
                    } else if(op==OP_AND){
                        if(( process_left && left_val == 0 )|| ( process_right && right_val == 0 )){
                            new_left_arg=SymbolTable::zeroVariable;
                        }else if( process_left && left_val != 0){
                            new_op = OP_NEQ;
                            new_left_arg=right_arg;
                            new_right_arg = SymbolTable::zeroVariable;
                        }else if(process_right && right_val!=0){
                            new_op = OP_NEQ;
                            new_left_arg=left_arg;
                            new_right_arg = SymbolTable::zeroVariable;
                        }
                    } else if(op==OP_OR){
                        if( ( process_left && left_val != 0 ) || ( process_right && right_val != 0 )){
                            new_left_arg = SymbolTable::oneVariable;
                        }else if( process_left && left_val == 0 ){
                            new_op = OP_NEQ;
                            new_left_arg = right_arg;
                            new_right_arg = SymbolTable::zeroVariable;
                        }else if( process_right && right_val == 0 ){
                            new_op = OP_NEQ;
                            new_left_arg = left_arg;
                            new_right_arg = SymbolTable::zeroVariable;
                        }
                    }
                    if(new_left_arg != NULL ){
                        instruct -> replace_inst(new_op, storePoint , new_left_arg , new_right_arg );
                    } else{
                        if(process_left){
                            new_left_arg  = new Variable((int)left_val);
                            symbolTable   -> add_variable(new_left_arg);
                            new_right_arg = right_arg;
                        } else if(process_right){
                            new_right_arg = new Variable((int)right_val);
                            symbolTable   -> add_variable(new_right_arg);
                            new_left_arg  = left_arg;
                        }
                        instruct -> replace_inst(op, storePoint , new_left_arg , new_right_arg );
                    }
                }
            } else if( op == OP_ARG || op == OP_RETURN_WITH_VAL) {
                Variable * left_arg = instruct -> getLeftArg();
                if( left_arg -> opt_index != -1 ){
                    long result = instruct ->get_out_vals()[left_arg->opt_index];
                    if( result != UNDEF && result != NAC ){
                        Variable * newVar = new Variable((int)result);
                        symbolTable->add_variable(newVar);
                        instruct->setLeftArg(newVar);
                    }
                }
            }
        }
    }
}

void ConstPropagation::condition_jump_simplify() {
    for (unsigned long blockIndex = 0; blockIndex < dataFlowGraph -> all_blocks.size(); blockIndex ++) {
        vector<IntermediateInstruct *> * insts = dataFlowGraph->all_blocks[blockIndex]->get_inner_instructs();
        for(vector<IntermediateInstruct*>::iterator iter = insts -> begin(), iter_next = iter;
        iter != insts->end() ; iter = iter_next){
            iter_next ++;
            IntermediateInstruct * instruct = * iter;
            InterCodeOperator op = instruct ->getInterCodeOperator();
            if(instruct->is_jmcond()){
                IntermediateInstruct * targetInst = instruct->getJumpTarget();
                Variable * jumpArg = instruct -> getLeftArg();
                long condiction_value = UNDEF;
                if( jumpArg -> opt_index == -1 ){
                    if( jumpArg -> is_basic_type()){
                        condiction_value = jumpArg -> get_integer_const_val();
                    }
                }else{
                    condiction_value = instruct->get_in_vals()[jumpArg->opt_index];
                }
                if(condiction_value == NAC || condiction_value == UNDEF){
                    continue;
                }
                if( op == OP_JMP_TRUE){
                    if(condiction_value == 0){
                        instruct -> get_block() -> get_inner_instructs() ->erase(iter);
                        if( dataFlowGraph -> all_blocks[blockIndex+1] != targetInst -> get_block()){
                            dataFlowGraph -> un_link(instruct->get_block(),targetInst->get_block());
                        }
                    }
                    else{
                        instruct -> optimise_jmp(OP_JMP,targetInst);
                        if( dataFlowGraph -> all_blocks[blockIndex+1] != targetInst -> get_block()){
                            dataFlowGraph -> un_link(instruct->get_block(),dataFlowGraph->all_blocks[blockIndex+1]);
                        }
                    }
                }
                else if(op== OP_JMP_FALSE){
                    if(condiction_value != 0 ){
                        instruct -> get_block() -> get_inner_instructs() ->erase(iter);
                        if( dataFlowGraph -> all_blocks[blockIndex+1] != targetInst -> get_block()){
                            dataFlowGraph -> un_link(instruct->get_block(),targetInst->get_block());
                        }
                    } else{
                        instruct -> optimise_jmp(OP_JMP,targetInst);
                        if( dataFlowGraph -> all_blocks[blockIndex+1] != targetInst -> get_block()){
                            dataFlowGraph -> un_link(instruct->get_block(),dataFlowGraph->all_blocks[blockIndex+1]);
                        }
                    }
                }
            }
        }
    }
}