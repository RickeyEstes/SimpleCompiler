#ifndef SIMPLE_DUMPER_H
#define SIMPLE_DUMPER_H

#include "Dumper.h"
#include "ASTNode.h"

#include <iostream>
#include <sstream>

struct SimpleDumper : public Dumper {
    std::map<std::string, std::vector<std::string>> text;
    std::stringstream ss;

    template<class T>
    void operator()(T &local, const ASTNode &node);

    void saveCode(std::ostream &s) {
        s << ss.str() << std::endl;
    }
};

#endif // SIMPLE_DUMPER_H
