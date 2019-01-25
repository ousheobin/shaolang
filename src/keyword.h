//
// Created by Ou Sheobin on 2018/12/29.
//

#ifndef SHAOLANG_KEYWORD_H
#define SHAOLANG_KEYWORD_H

#include <functional>

#if defined(WIN32) || defined(WIN64)
#include <unordered_map>
using namespace stdext;
#else
#include <unordered_map>
#endif

#include "common.h"


class KeyWord{
private:
    struct str_hash{
        size_t operator()(const string& str) const
        {
#ifdef MAC_OSX
            std::hash<std::string> hash_struct;
            return hash_struct(str.c_str());
#elif WINDOWS
            return stdext::hash(str.c_str());
#else
            return __stl_hash_string(str.c_str());
#endif
        }
    };
    unordered_map<string,LexicalType,str_hash> hashMap;
public:
    KeyWord();
    ~KeyWord();
    LexicalType get_type(string name);
    bool contains(string name);
};

#endif //SHAOLANG_KEYWORD_H
