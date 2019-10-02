#include "OptimizedDumper.h"

static void toQuadVarDef(const std::string &prefix, const Variable &v, std::ostream &stream) {
    switch(v.type) {
    case VarIntType:
        stream << prefix << " int " << v.identifier << std::endl;
        break;
    case VarCharType:
        stream << prefix << " char " << v.identifier << std::endl;
        break;
    case VarIntArray:
        stream << prefix << " int " << v.identifier << "[" << v.size << "]" << std::endl;
        break;
    case VarCharArray:
        stream << prefix << " char " << v.identifier << "[" << v.size << "]" << std::endl;
        break;
    default:
        break;
    }
}

static void toQuad(const Function &func, const std::vector<std::vector<MC>> &codes,
        const std::vector<std::string> &labels, std::ostream &stream) {
    stream << func.returnType() << " " << func.identifier << "()" << std::endl;
    for(const auto &item : func.paramList.variables) {
        toQuadVarDef("param", item, stream);
    }
    stream << std::endl;
    for(const auto &item : func.varList.variables) {
        toQuadVarDef("var", item, stream);
    }
    stream << std::endl;
    assert(codes.size() == labels.size());
    const std::string indent = "    ";
    for(size_t i = 0; i < codes.size(); ++i) {
        stream << "// block " << i << std::endl;
        if(labels[i].size() > 0) {
            stream << labels[i] << ":" << std::endl;
        }
        for(const MC &c : codes[i]) {
            c.toQuad(stream, indent, func);
        }
        if(codes[i].size() > 0) {
            stream << std::endl;
        }
    }
}

void dumpOptQuad(const OptimizedDumper &dumper, std::ostream &stream) {
    for(const auto &item : dumper.prog->varList.variables) {
        toQuadVarDef("var", item, stream);
    }
    stream << std::endl;
    for(const auto &item : dumper.optQuad) {
        toQuad(*item.first, item.second.codes, item.second.labels, stream);
        stream << std::endl;
    }
}
