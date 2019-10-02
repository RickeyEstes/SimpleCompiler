#ifndef SPECIAL_DUMPER_H
#define SPECIAL_DUMPER_H

#include "Dumper.h"
#include "ASTNode.h"

#include <iostream>
#include <sstream>

struct SpecialDumper : public Dumper {
    std::map<std::string, std::string> text;
    std::stringstream ss;

    template<class T>
    void operator()(T &local, const ASTNode &node);

    void saveCode(std::ostream &s) {
        s << ss.str() << std::endl;
    }
};

#endif // SPECIAL_DUMPER_H
