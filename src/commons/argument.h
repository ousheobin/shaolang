//
// Created by Ou Sheobin on 2018/12/21.
//

#ifndef SHAOLANG_ARGUMENT_H
#define SHAOLANG_ARGUMENT_H

class CompilerArguments
{
public:
    static bool showHelp;
    static bool showLexicalResult;
    static bool showDataFlowBlocks;
    static bool showIntermediateCode;
    static bool showCompileSummary;
    static bool emitAssembly;
    static bool generateExecutable;
};

#endif //SHAOLANG_ARGUMENT_H
