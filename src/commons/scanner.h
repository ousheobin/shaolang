//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_SCANNER_H
#define SHAOLANG_SCANNER_H

#include <cstdio>

class FileScanner{

    static const int BUFFER_SIZE = 128;

    // 读到的字节按无符号取值，范围 0~255，因此 -1 可以安全地表示文件结束
    static const int EOF_CHAR = -1;

    char    * file_name;
    FILE    * file;

    char    buffer[BUFFER_SIZE];
    int     current_read_length;
    int     current_read_position;

    int     last_char;
    int     current_char;

    int     current_line_number;
    int     current_column_number;

public:

    FileScanner(char * file);
    ~FileScanner();

    int move();

    char * get_file_name();
    int get_current_char();
    int get_current_line();
    int get_current_column();

};

#endif //SHAOLANG_SCANNER_H
