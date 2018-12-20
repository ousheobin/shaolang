//
//  shaolang.cpp
//  shaolang
//
//  Created by Ou Sheobin on 2018/12/20.
//

#include <iostream>
#include "shaolang.hpp"

using namespace std;

int main(int argc, char *argv[]){
    if( argc < 2){
        cout << "Error: Nothing is specified. \nUsage: \nshaolang xxx.sheo"<<endl;
    }
    base_path = new string(argv[0]);
    string * target_file = new string(argv[1]);
#ifdef DEBUG
    cout <<"base path:"<< *base_path << endl;
    cout <<"target file:"<< *target_file << endl;
#endif
    return 0;
}
