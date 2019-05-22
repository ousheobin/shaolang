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

    bool already_visited;
    bool reachable;

    vector<long> in_vals;
    vector<long> out_vals;

public:

    vector<Block *> prev;
    vector<Block *> next;

    Block(vector<IntermediateInstruct *> & intercodes);
    string to_string();
    vector<IntermediateInstruct *> * get_inner_instructs();

    void add_next_block(Block * block);
    void add_prev_block(Block * block);

    vector<Block *> & get_next_block();
    vector<Block *> & get_prev_block();

    void set_in_vals(vector<long> & invals);
    vector<long> get_in_vals();

    void set_out_vals(vector<long> & outvals);
    vector<long> get_out_vals();

    bool is_already_visited();
    void set_already_visited(bool already_visited);

    bool is_reachable();
    void set_reachable(bool reachable);
};

#endif //SHAOLANG_BLOCK_H
