//
// Created by Ou Sheobin on 2019/5/1.
//

#include "data_flow_graph.h"
#include "block.h"
#include "ir/inter_code_collection.h"

#include <iostream>

DataFlowGraph::DataFlowGraph(InterCodeCollection &codeCollection) {
    codeCollection.mark_all_first_inst();
    this -> intercode = codeCollection.get_codes();
    create_new_blocks();
    link_blocks();
}

void DataFlowGraph::create_new_blocks() {
    vector<IntermediateInstruct *> tempCodes;
    unsigned long size = intercode.size();
    for(unsigned long index = 0 ; index < size ; index ++ ){
        if(tempCodes.empty()){
            if(intercode[index]->is_first()){
                tempCodes.push_back(intercode[index]);
            }
        }else{
            if(intercode[index]->is_first()){
                all_blocks.push_back(new Block(tempCodes));
                tempCodes.clear();
            }
            tempCodes.push_back(intercode[index]);
        }
    }
    all_blocks.push_back(new Block(tempCodes));
}

void DataFlowGraph::link_blocks() {
    unsigned long size = all_blocks.size();
    for( unsigned long index = 0 ; index < size ; index ++ ){
        IntermediateInstruct * last = all_blocks[index]->get_inner_instructs()->back();
        if(!last->is_jmp()){
            if(index < size - 1){
                all_blocks[index] -> add_next_block(all_blocks[index+1]);
            }
            if( index > 0 ){
                all_blocks[index] -> add_prev_block(all_blocks[index-1]);
            }
        }
    }

    for( unsigned long index = 0 ; index < all_blocks.size() ; index ++ ) {
        IntermediateInstruct *last = all_blocks[index]->get_inner_instructs()->back();
        if(last->is_jmp_or_jmcond() && last -> getJumpTarget() != NULL){
            all_blocks[index] -> add_next_block(last -> getJumpTarget()->get_block());
            last -> getJumpTarget()->get_block()->add_prev_block(all_blocks[index]);
        }
    }


}

void DataFlowGraph::reset_visited() {

}

void DataFlowGraph::is_reachable(Block *block) {

}

void DataFlowGraph::delete_all_next_link(Block *block) {

}

DataFlowGraph::~DataFlowGraph() {

}

void DataFlowGraph::remove_line(Block *begin, Block *end) {

}

string DataFlowGraph::to_string() {
    for(unsigned long index = 0 ; index < all_blocks.size() ; index ++ ){
        std::cout << all_blocks[index]->to_string();
    }
    return string();
}
