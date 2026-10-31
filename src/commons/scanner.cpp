//
// Created by Ou Sheobin on 2018/12/21.
//

#include "scanner.h"

FileScanner::FileScanner(char *file_path) {
    file = fopen(file_path,"r");
    if( file == NULL ){
        printf("错误，文件: %s 打开失败，请检查路径及文件名或访问权限。",file_path);
    }
    file_name = file_path;
    current_read_length = 0;
    current_read_position = -1;
    current_line_number = 1;
    current_column_number = 0;

    last_char = '\0';
    current_char = '\0';
}

FileScanner::~FileScanner() {

    if( file != NULL){
        fclose(file);
        file = NULL;
    }

}

int FileScanner::get_current_line() {
    return current_line_number;
}

int FileScanner::get_current_column() {
    return current_column_number + 1;
}

int FileScanner::get_current_char() {
    return current_char;
}

char* FileScanner::get_file_name() {
    return file_name;
}

int FileScanner::move() {

    // 文件读完后句柄已经关闭，此后一律返回结束标记
    if(file == NULL){
        return EOF_CHAR;
    }

    if(current_read_position == current_read_length - 1){
        current_read_length = (int) fread(buffer,1,BUFFER_SIZE,file);
        current_read_position = -1;
        if(current_read_length < 1){
            this -> last_char = current_char;
            this -> current_char = EOF_CHAR;
            fclose(file);
            file = NULL;
            return current_char;
        }
    }

    // 转成无符号再放宽为 int：char 在 ARM64 Linux 上不带符号，真实字节永远撞不到 -1
    int current_scan_ch = (unsigned char) buffer[++ current_read_position];

#ifdef LF
    if(last_char == '\n'){
        current_line_number ++;
        current_column_number = 0;
    }
#elif CRLF
    if(last_char == '\r'){
        current_line_number ++;
        current_column_number = 0;
    }
#endif
    current_column_number ++;
    this->last_char = current_char;
    this->current_char = current_scan_ch;

    return current_char;
}
