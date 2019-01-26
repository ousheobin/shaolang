//
// Created by Ou Sheobin on 2019/1/26.
//

#include "error.h"

using namespace std;

FileScanner * Error::scanner = NULL;
int Error::error_amount = 0;
int Error::warning_amount = 0;

string ErrorNames[] = {
        "[Error]",
        "[Waring]"
};

void Error::set_scanner(FileScanner * fileScanner) {
    scanner = fileScanner;
}

void Error::showMessage(ErrorType errorType, std::string message) {
    if(errorType == ErrorType::Error){
        error_amount ++;
    }else{
        warning_amount ++;
    }
    cout<<ErrorNames[errorType]<<" Line "
    <<scanner->get_current_line()<<":"<<scanner->get_current_column()<<" "
    << message <<" ("<<scanner->get_file_name()<<")"<<endl;
}

int Error::get_error_amount() {
    return error_amount;
}

int Error::get_warning_amount() {
    return warning_amount;
}