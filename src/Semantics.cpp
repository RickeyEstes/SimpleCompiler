#include "Semantics.h"
#include "Logger.h"
#include "SimpleDumper.h"
#include "OptimizedDumper.h"
#include "SpecialDumper.h"

template<class A, class B>
void logConflict(const std::string &identifier, const A &previous, const B &now) {
    Logger::getInstance().error(now.definedAt->getPos(), "conflict identifier `" + identifier + "' here");
    Logger::getInstance().note(previous.definedAt->getPos(), "previous `" + identifier + "' is defined here");
}

template<>
void logConflict<Constant, Variable>(const std::string &identifier, const Constant &previous, const Variable &now) {
    Logger::getInstance().error(now.definedAt->getPos(), "conflict identifier `" + identifier + "' here");
    Logger::getInstance().note(previous.definedAt->getPos(), "previous `" + identifier + "' is defined here");
}

template<class T>
void defVarDesc(T &slot, const ASTNode &node) {
    assert(node.is("VarDesc"));
    auto _parseInt = [&] (const ASTNode &decl) {
        assert(decl.is("VarIntDecl"));
        for(const ASTNode &c : decl) {
            if(c.is("Identifier")) {
                slot.addVariable({c.valIter->str(), c.startIter, VarType::VarIntType, 0});
            } else if(c.is("VarArrayDecl")) {
                slot.addVariable({c.getChildren()[0].valIter->str(), c.startIter,
                    VarType::VarIntArray, c.getChildren()[1].valIter->getVal<UnsignedInfo>().v});
            } else {
                assert(false);
            }
        }
    };
    auto _parseChar = [&] (const ASTNode &decl) {
        assert(decl.is("VarCharDecl"));
        for(const ASTNode &c : decl) {
            if(c.is("Identifier")) {
                slot.addVariable({c.valIter->str(), c.startIter, VarType::VarCharType, 0});
            } else if(c.is("VarArrayDecl")) {
                slot.addVariable({c.getChildren()[0].valIter->str(), c.startIter,
                    VarType::VarCharArray, c.getChildren()[1].valIter->getVal<UnsignedInfo>().v});
            } else {
                assert(false);
            }
        }
    };
    for(const auto &c : node) {
        if(c.is("VarIntDecl"))
            _parseInt(c);
        else if(c.is("VarCharDecl"))
            _parseChar(c);
        else
            assert(false);
    }
}

template<class T>
void defConstDesc(T &slot, const ASTNode &node) {
    assert(node.is("ConstDesc"));
    auto _parseInt = [&] (const ASTNode &decl) {
        assert(decl.is("ConstIntDecl"));
        for(size_t i = 0; i < decl.getChildren().size(); i += 2) {
            slot.addConstant({decl.getChildren()[i].valIter->str(),
                decl.getChildren()[i].startIter, ConstType::ConstIntType,
                decl.getChildren()[i + 1].valIter->getVal<int32_t>()});
        }
    };
    auto _parseChar = [&] (const ASTNode &decl) {
        assert(decl.is("ConstCharDecl"));
        for(size_t i = 0; i < decl.getChildren().size(); i += 2) {
            slot.addConstant({decl.getChildren()[i].valIter->str(),
                decl.getChildren()[i].startIter, ConstType::ConstCharType,
                decl.getChildren()[i + 1].valIter->getVal<char>()});
        }
    };
    for(const auto &c : node) {
        if(c.is("ConstIntDecl"))
            _parseInt(c);
        else if(c.is("ConstCharDecl"))
            _parseChar(c);
        else
            assert(false);
    }
}

Function &Program::addFunction(const ASTNode &p) {
    [&] (const Function &f) {
        checkConflict(f);
        functions.emplace_back(f);
    }(Function({p, *this}));
    funcList[functions.back().identifier] = functions.size() - 1;
    return functions.back();
}

template<>
void Function::parse(SimpleDumper &dumper) {
    if(node.hasChild("parameters")) {
        const auto &params = node.getChild("parameters");
        for(size_t i = 0; i < params.getChildren().size(); i += 2) {
            assert(params[i].is("IntTypename") || params[i].is("CharTypename"));
            VarType type = params[i].is("IntTypename") ? VarType::VarIntType : VarType::VarCharType;
            addParameter({params[i + 1].valIter->str(), params[i + 1].startIter, type, 0});
        }
    }
    const auto &compound = node.getChild("compound");
    if(compound.hasChild("const")) {
        defConstDesc(*this, compound.getChild("const"));
    }
    if(compound.hasChild("var")) {
        defVarDesc(*this, compound.getChild("var"));
    }
    dumper(*this, compound);
}

template<>
void Program::parse(SimpleDumper &dumper) {
    if(node.hasChild("const")) {
        defConstDesc(*this, node.getChild("const"));
    }
    if(node.hasChild("var")) {
        defVarDesc(*this, node.getChild("var"));
    }
    for(const auto &c : node) {
        assert(c.is("ConstDesc") || c.is("VarDesc")
            || c.is("MainFunc") || c.is("VoidFunc")
            || c.is("IntFunc") || c.is("CharFunc"));
        if(!c.is("ConstDesc") && !c.is("VarDesc")) {
            Function &func = addFunction(c);
            func.parse(dumper);
        }
    }
    dumper(*this, node);
}

template<>
void Function::parse(OptimizedDumper &dumper) {
    if(node.hasChild("parameters")) {
        const auto &params = node.getChild("parameters");
        for(size_t i = 0; i < params.getChildren().size(); i += 2) {
            assert(params[i].is("IntTypename") || params[i].is("CharTypename"));
            VarType type = params[i].is("IntTypename") ? VarType::VarIntType : VarType::VarCharType;
            addParameter({params[i + 1].valIter->str(), params[i + 1].startIter, type, 0});
        }
    }
    const auto &compound = node.getChild("compound");
    if(compound.hasChild("const")) {
        defConstDesc(*this, compound.getChild("const"));
    }
    if(compound.hasChild("var")) {
        defVarDesc(*this, compound.getChild("var"));
    }
}

template<>
void Program::parse(OptimizedDumper &dumper) {
    if(node.hasChild("const")) {
        defConstDesc(*this, node.getChild("const"));
    }
    if(node.hasChild("var")) {
        defVarDesc(*this, node.getChild("var"));
    }
    for(const auto &c : node) {
        assert(c.is("ConstDesc") || c.is("VarDesc")
            || c.is("MainFunc") || c.is("VoidFunc")
            || c.is("IntFunc") || c.is("CharFunc"));
        if(!c.is("ConstDesc") && !c.is("VarDesc")) {
            Function &func = addFunction(c);
            func.parse(dumper);
        }
    }
    for(Function &func : functions) {
        const auto &compound = func.node.getChild("compound");
        dumper(func, compound);
    }
    dumper(*this, node);
}

template<>
void Function::parse(SpecialDumper &dumper) {
    if(node.hasChild("parameters")) {
        const auto &params = node.getChild("parameters");
        for(size_t i = 0; i < params.getChildren().size(); i += 2) {
            assert(params[i].is("IntTypename") || params[i].is("CharTypename"));
            VarType type = params[i].is("IntTypename") ? VarType::VarIntType : VarType::VarCharType;
            addParameter({params[i + 1].valIter->str(), params[i + 1].startIter, type, 0});
        }
    }
    const auto &compound = node.getChild("compound");
    if(compound.hasChild("const")) {
        defConstDesc(*this, compound.getChild("const"));
    }
    if(compound.hasChild("var")) {
        defVarDesc(*this, compound.getChild("var"));
    }
}

template<>
void Program::parse(SpecialDumper &dumper) {
    if(node.hasChild("const")) {
        defConstDesc(*this, node.getChild("const"));
    }
    if(node.hasChild("var")) {
        defVarDesc(*this, node.getChild("var"));
    }
    for(const auto &c : node) {
        assert(c.is("ConstDesc") || c.is("VarDesc")
            || c.is("MainFunc") || c.is("VoidFunc")
            || c.is("IntFunc") || c.is("CharFunc"));
        if(!c.is("ConstDesc") && !c.is("VarDesc")) {
            Function &func = addFunction(c);
            func.parse(dumper);
        }
    }
    for(Function &func : functions) {
        const auto &compound = func.node.getChild("compound");
        dumper(func, compound);
    }
    dumper(*this, node);
}

