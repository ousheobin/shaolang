//
// Created by Ou Sheobin on 2019/1/26.
//

#include "compiler.h"
#include <iostream>

Compiler::Compiler(Lexer *lexer_ptr, FileScanner *scanner_ptr) {
    this -> lexer = lexer_ptr;
    this -> scanner = scanner_ptr;
    this -> parser = NULL;
    this -> symbolTable = new SymbolTable();
    this -> irGenerator = new IRGenerator(symbolTable);
    this -> symbolTable -> set_ir_generator(irGenerator);
}

void Compiler::compile() {
    if(parser){
        delete parser;
    }
    parser = new Parser(lexer,symbolTable,irGenerator);
    parser -> do_anlayse();
    if(!CompilerArguments::showIntermediateCode){
        // 编译目标模式下中间代码不打印，优化统一由 emit_assembly 完成
        return;
    }
    symbolTable -> show_all_code();
    symbolTable -> optimize();
    cout << "----- After Optimization: ----"<<endl;
    symbolTable -> show_all_code();
}

void Compiler::emit_assembly(ostream & output,const CompileTarget & target) {
    symbolTable -> optimize();
    if(target.architecture == Architecture::X86){
        X86CodeGenerator generator(symbolTable,output,target);
        generator.generate();
    }else if(target.architecture == Architecture::X64){
        X64CodeGenerator generator(symbolTable,output,target);
        generator.generate();
    }else{
        AArch64CodeGenerator generator(symbolTable,output,target);
        generator.generate();
    }
}

Compiler::~Compiler() {
    if(parser){
        delete parser;
    }
    if(lexer){
        delete lexer;
    }
    if(scanner){
        delete scanner;
    }
    if(irGenerator){
        delete irGenerator;
    }
    if(symbolTable){
        delete symbolTable;
    }
}
