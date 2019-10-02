#include "SourceCode.h"
#include "Tokenizer.h"
#include "Parser.h"
#include "Token.h"
#include "Logger.h"
#include "ASTQuadruple.h"
#include "Semantics.h"
#include "SimpleDumper.h"
#include "OptimizedDumper.h"
#include "SpecialDumper.h"

#include <iostream>
#include <fstream>

#define CHECK_ERROR \
    if(Logger::getInstance().hasError || Logger::getInstance().hasFatal) { \
        Logger::getInstance().output(std::cerr); \
        return -1; \
    }

int main(int argc, const char *argv[]) {
    std::string src_path, o0_quad_path, o0_asm_path, o1_quad_path, o1_asm_path, sp_c_path;
    if(argc <= 1) {
        std::cout << "Source path: ";
        std::cin >> src_path;
        std::cout << "Quadruple code output path: ";
        std::cin >> o0_quad_path;
        std::cout << "MIPS ASM output path: ";
        std::cin >> o0_asm_path;
        std::cout << "Optimized Quadruple code output path: ";
        std::cin >> o1_quad_path;
        std::cout << "Optimized MIPS ASM output path: ";
        std::cin >> o1_asm_path;
    } else if(argc <= 5) {
        std::cerr << "Usage: " << argv[0] << " <source> <quad_output> <asm_output> <opt_quad_output> <opt_asm_output>" << std::endl;
        return 1;
    } else {
        src_path = argv[1];
        o0_quad_path = argv[2];
        o0_asm_path = argv[3];
        o1_quad_path = argv[4];
        o1_asm_path = argv[5];
        if(argc > 6) {
            sp_c_path = argv[6];
        }
    }
    std::ifstream fin(src_path);
    if(fin.fail()) {
        std::cerr << "source " << src_path << " does not exist" << std::endl;
        return -1;
    }

    SourceCode src(fin);
    Tokenizer tokenizer(src);
    CHECK_ERROR;
    Parser parser(tokenizer);
    CHECK_ERROR;

    {
        Program prog(parser.getRoot());
        SimpleDumper dumper;

        prog.parse(dumper);
        CHECK_ERROR;

        std::ofstream fquad(o0_quad_path);
        dumpQuadruple(parser.getRoot(), fquad);
        fquad.close();
        CHECK_ERROR;

        std::ofstream fasm(o0_asm_path);
        dumper.saveCode(fasm);
        fasm.close();
        CHECK_ERROR;
    }
    {
        Program prog(parser.getRoot());
        OptimizedDumper dumper;

        prog.parse(dumper);
        CHECK_ERROR;

        std::ofstream fasm(o1_asm_path);
        dumper.saveCode(fasm);
        fasm.close();
        CHECK_ERROR;

        std::ofstream fquad(o1_quad_path);
        dumpOptQuad(dumper, fquad);
        fquad.close();
        CHECK_ERROR;
    }
    if(!sp_c_path.empty()) {
        Program prog(parser.getRoot());
        SpecialDumper dumper;

        prog.parse(dumper);
        CHECK_ERROR;

        std::ofstream fc(sp_c_path);
        dumper.saveCode(fc);
        fc.close();
        CHECK_ERROR;
    }

    fin.close();
    return 0;
}
