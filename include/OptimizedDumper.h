#ifndef OPTIMIZED_DUMPER_H
#define OPTIMIZED_DUMPER_H

#include "Dumper.h"
#include "ASTNode.h"
#include "Semantics.h"

#include <iostream>
#include <sstream>

enum OP {
    oli, oneg, omov, omovv0, oarg,
    oadd, osub, omul, odiv,
    oloadarr, ostorearr,
    ojmp, obeq, obne, oblt, oble, obgt, obge,
    obeqz, obnez,
    ocall, oret,
    opstr, opint, opchar,
    orint, orchar
};

// struct tbiimm {
//     std::string dst;
//     std::string src; // a
//     int32_t / uint32_t imm; // b (str repr)
// };
// struct tbiop {
//     std::string dst;
//     std::string a;
//     std::string b;
// };
// struct tarr {
//     std::string lab;
//     std::string ind; // a
//     std::string val; // b
// };
// struct tlabel {
//     std::string lab;
// };
// struct tseq {
//     std::vector<std::string> ids;
// };
// struct tret {
//     std::string val; // dst
// }

struct MC {
    OP o;
    std::string lab;
    std::string dst;
    std::string a;
    std::string b;
    void toQuad(std::ostream &stream, const std::string &indent, const Function &func) const;
};

struct OptimizedDumper : public Dumper {
    std::stringstream ss;

    Program *prog;

    struct codeInfo {
        std::vector<std::vector<MC>> codes;
        std::vector<std::string> labels;
    };
    std::map<const Function *, codeInfo> info, optQuad;
    std::map<const Function *, std::map<std::string, std::string>> protectRegs;
    std::map<const Function *, bool> polluteAReg;
    std::map<const Function *, bool> hasCall;
    std::map<const Function *, bool> hasStInter;

    std::map<std::string, std::vector<std::string>> text;

    template<class T>
    void operator()(T &local, const ASTNode &node);

    void saveCode(std::ostream &s) {
        s << ss.str() << std::endl;
    }
};

void toMC(Function &local, const ASTNode &node, std::vector<std::vector<MC>> &codes, std::vector<std::string> &labels);
void dumpOptQuad(const OptimizedDumper &dumper, std::ostream &stream);
void optDAG(const Function &local, std::vector<MC> &codes,
        std::vector<MC> &entities, std::map<std::string, size_t> &ie,
        std::set<std::string> &usage, std::set<std::string> &cover);
void optRestore(const Function &local,
        const std::vector<std::string> &labels,
        std::vector<std::vector<MC>> &entities,
        const std::vector<std::map<std::string, size_t>> &ieMap,
        const std::vector<std::set<std::string>> &usage,
        const std::vector<std::set<std::string>> &cover,
        std::vector<std::set<std::string>> &restore);
void optimizeMC(Function &local, std::vector<std::vector<MC>> &codes,
        const std::vector<std::string> &labels, OptimizedDumper &dumper);
void optAssignReg(Function &local,
        std::vector<std::vector<MC>> &entities,
        const std::vector<std::string> &labels,
        const std::vector<std::set<std::string>> &usage,
        OptimizedDumper &dumper);
std::vector<std::vector<size_t>> optCalcNxt(const std::vector<std::vector<MC>> &entities,
        const std::vector<std::string> &labels);
void optToMIPS(Function &local, OptimizedDumper &dumper);

#endif // OPTIMIZED_DUMPER_H
