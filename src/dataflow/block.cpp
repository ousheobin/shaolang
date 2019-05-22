//
// Created by Ou Sheobin on 2019/5/1.
//

#include "block.h"
#include "../ir/inter_code.h"

#include <sstream>
#include <iostream>

int Block::global_block_id = 0;

Block::Block(vector<IntermediateInstruct *> & input_instructs) {
    already_visited = false;
    reachable = true;
    prev.clear();
    next.clear();
    for(unsigned long index = 0 ; index < input_instructs.size() ; index ++){
        input_instructs[index]->set_block(this);
        inner_instructs.push_back(input_instructs[index]);
    }
    block_id = ( global_block_id ++ );
}

string Block::to_string() {
    std::stringstream ss;
    ss << "---------------------------------" << endl;
    ss << "Block:" << block_id << endl;
    ss << "- Prev:" << endl;
    for(unsigned long i = 0 ; i < prev.size() ; i ++ ){
        if( i != 0 ){
            ss <<" , ";
        }
        ss << prev[i]->block_id;
    }
    ss << endl << "- Next:" << endl;
    for(unsigned long i = 0 ; i < next.size() ; i ++ ){
        if( i != 0 ){
            ss <<" , ";
        }
        ss << next[i]->block_id;
    }
    ss << endl << "- Code:" << endl;
    for(unsigned long i = 0 ; i < inner_instructs.size() ; i ++){
        ss << inner_instructs[i]->display();
    }
    ss << "---------------------------------" << endl;
    return ss.str();
}

vector<IntermediateInstruct *>* Block::get_inner_instructs() {
    return &inner_instructs;
}

void Block::add_next_block(Block *block) {
    next.push_back(block);
}

void Block::add_prev_block(Block *block) {
    prev.push_back(block);
}

vector<Block *>& Block::get_prev_block() {
    return prev;
}

vector<Block *>& Block::get_next_block() {
    return next;
}

void Block::set_in_vals(vector<long> & invals) {
    this->in_vals = invals;
}

vector<long> Block::get_in_vals() {
    return in_vals;
}

void Block::set_out_vals(vector<long> &outvals) {
    this -> out_vals = outvals;
}

vector<long> Block::get_out_vals() {
    return out_vals;
}

bool Block::is_already_visited() {
    return already_visited;
}

void Block::set_already_visited(bool already_visited) {
    this -> already_visited = already_visited;
}

bool Block::is_reachable() {
    return reachable;
}

void Block::set_reachable(bool reachable) {
    this -> reachable = reachable;
}
