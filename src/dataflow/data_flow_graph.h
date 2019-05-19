//
// Created by Ou Sheobin on 2019/5/1.
//

#ifndef SHAOLANG_DATA_FLOW_GRAPH_H
#define SHAOLANG_DATA_FLOW_GRAPH_H

#include "commons/common.h"
#include <vector>

using namespace std;

class DataFlowGraph{

private:
    void create_new_blocks();
    void link_blocks();

    void reset_visited();
    void is_reachable(Block * block);
    void delete_all_next_link(Block * block);

public:
    vector<IntermediateInstruct *> intercode;
    vector<Block *> all_blocks;
    vector<IntermediateInstruct *> opt_codes;

    DataFlowGraph(InterCodeCollection & codeCollection);
    ~DataFlowGraph();

    void remove_line(Block * begin,Block * end);

    string to_string();

};

#endif //SHAOLANG_DATA_FLOW_GRAPH_H
