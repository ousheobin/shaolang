//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_SCANNER_H
#define SHAOLANG_SCANNER_H

#include <cstdio>

class FileScanner{

    static const int BUFFER_SIZE = 128;

    // 文件结束哨兵，与 UTF-8 字节不会冲突（UTF-8 中 0xFF 无合法编码）
    static const char EOF_CHAR = (char)-1;

    char    * file_name;
    FILE    * file;

    char    buffer[BUFFER_SIZE];
    int     current_read_length;
    int     current_read_position;

    char    last_char;
    char    current_char;

    int     current_line_number;
    int     current_column_number;

public:

    FileScanner(char * file);
    ~FileScanner();

    char move();

    char * get_file_name();
    char get_current_char();
    int get_current_line();
    int get_current_column();

};

#endif //SHAOLANG_SCANNER_H
