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

char FileScanner::get_current_char() {
    return current_char;
}

char* FileScanner::get_file_name() {
    return file_name;
}

char FileScanner::move() {

    if(file == NULL){
        return EOF_CHAR;
    }

    if(current_read_position == current_read_length - 1){
        current_read_length = (int) fread(buffer,1,BUFFER_SIZE,file);
        if(current_read_length < 1 ){
            current_read_length = -1;
            buffer[0] = EOF_CHAR;
        }
        current_read_position = -1;
    }

    char current_scan_ch = buffer[++ current_read_position];

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
    // 只有读完文件才关闭句柄；非 ASCII 字节（如 UTF-8 注释）只是普通字符
    if(current_scan_ch != EOF_CHAR){
        current_column_number ++;
    }else{
        fclose(file);
        file = NULL;
    }
    this->last_char = current_char;
    this->current_char = current_scan_ch;

    return current_char;
}
