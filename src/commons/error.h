//
// Created by Ou Sheobin on 2019/1/26.
//

#include <string>
#include <iostream>

#ifndef SHAOLANG_ERROR_H
#define SHAOLANG_ERROR_H

#include "scanner.h"

using namespace std;

enum ErrorType{
    Error,
    Warning
};

class Error {

private:
    static FileScanner * scanner;
    static int error_amount;
    static int warning_amount;

public:
    static void set_scanner(FileScanner * fileScanner);
    static void showMessage(ErrorType errorType,std::string message);
    static int get_error_amount();
    static int get_warning_amount();
};

#endif //SHAOLANG_ERROR_H
