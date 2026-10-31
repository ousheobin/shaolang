//
// Created by Ou Sheobin on 2018/12/21.
//

#include <climits>
#include <sstream>

#include "lexer.h"
#include "commons/error.h"

#define LEXICAL_ERROR(err_msg) Error::showMessage(ErrorType::Error,err_msg);

namespace {

struct KeywordEntry {
    const char * word;
    LexicalType  type;
};

const KeywordEntry KEYWORDS[] = {
        { "char",     TK_CHAR },
        { "int",      TK_INT },
        { "void",     TK_VOID },
        { "if",       K_IF },
        { "else",     K_ELSE },
        { "while",    K_WHILE },
        { "break",    K_BREAK },
        { "continue", K_CONTINUE },
        { "return",   K_RETURN },
        { "input",    K_INPUT },
        { "output",   K_OUTPUT },
};

const int KEYWORD_AMOUNT = (int)(sizeof(KEYWORDS) / sizeof(KEYWORDS[0]));

LexicalType search_keyword(const string & word){
    for(int i = 0 ; i < KEYWORD_AMOUNT ; i ++ ){
        if(word == KEYWORDS[i].word){
            return KEYWORDS[i].type;
        }
    }
    return LexicalType::ID;
}

}

Lexer::Lexer(FileScanner &sc):scanner(sc) {
    this -> current_char = scanner.move();
    this -> current_token = NULL;
    Error::set_scanner(&sc);
}

Lexer::~Lexer() {
    delete current_token;
    current_token = NULL;
}

Token * Lexer::next_token() {
    while(true){
        skip_blanks();
        if(current_char == -1){
            break;
        }

        Token * token = NULL;
        if(check_is_valid_char(current_char)){
            token = read_identifier();
        }else if(current_char == '"'){
            token = read_string();
        }else if(current_char == '\''){
            token = read_char();
        }else if(check_is_number(current_char)){
            token = read_number();
        }else{
            token = read_operator();
        }

        if(token == NULL){
            // 词法错误已经报告，丢弃该记号并从当前位置继续扫描
            continue;
        }

        delete this -> current_token;
        this -> current_token = token;
        return this -> current_token;
    }

    delete current_token;
    current_token = new Token(LexicalType::END);
    return this -> current_token;
}

void Lexer::advance() {
    current_char = scanner.move();
}

void Lexer::skip_blanks() {
    while(current_char != -1 && check_is_blank(current_char)){
        advance();
    }
}

void Lexer::skip_line_comment() {
    // 调用时已消费 '/' '/'，从第二个 '/' 处跳到行尾
    while(current_char != -1 && current_char != '\n' && current_char != '\r'){
        advance();
    }
}

void Lexer::skip_block_comment() {
    // 调用时 current_char 为 '/' 之后的 '*'
    bool closed = false;
    while(current_char != -1){
        if(current_char == '*'){
            advance();
            if(current_char == '/'){
                advance();
                closed = true;
                break;
            }
        }else{
            advance();
        }
    }
    if(!closed){
        LEXICAL_ERROR("注释没有结束，请检查是否缺少 '*/' .");
    }
}

Token * Lexer::read_identifier() {
    string word = "";
    while(check_is_valid_char(current_char) || check_is_number(current_char)){
        word += (char) current_char;
        advance();
    }
    LexicalType keyword_type = search_keyword(word);
    if(keyword_type == LexicalType::ID){
        return new IDToken(word);
    }
    return new Token(keyword_type);
}

Token * Lexer::read_string() {
    string str = "";
    bool closed = false;
    advance();
    while(current_char != -1){
        if(current_char == '"'){
            closed = true;
            break;
        }
        if(current_char == '\n' || current_char == '\r'){
            break;
        }
        if(current_char == '\\'){
            advance();
            switch (current_char){
                case '\\': str += '\\'; break;
                case 'n' : str += '\n'; break;
                case 'r' : str += '\r'; break;
                case 't' : str += '\t'; break;
                case '"' : str += '"' ; break;
                case '\'': str += '\''; break;
                case '0' : str += '\0'; break;
                default  : str += (char) current_char;
            }
        }else{
            str += (char) current_char;
        }
        advance();
    }
    if(!closed){
        LEXICAL_ERROR("字符串没有结束，请检查是否缺少 '\"' .");
        return NULL;
    }
    advance();
    return new StringToken(str);
}

Token * Lexer::read_char() {
    char character = '\0';
    bool valid = true;
    advance();
    if(current_char == '\\'){
        advance();
        switch (current_char){
            case '\\': character = '\\'; break;
            case 'n' : character = '\n'; break;
            case 'r' : character = '\r'; break;
            case 't' : character = '\t'; break;
            case '"' : character = '"' ; break;
            case '\'': character = '\''; break;
            case '0' : character = '\0'; break;
            default  : character = (char) current_char;
        }
    }else if(current_char == '\'' || current_char == '\n' || current_char == '\r'){
        valid = false;
    }else{
        character = (char) current_char;
    }
    if(valid){
        advance();
    }
    if(!valid || current_char != '\''){
        LEXICAL_ERROR("字符常量不正确，请检查单引号是否成对且内容非空.");
        if(current_char == '\''){
            advance();
        }
        return NULL;
    }
    advance();
    return new CharToken(character);
}

Token * Lexer::read_number() {
    long value = 0;
    while(check_is_number(current_char)){
        value = value * 10 + (current_char - '0');
        advance();
    }
    if(check_is_valid_char(current_char)){
        while(check_is_number(current_char) || check_is_valid_char(current_char) || current_char == '.'){
            advance();
        }
        LEXICAL_ERROR("ShaoLang不允许使用数字作为标识符的开头.");
        return NULL;
    }
    if(current_char == '.'){
        while(check_is_number(current_char) || check_is_valid_char(current_char)){
            advance();
        }
        LEXICAL_ERROR("ShaoLang不支持浮点数.");
        return NULL;
    }
    if(value > INT_MAX){
        LEXICAL_ERROR("整型常量超出 int 表示范围.");
        return NULL;
    }
    return new IntegerToken((int) value);
}

Token * Lexer::read_operator() {
    int ch = current_char;
    advance();
    switch (ch){
        case '+':
            if(current_char == '+'){ advance(); return new Token(LexicalType::INC); }
            return new Token(LexicalType::ADD);
        case '-':
            if(current_char == '-'){ advance(); return new Token(LexicalType::DEC); }
            return new Token(LexicalType::SUB);
        case '/':
            // 注释在此识别：'/' 之后的字符已经在 current_char 上
            if(current_char == '/'){
                skip_line_comment();
                return NULL;
            }
            if(current_char == '*'){
                skip_block_comment();
                return NULL;
            }
            return new Token(LexicalType::DIV);
        case '*': return new Token(LexicalType::MUL);
        case '%': return new Token(LexicalType::MOD);
        case '!':
            if(current_char == '='){ advance(); return new Token(LexicalType::NEQ); }
            return new Token(LexicalType::NOT);
        case '>':
            if(current_char == '='){ advance(); return new Token(LexicalType::GE); }
            if(current_char == '>'){ advance(); return new Token(LexicalType::RIGHT_STREAM); }
            return new Token(LexicalType::GT);
        case '<':
            if(current_char == '='){ advance(); return new Token(LexicalType::LE); }
            if(current_char == '<'){ advance(); return new Token(LexicalType::LEFT_STREAM); }
            return new Token(LexicalType::LT);
        case '&':
            if(current_char == '&'){ advance(); return new Token(LexicalType::AND); }
            return new Token(LexicalType::LEA);
        case '|':
            if(current_char == '|'){ advance(); return new Token(LexicalType::OR); }
            return invalid_operator(ch);
        case '=':
            if(current_char == '='){ advance(); return new Token(LexicalType::EQ); }
            return new Token(LexicalType::ASSIGN);
        case ',': return new Token(LexicalType::COMMA);
        case ';': return new Token(LexicalType::SEMICOLON);
        case ':': return new Token(LexicalType::COLON);
        case '(': return new Token(LexicalType::L_PARENTHESE);
        case ')': return new Token(LexicalType::R_PARENTHESE);
        case '[': return new Token(LexicalType::L_BRACKET);
        case ']': return new Token(LexicalType::R_BRACKET);
        case '{': return new Token(LexicalType::L_BRACE);
        case '}': return new Token(LexicalType::R_BRACE);
        default:  return invalid_operator(ch);
    }
}

Token * Lexer::invalid_operator(int ch) {
    std::stringstream ss;
    ss << "符号'" << (char) ch << "'不是ShaoLang可用的运算符";
    LEXICAL_ERROR(ss.str());
    return NULL;
}

bool Lexer::check_is_valid_char(const int ch) const{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' ;
}

bool Lexer::check_is_number(const int ch) const {
    return ch >= '0' && ch <= '9';
}

bool Lexer::check_is_blank(const int ch) const {
    return ch == ' '  || ch == '\t' || ch == '\n' || ch == '\r';
}
