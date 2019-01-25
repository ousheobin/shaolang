//
// Created by Ou Sheobin on 2018/12/21.
//

#include <vector>

#include "lexer.h"

#define LEXICAL_ERROR(error) cout<<"error"<<endl;

KeyWord Lexer::keyWord;

Lexer::Lexer(FileScanner &sc):scanner(sc) {
    this -> current_char = scanner.move();
}

Lexer::~Lexer() {
    if(!current_token){
        delete current_token;
        current_token = NULL;
    }
}

Token * Lexer::next_token() {
    while (current_char != -1){
        Token * token = NULL;
        while( current_char == ' '  || current_char == '\t' ||
               current_char == '\n' || current_char == '\r' ){
            current_char = scanner.move();
        }
        // 标识符或关键字
        if (check_is_valid_char(current_char)){
            string keywor_or_id = "";
            do{
                keywor_or_id += current_char;
                current_char = scanner.move();
            }while (check_is_valid_char(current_char));

            if(keyWord.contains(keywor_or_id)){
                token = new Token(keyWord.get_type(keywor_or_id));
            }else{
                token = new IDToken(keywor_or_id);
            }

        }
        // 字符串
        else if(current_char == '"'){
            string str = "";
            while (!scanner.scan_and_move('"')){
                current_char = scanner.get_current_char();
                if(current_char == '\\'){
                    current_char = scanner.move();
                    switch (current_char){
                        case '\\':
                            str+='\\'; break;
                        case 'n':
                            str+='\n'; break;
                        case 'r':
                            str+='\r'; break;
                        case '"':
                            str+='"'; break;
                        case '\'':
                            str+='\''; break;
                        case '0':
                            str+='\0'; break;
                        default:
                            str += current_char;
                    }
                }
                if(current_char=='\r'|| current_char=='\n'|| current_char < 0){
                    // [异常] 提前结束，字符串没有闭合
                    token = new Token(LexicalType::ERR);
                    if(current_token){
                        delete token;
                    }
                    current_token = token;
                    return current_token;
                }
            }
            token = new StringToken(str);
        }
        // 单个字符
        else if(current_char == '"'){
            char character;
            current_char = scanner.move();
            if(current_char == '\''){
                current_char = scanner.move();
                switch (current_char){
                    case '\\':
                        character = '\\'; break;
                    case 'n':
                        character = '\n'; break;
                    case 'r':
                        character = '\r'; break;
                    case '"':
                        character = '"'; break;
                    case '\'':
                        character = '\''; break;
                    case '0':
                        character = '\0'; break;
                    default:
                        character = current_char;
                }
            }else if( current_char == '\''){
                // [异常] 字符为空
                token = new Token(LexicalType::ERR);
            }else if(current_char=='\r'|| current_char=='\n'|| current_char < 0){
                // [异常] 提前结束
                token = new Token(LexicalType::ERR);
            }else{
                character = current_char;
            }

            if(!token){
                if(scanner.scan_and_move('\'')){
                    token = new CharToken(character);
                }else{
                    // [异常] 没有右侧单引号
                    token = new Token(ERR);
                }
            }
        }
        // 数字
        else if(check_is_number(current_char)){
            int value = 0;
            do{
                value = value * 10 + (current_char - '0');
                current_char = scanner.move();
            } while (check_is_number(current_char));

            if(current_char == '.'){
                // 浮点数
                double base = 0.1;
                double float_value = 0;
                current_char = scanner.move();
                do{
                    float_value = float_value + (current_char - '0') * base;
                    base /= 10;
                    current_char = scanner.move();
                } while (check_is_number(current_char));

                if(current_char == '.'){
                    // [异常] 重复的小数点
                    token = new Token(LexicalType::ERR);
                }else{
                    token = new FloatToken(value+float_value);
                }

            } else if( current_char == ' '  || current_char == '\t' ||
                     current_char == '\n' || current_char == '\r' ){
                token = new IntegerToken(value);
            } else{
                // 整数
                token = new Token(LexicalType::ERR);
            }
        }
        // 界符
        else{
            switch (current_char){
                case '+':
                    if(scanner.scan_and_move('+')){
                        token = new Token(LexicalType::INC);
                    }else{
                        token = new Token(LexicalType::ADD);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case '-':
                    if(scanner.scan_and_move('-')){
                        token = new Token(LexicalType::DEC);
                    }else{
                        token = new Token(LexicalType::SUB);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case '*':
                    token = new Token(LexicalType::MUL);
                    current_char = scanner.move();
                    break;
                case '/':
                    token = new Token(LexicalType::DIV);
                    current_char = scanner.move();
                    break;
                case '%':
                    token = new Token(LexicalType::MOD);
                    current_char = scanner.move();
                    break;
                case '!':
                    if(scanner.scan_and_move('=')){
                        token = new Token(LexicalType::NEQ);
                    }else{
                        token = new Token(LexicalType::NOT);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case '>':
                    if(scanner.scan_and_move('=')){
                        token = new Token(LexicalType::GE);
                    }else{
                        token = new Token(LexicalType::GT);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case '<':
                    if(scanner.scan_and_move('=')){
                        token = new Token(LexicalType::LE);
                    }else{
                        token = new Token(LexicalType::LT);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case '&':
                    if(scanner.scan_and_move('&')){
                        token = new Token(LexicalType::AND);
                    }else{
                        token = new Token(LexicalType::LEA);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case '=':
                    if(scanner.scan_and_move('=')){
                        token = new Token(LexicalType::EQ);
                    }else{
                        token = new Token(LexicalType::ASSIGN);
                    }
                    current_char = scanner.get_current_char();
                    break;
                case ',':
                    token = new Token(LexicalType::COMMA);
                    current_char = scanner.move();
                    break;
                case ';':
                    token = new Token(LexicalType::SEMICOLON);
                    current_char = scanner.move();
                    break;
                case ':':
                    token = new Token(LexicalType::COLON);
                    current_char = scanner.move();
                    break;
                case '(':
                    token = new Token(LexicalType::L_PARENTHESE);
                    current_char = scanner.move();
                    break;
                case ')':
                    token = new Token(LexicalType::R_PARENTHESE);
                    current_char = scanner.move();
                    break;
                case '[':
                    token = new Token(LexicalType::L_BRACKET);
                    current_char = scanner.move();
                    break;
                case ']':
                    token = new Token(LexicalType::R_BRACKET);
                    current_char = scanner.move();
                    break;
                case '{':
                    token = new Token(LexicalType::L_BRACE);
                    current_char = scanner.move();
                    break;
                case '}':
                    token = new Token(LexicalType::R_BRACE);
                    current_char = scanner.move();
                    break;
                case EOF:
                    break;
                default:
                    // [错误] 词法记号错误
                    token = new Token(LexicalType::ERR);
            }
        }
        if(this->current_token){
            delete this->current_token;
        }
        this -> current_token = token;
        if(token && token -> type != LexicalType::ERR){
            return this -> current_token;
        }else{
            continue;
        }
    }
    if(current_token){
        delete current_token;
    }
    current_token = new Token(LexicalType::END);
    return this -> current_token;
}

bool Lexer::check_is_valid_char(const char ch){
    if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' ){
        return true;
    }
    return false;
}

bool Lexer::check_is_number(const char ch) {
    return ch >= '0' && ch <= '9';
}

