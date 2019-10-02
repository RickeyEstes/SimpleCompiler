#include "OptimizedDumper.h"

#include <iostream>
#include <functional>
#include <regex>

static std::string escapeSlash(const std::string &s) {
    return regex_replace(s, std::regex("\\\\"), "\\\\");
}

template<>
void OptimizedDumper::operator()(Program &local, const ASTNode &node) {
    prog = &local;

    ss << ".data" << std::endl;
    ss << std::endl;
    for(const auto &c : local.varList.variables) {
        ss << c.getLabel() << ": .space " << c.spaceAligned() << std::endl;
    }
    for(const auto &item : local.strList.strings) {
        ss << item.first << ": .asciiz \"" << escapeSlash(item.second->getLiteral()) << "\"" << std::endl;
    }
    ss << std::endl;

    ss << ".text" << std::endl;
    ss << std::endl;

    // ss << "j " << local.functions[local.functions.size() - 1].entryLabel() << std::endl;
    ss << ".globl " << local.functions[local.functions.size() - 1].entryLabel() << std::endl;
    ss << std::endl;

    for(const Function &f : local.functions) {
        for(const auto &line : text[f.identifier]) {
            ss << line << std::endl;
        }
        ss << std::endl;
    }
}

struct _code_ {
    std::string s;
    _code_(): s() {}
    _code_(std::string _s): s(_s) {}
    _code_ operator<<(const char *t) const {
        return _code_(s + std::string(t));
    }
    _code_ operator<<(const std::string &t) const {
        return _code_(s + t);
    }
    template<class T>
    _code_ operator<<(const T &t) const {
        return _code_(s + std::to_string(t));
    }
};

template<>
void OptimizedDumper::operator()(Function &local, const ASTNode &node) {
    assert(node.is("Compound"));

    toMC(local, node, info[&local].codes, info[&local].labels);
    optimizeMC(local, info[&local].codes, info[&local].labels, *this);
    optToMIPS(local, *this);

    // auto &asmCode = text[local.identifier];
    // asmCode.push_back(local.entryLabel() + ":");

    // auto C = _code_();
    // auto W = [&] (const _code_ &c) {
    //     asmCode.push_back(c.s);
    // };

    // W(C << "__end_func_" << local.entryLabel() << ":");
}
