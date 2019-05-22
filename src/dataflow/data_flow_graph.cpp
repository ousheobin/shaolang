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

    for (unsigned int i = 0; i < all_blocks.size()-1; ++i){
        IntermediateInstruct * last=all_blocks[i]->get_inner_instructs()->back();
        if(!last->is_jmp()){
            all_blocks[i]->get_next_block().push_back(all_blocks[i+1]);
        }
    }
    for (unsigned int i = 1; i < all_blocks.size(); ++i){
        IntermediateInstruct*last=all_blocks[i-1]->get_inner_instructs()->back();
        if(!last->is_jmp()) {
            all_blocks[i]->get_prev_block().push_back(all_blocks[i - 1]);
        }
    }

    for( unsigned long index = 0 ; index < all_blocks.size() ; index ++ ) {
        IntermediateInstruct *last = all_blocks[index]->get_inner_instructs()->back();
        if(last->is_jmp_or_jmcond()){
            all_blocks[index] -> add_next_block(last -> getJumpTarget()->get_block());
            last -> getJumpTarget()->get_block()->add_prev_block(all_blocks[index]);
        }
    }

}

void DataFlowGraph::reset_visited() {
    for(vector<Block *>::iterator iter = all_blocks.begin() ; iter != all_blocks.end() ; iter ++){
        Block * block = * iter;
        block -> set_already_visited(false);
    }
}

bool DataFlowGraph::is_reachable(Block *block) {
    if(block == all_blocks[0]){
        return true;
    }
    if(block -> is_already_visited()){
        return false;
    }
    block -> set_already_visited(true);
    bool reachable = false;
    for(vector<Block *>::iterator iter = block->get_prev_block().begin() ;
        iter != block->get_prev_block().end() ; iter ++){
        Block * testBlock = * iter;
        if(is_reachable(testBlock)){
            reachable = true;
            break;
        }
    }
    return reachable;
}

void DataFlowGraph::delete_all_next_link(Block *block) {

    if(is_reachable(block)) {
        return;
    }
    vector<Block*> delList;
    for(vector<Block*>::iterator i=block->next.begin();i!=block->next.end();++i){
        delList.push_back(*i);
    }
    for(vector<Block*>::iterator i=delList.begin();i!=delList.end();++i){
        block->next.erase(remove(block->next.begin(), block->next.end(), *i),block->next.end());
        (*i)->prev.erase(remove((*i)->prev.begin(), (*i)->prev.end(), *i),(*i)->prev.end());
    }
    for(vector<Block*>::iterator i=delList.begin();i!=delList.end();++i){
        delete_all_next_link(*i);
    }
}

DataFlowGraph::~DataFlowGraph() {

}

void DataFlowGraph::un_link(Block *begin, Block *end) {
    if( begin != NULL ){
        for(vector<Block *>::iterator iter = begin->get_next_block().begin() ;
            iter != begin->get_next_block().end() ; iter ++){
            if(*iter == end){
                begin -> get_next_block().erase(iter);
                break;
            }
        }
        for(vector<Block *>::iterator iter = end->get_prev_block().begin() ;
            iter != end->get_prev_block().end() ; iter ++){
            if(*iter == begin){
                end -> get_prev_block().erase(iter);
                break;
            }
        }
    }
    reset_visited();
    delete_all_next_link(end);
}

string DataFlowGraph::to_string() {
    for(unsigned long index = 0 ; index < all_blocks.size() ; index ++ ){
        std::cout << all_blocks[index]->to_string();
    }
    return string();
}

void DataFlowGraph::write_opt_code(vector<IntermediateInstruct *> & code_tgt) {
    code_tgt.clear();
    for(unsigned long id = 0 ; id < all_blocks.size() ; id ++ ){
        reset_visited();
        if(is_reachable(all_blocks[id])){
            vector<IntermediateInstruct *> * insts = all_blocks[id]->get_inner_instructs();
            for(vector<IntermediateInstruct*>::iterator iter = insts->begin(); iter != insts->end(); iter ++){
                code_tgt.push_back(*iter);
            }
        }
    }
}