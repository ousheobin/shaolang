//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_SCANNER_H
#define SHAOLANG_SCANNER_H

#include <cstdio>

class FileScanner{

    static const int BUFFER_SIZE = 128;

    char    * file_name;
    FILE    * file;

    char    buffer[BUFFER_SIZE];
    size_t  current_line_length;
    int     current_read_position;

    char    last_char;
    char    current_char;

    int     current_line_number;
    int     current_column_number;

public:

    FileScanner(char * file);
    ~FileScanner();

    char move();
    char scan_and_move(char target);

    char * get_file_name();
    char get_current_char();
    int get_current_line();
    int get_current_column();

};

#endif //SHAOLANG_SCANNER_H
