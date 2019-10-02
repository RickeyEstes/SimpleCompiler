#include "SourceCode.h"

#include <sstream>

SourceCode::SourceCode(std::istream &stream):
        code(std::istreambuf_iterator<char>(stream), {}) {
    std::stringstream ss(code);
    while(ss.good()) {
        std::string s;
        std::getline(ss, s);
        lines.push_back(s);
    }
}
