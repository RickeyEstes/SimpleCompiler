#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ASTNode.h"
#include "Dumper.h"

#include <set>
#include <string>
#include <cstdint>
#include <cassert>

#define ALIGN_UP(x) (((x) + (size_t)3) & ~(size_t)3)

template<class A, class B>
void logConflict(const std::string &identifier, const A &previous, const B &now);

enum VarType {
    VarIntType, VarCharType, VarIntArray, VarCharArray, VarIntImm, VarCharImm
};

struct Variable {
    std::string identifier;
    Tokenizer::const_iterator definedAt;
    VarType type;
    uint32_t size;
    size_t space() const {
        switch(type) {
        case VarIntType:
            return 4;
        case VarCharType:
            return 1;
        case VarIntArray:
            return size * (size_t)4;
        case VarCharArray:
            return size;
        default:
            break;
        }
        return 0;
    }
    size_t spaceAligned() const {
        return ALIGN_UP(space());
    }
    std::string getLabel() const {
        return std::string("__var_") + identifier;
    }
};

class VariableList {
protected:
    std::vector<size_t> address;

    size_t addressAlignedNext(size_t index) const {
        return address[index] + variables[index].spaceAligned();
    }

public:
    std::vector<Variable> variables;
    std::map<std::string, size_t> lookup;

    size_t size() const {
        return variables.size();
    }

    size_t space() const {
        if(variables.empty())
            return 0;
        else
            return addressAlignedNext(address.size() - 1);
    }

    void addVariable(const Variable &v) {
        variables.emplace_back(v);
        lookup[v.identifier] = variables.size() - 1;
        if(address.empty())
            address.emplace_back(0);
        else
            address.emplace_back(space());
    }

    bool hasVariable(const std::string &identifier) const {
        return lookup.find(identifier) != lookup.end();
    }

    const Variable &getVariable(const std::string &identifier) const {
        auto iter = lookup.find(identifier);
        assert(iter != lookup.end());
        return variables[iter->second];
    }

    size_t getAddress(const std::string &identifier) const {
        auto iter = lookup.find(identifier);
        assert(iter != lookup.end());
        return address[iter->second];
    }

    size_t getAddress(const size_t &index) const {
        return address[index];
    }

    const Variable &operator[](const size_t &i) const {
        return variables[i];
    }
};

enum ConstType {
    ConstIntType, ConstCharType
};

struct Constant {
    std::string identifier;
    Tokenizer::const_iterator definedAt;
    ConstType type;
    int32_t val;
};

class ConstantList {
protected:
    std::vector<Constant> constants;
    std::map<std::string, size_t> lookup;

public:
    void addConstant(const Constant &v) {
        constants.emplace_back(v);
        lookup[v.identifier] = constants.size() - 1;
    }

    bool hasConstant(const std::string &identifier) const {
        return lookup.find(identifier) != lookup.end();
    }

    const Constant &getConstant(const std::string &identifier) const {
        auto iter = lookup.find(identifier);
        assert(iter != lookup.end());
        return constants[iter->second];
    }
};

class StringLiteral : public Token {
public:
    const char *getLiteral() const {
        return (const char *)val;
    }

    std::string getLabel() const {
        uint64_t uniqueId = (uint64_t)val;
        return std::string("__str_") + std::to_string(uniqueId);
    }
};

class StringLiteralList {
public:
    std::map<std::string, const StringLiteral *> strings;

    std::string addStringLiteral(const Token &t) {
        std::string label = ((const StringLiteral *)&t)->getLabel();
        strings[label] = (const StringLiteral *)&t;
        return label;
    }
};

class Dumper;

class Program;
class Function;

enum LookupResultType {
    TConstant, TFunction, TParameter, TLocalVariable, TGlobalVariable, TNotFound
};

struct LookupResult {
    LookupResultType type;
    union {
        const Constant *c;
        const Variable *v;
        const Function *f;
    } result;
};

class Program {
    friend Function;
    friend Dumper;

public:
    const ASTNode &node;

    ConstantList constList;
    VariableList varList;
    StringLiteralList strList;

    std::vector<Function> functions;
    std::map<std::string, int> funcList;

    Program(const ASTNode &_node): node(_node) {}

    template<class D>
    void parse(D &dumper);

    template<class A>
    void checkConflict(const A &obj) const {
        const std::string &identifier = obj.identifier;
        if(constList.hasConstant(identifier)) {
            logConflict(identifier, constList.getConstant(identifier), obj);
        } else if(varList.hasVariable(identifier)) {
            logConflict(identifier, varList.getVariable(identifier), obj);
        } else if(funcList.find(identifier) != funcList.end()) {
            logConflict(identifier, functions[funcList.find(identifier)->second], obj);
        }
    }

    LookupResult lookup(const std::string &identifier) const {
        if(constList.hasConstant(identifier)) {
            return (LookupResult){.type=LookupResultType::TConstant, .result={.c=&constList.getConstant(identifier)}};
        } else if(varList.hasVariable(identifier)) {
            return (LookupResult){.type=LookupResultType::TGlobalVariable, .result={.v=&varList.getVariable(identifier)}};
        } else if(funcList.find(identifier) != funcList.end()) {
            return (LookupResult){.type=LookupResultType::TFunction, .result={.f=&functions[funcList.find(identifier)->second]}};
        }
        return (LookupResult){.type=LookupResultType::TNotFound, .result={.c=nullptr}};
    }

    void addConstant(const Constant &c) {
        checkConflict(c);
        constList.addConstant(c);
    }

    void addVariable(const Variable &v) {
        checkConflict(v);
        varList.addVariable(v);
    }

    Function &addFunction(const ASTNode &p);

    std::string addStringLiteral(const Token &t) {
        return strList.addStringLiteral(t);
    }
};

class Function {
    friend Dumper;

protected:
    void addParameter(const Variable &v) {
        assert(v.type == VarType::VarIntType || v.type == VarType::VarCharType);
        paramList.addVariable(v);
    }

public:
    const ASTNode &node;
    Program &prog;

    std::string identifier;
    Tokenizer::const_iterator definedAt;

    VariableList paramList;
    VariableList varList;
    ConstantList constList;

    Function(const ASTNode &_node, Program &_prog): node(_node), prog(_prog) {
        identifier = node.getChild("name").valIter->str();
        definedAt = node.getChild("name").valIter;
    }

    template<class D>
    void parse(D &dumper);

    template<class A>
    void checkConflict(const A &obj) const {
        const std::string &identifier = obj.identifier;
        if(paramList.hasVariable(identifier)) {
            logConflict(identifier, paramList.getVariable(identifier), obj);
        } else if(constList.hasConstant(identifier)) {
            logConflict(identifier, constList.getConstant(identifier), obj);
        } else if(varList.hasVariable(identifier)) {
            logConflict(identifier, varList.getVariable(identifier), obj);
        } else if(prog.funcList.find(identifier) != prog.funcList.end()) {
            logConflict(identifier, prog.functions[prog.funcList.find(identifier)->second], obj);
        }
    }

    LookupResult lookup(const std::string &identifier) const {
        if(paramList.hasVariable(identifier)) {
            return (LookupResult){.type=LookupResultType::TParameter, .result={.v=&paramList.getVariable(identifier)}};
        } else if(constList.hasConstant(identifier)) {
            return (LookupResult){.type=LookupResultType::TConstant, .result={.c=&constList.getConstant(identifier)}};
        } else if(varList.hasVariable(identifier)) {
            return (LookupResult){.type=LookupResultType::TLocalVariable, .result={.v=&varList.getVariable(identifier)}};
        }
        return prog.lookup(identifier);
    }

    void addConstant(const Constant &c) {
        checkConflict(c);
        constList.addConstant(c);
    }

    void addVariable(const Variable &v) {
        checkConflict(v);
        varList.addVariable(v);
    }

    std::string addStringLiteral(const ASTNode &t) {
        return prog.addStringLiteral(*t.valIter);
    }

    size_t newTempVariable() {
        static size_t cnt = 0;
        std::string fakeIdentifier = std::string("$tmp") + std::to_string(cnt++);
        varList.addVariable({fakeIdentifier, node.startIter, VarIntType, 0});
        return varList.size() - 1;
    }

    std::string entryLabel() const {
        if(identifier == "main")
            return "main";
        return std::string("__func_") + identifier;
    }

    std::string endLabel() const {
        return std::string("__end_func_") + identifier;
    }

    std::string returnType() const {
        if(node.is("VoidFunc") || node.is("MainFunc")) {
            return "void";
        } else if(node.is("CharFunc")) {
            return "char";
        } else if(node.is("IntFunc")) {
            return "int";
        }
        assert(false);
        return "";
    }
};

#endif // SEMANTICS_H
