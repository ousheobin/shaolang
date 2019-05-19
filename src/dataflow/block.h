//
// Created by Ou Sheobin on 2019/5/1.
//

#ifndef SHAOLANG_BLOCK_H
#define SHAOLANG_BLOCK_H

#include <vector>
#include <string>
#include "commons/common.h"

using namespace std;

class Block{
private:
    static int global_block_id;

    int block_id;

    vector<IntermediateInstruct *> inner_instructs;
    vector<Block *> prev;
    vector<Block *> next;

    bool already_visited;
    bool reachable;

    vector<double> in_vals;
    vector<double> out_vals;

public:
    Block(vector<IntermediateInstruct *> & intercodes);
    string to_string();
    vector<IntermediateInstruct *> * get_inner_instructs();

    void add_next_block(Block * block);
    void add_prev_block(Block * block);
};

#endif //SHAOLANG_BLOCK_H
