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
    current_line_length = 0;
    current_read_position = -1;
    current_line_number = 1;
    current_column_number = 0;

    last_char = '\0';

}

FileScanner::~FileScanner() {

    if( file != NULL){
        fclose(file);
    }

}

int FileScanner::get_current_line() {
    return current_line_number;
}

int FileScanner::get_current_column() {
    return current_column_number;
}

char* FileScanner::get_file_name() {
    return file_name;
}

char FileScanner::next_char() {

    if(file == NULL){
        return -1;
    }

    if(current_read_position == current_line_length - 1){
        current_line_length = fread(buffer,1,BUFFER_SIZE,file);
        if(current_line_length < 1 ){
            current_line_length = -1;
            buffer[0] = -1;
        }
        current_read_position = -1;
    }

    char current_ch = buffer[++ current_read_position];

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
    if(current_ch > 0){
        current_column_number ++;
    }else{
        fclose(file);
        file = NULL;
    }
    last_char = current_ch;

    return current_ch;
}