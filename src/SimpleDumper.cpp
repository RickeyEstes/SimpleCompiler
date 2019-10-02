#include "SimpleDumper.h"
#include "Semantics.h"

#include <iostream>
#include <functional>
#include <regex>

static std::string escapeSlash(const std::string &s) {
    return regex_replace(s, std::regex("\\\\"), "\\\\");
}

template<>
void SimpleDumper::operator()(Program &local, const ASTNode &node) {
    ss << ".data" << std::endl;
    for(const auto &c : local.varList.variables) {
        ss << c.getLabel() << ": .space " << c.spaceAligned() << std::endl;
    }
    for(const auto &item : local.strList.strings) {
        ss << item.first << ": .asciiz \"" << escapeSlash(item.second->getLiteral()) << "\"" << std::endl;
    }
    ss << ".text" << std::endl;
    ss << "j " << local.functions[local.functions.size() - 1].entryLabel() << std::endl;
    for(const Function &f : local.functions) {
        for(const auto &line : text[f.identifier]) {
            ss << line << std::endl;
        }
    }
}

template<>
void SimpleDumper::operator()(Function &local, const ASTNode &node) {
    assert(node.is("Compound"));
    assert(text.find(local.identifier) == text.end());

    auto &asmCode = text[local.identifier];
    asmCode.push_back(local.entryLabel() + ":");

    size_t maxEvalDepth = 0;
    std::function<void(const ASTNode &, size_t)> depthDfs;
    depthDfs = [&] (const ASTNode &node, size_t d) {
        maxEvalDepth = std::max(maxEvalDepth, d);
        for(const auto &c : node) {
            depthDfs(c, d + 1);
        }
    };
    depthDfs(node, 1);

    std::vector<size_t> tmpVariables;
    std::deque<size_t> involkPreserve;
    for(size_t i = 0; i < maxEvalDepth; ++i)
        tmpVariables.push_back(local.newTempVariable());
    for(size_t i = 0; i < 300; ++i)
        involkPreserve.push_back(local.newTempVariable());

    int64_t stackSize = local.paramList.space() + 12 + local.varList.space();

    asmCode.push_back(std::string("addiu $sp, $sp, -") + std::to_string(stackSize));
    asmCode.push_back(std::string("sw $ra, ") + std::to_string(stackSize - local.paramList.space() - 4) + "($sp)");
    asmCode.push_back(std::string("sw $s0, ") + std::to_string(stackSize - local.paramList.space() - 8) + "($sp)");
    asmCode.push_back(std::string("sw $s1, ") + std::to_string(stackSize - local.paramList.space() - 12) + "($sp)");

    auto storeVar = [&] (const std::string &id, const std::string &reg, const ASTNode &pos) {
        auto res = local.lookup(id);
        switch(res.type) {
        case TParameter: {
            const auto v = *res.result.v;
            assert(v.type == VarIntType || v.type == VarCharType);
            std::string ins = v.type == VarIntType ? "sw" : "sb";
            asmCode.push_back(ins + " " + reg + ", " + std::to_string(stackSize - (int64_t)(local.paramList.getAddress(id)) - 4ll) + "($sp)");
            break;
        }
        case TLocalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntType && v.type != VarCharType) {
                Logger::getInstance().error(pos, "symbol is not a plain variable");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            std::string ins = v.type == VarIntType ? "sw" : "sb";
            asmCode.push_back(ins + " " + reg + ", " + std::to_string(stackSize - (int64_t)(local.paramList.space() + 12 + local.varList.getAddress(id)) - 4ll) + "($sp)");
            break;
        }
        case TGlobalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntType && v.type != VarCharType) {
                Logger::getInstance().error(pos, "symbol is not a plain variable");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            std::string ins = v.type == VarIntType ? "sw" : "sb";
            asmCode.push_back(ins + " " + reg + ", " + v.getLabel());
            break;
        }
        case TNotFound:
            Logger::getInstance().error(pos, "undefined symbol");
            break;
        case TFunction:
            Logger::getInstance().error(pos, "symbol is a function, not a plain variable");
            Logger::getInstance().note(res.result.f->definedAt, "defined here");
            break;
        default:
            assert(false);
            break;
        }
    };

    auto loadVar = [&] (const std::string &id, const std::string &reg, const ASTNode &pos) {
        auto res = local.lookup(id);
        switch(res.type) {
        case TParameter: {
            const auto v = *res.result.v;
            assert(v.type == VarIntType || v.type == VarCharType);
            std::string ins = v.type == VarIntType ? "lw" : "lb";
            asmCode.push_back(ins + " " + reg + ", " + std::to_string(stackSize - (int64_t)(local.paramList.getAddress(id)) - 4ll) + "($sp)");
            break;
        }
        case TLocalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntType && v.type != VarCharType) {
                Logger::getInstance().error(pos, "symbol is not a plain variable");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            std::string ins = v.type == VarIntType ? "lw" : "lb";
            asmCode.push_back(ins + " " + reg + ", " + std::to_string(stackSize - (int64_t)(local.paramList.space() + 12 + local.varList.getAddress(id)) - 4ll) + "($sp)");
            break;
        }
        case TGlobalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntType && v.type != VarCharType) {
                Logger::getInstance().error(pos, "symbol is not a plain variable");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            std::string ins = v.type == VarIntType ? "lw" : "lb";
            asmCode.push_back(ins + " " + reg + ", " + v.getLabel());
            break;
        }
        case TNotFound:
            Logger::getInstance().error(pos, "undefined symbol");
            break;
        case TFunction:
            Logger::getInstance().error(pos, "symbol is a function, not a plain variable");
            Logger::getInstance().note(res.result.f->definedAt, "defined here");
            break;
        default:
            assert(false);
            break;
        }
    };

    auto storeArr = [&] (const std::string &id, const std::string &indexReg, const std::string &valReg, const ASTNode &pos) {
        auto res = local.lookup(id);
        switch(res.type) {
        case TParameter:
            Logger::getInstance().error(pos, "symbol is a parameter, not an array");
            Logger::getInstance().note(res.result.v->definedAt, "defined here");
            break;
        case TLocalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntArray && v.type != VarCharArray) {
                Logger::getInstance().error(pos, "symbol is not an array");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            if(v.type == VarIntArray)
                asmCode.push_back(std::string("sll ") + indexReg + ", " + indexReg + ", 2");
            asmCode.push_back(std::string("addu ") + indexReg + ", " + indexReg + ", $sp");
            std::string ins = v.type == VarIntArray ? "sw" : "sb";
            asmCode.push_back(ins + " " + valReg + ", " + std::to_string(stackSize - (int64_t)(local.paramList.space() + 12 + local.varList.getAddress(id)) - v.spaceAligned()) + "(" + indexReg + ")");
            break;
        }
        case TGlobalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntArray && v.type != VarCharArray) {
                Logger::getInstance().error(pos, "symbol is not an array");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            if(v.type == VarIntArray)
                asmCode.push_back(std::string("sll ") + indexReg + ", " + indexReg + ", 2");
            std::string ins = v.type == VarIntArray ? "sw" : "sb";
            asmCode.push_back(ins + " " + valReg + ", " + v.getLabel() + "(" + indexReg + ")");
            break;
        }
        case TNotFound:
            Logger::getInstance().error(pos, "undefined symbol");
            break;
        case TFunction:
            Logger::getInstance().error(pos, "symbol is a function, not a plain variable");
            Logger::getInstance().note(res.result.f->definedAt, "defined here");
            break;
        case TConstant:
            Logger::getInstance().error(pos, "symbol is a constant, not a plain variable");
            Logger::getInstance().note(res.result.c->definedAt, "defined here");
            break;
        default:
            assert(false);
            break;
        }
    };

    auto loadArr = [&] (const std::string &id, const std::string &index, const std::string &val, const ASTNode &pos) {
        auto res = local.lookup(id);
        switch(res.type) {
        case TParameter:
            Logger::getInstance().error(pos, "symbol is a parameter, not an array");
            Logger::getInstance().note(res.result.v->definedAt, "defined here");
            break;
        case TLocalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntArray && v.type != VarCharArray) {
                Logger::getInstance().error(pos, "symbol is not an array");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            loadVar(index, "$t0", pos);
            if(v.type == VarIntArray)
                asmCode.push_back(std::string("sll $t0, $t0, 2"));
            asmCode.push_back(std::string("addu $t0, $t0, $sp"));
            std::string ins = v.type == VarIntArray ? "lw" : "lb";
            asmCode.push_back(ins + " $t1, " + std::to_string(stackSize - (int64_t)(local.paramList.space() + 12 + local.varList.getAddress(id)) - v.spaceAligned()) + "($t0)");
            storeVar(val, "$t1", pos);
            break;
        }
        case TGlobalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntArray && v.type != VarCharArray) {
                Logger::getInstance().error(pos, "symbol is not an array");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            loadVar(index, "$t0", pos);
            if(v.type == VarIntArray)
                asmCode.push_back(std::string("sll $t0, $t0, 2"));
            std::string ins = v.type == VarIntArray ? "lw" : "lb";
            asmCode.push_back(ins + " $t1, " + v.getLabel() + "($t0)");
            storeVar(val, "$t1", pos);
            break;
        }
        case TNotFound:
            Logger::getInstance().error(pos, "undefined symbol");
            break;
        case TFunction:
            Logger::getInstance().error(pos, "symbol is a function, not a plain variable");
            Logger::getInstance().note(res.result.f->definedAt, "defined here");
            break;
        case TConstant:
            Logger::getInstance().error(pos, "symbol is a constant, not a plain variable");
            Logger::getInstance().note(res.result.c->definedAt, "defined here");
            break;
        default:
            assert(false);
            break;
        }
    };

    auto readVar = [&] (const ASTNode &node) {
        const auto id = node.valIter->str();
        auto res = local.lookup(id);
        switch(res.type) {
        case TParameter:
        case TGlobalVariable:
        case TLocalVariable: {
            const auto v = *res.result.v;
            if(v.type != VarIntType && v.type != VarCharType) {
                Logger::getInstance().error(node, "symbol is not a plain variable");
                Logger::getInstance().note(v.definedAt, "defined here");
                return;
            }
            asmCode.push_back(v.type == VarIntType ? "li $v0, 5" : "li $v0, 12");
            asmCode.push_back(std::string("syscall"));
            storeVar(id, "$v0", node);
            break;
        }
        case TNotFound:
            Logger::getInstance().error(node, "undefined symbol");
            break;
        case TFunction:
        case TConstant:
            Logger::getInstance().error(node, "symbol is not a plain variable");
            Logger::getInstance().note(res.result.c->definedAt, "defined here");
            break;
        default:
            assert(false);
            break;
        }
    };

    std::function<const Function *(const ASTNode &, size_t)> involk;
    std::function<std::tuple<std::string, VarType>(const ASTNode &, size_t)> expression, item, factor;

    involk = [&] (const ASTNode &node, size_t depth) -> const Function * {
        const auto name = node.getChild("funcName").valIter->str();
        auto res = local.lookup(name);
        if(res.type == TNotFound) {
            Logger::getInstance().error(node, "undefined symbol");
            return (const Function *)nullptr;
        }
        if(res.type != TFunction) {
            Logger::getInstance().error(node, "only allowed to call functions");
            return (const Function *)nullptr;
        }
        const Function *f = res.result.f;
        if(f->paramList.size() != node.getChildren().size() - 1) {
            Logger::getInstance().error(node, "the number of arguments does not match with that of parameters");
            return (const Function *)nullptr;
        }
        std::vector<std::string> ids;
        for(size_t i = 1; i < node.getChildren().size(); ++i) {
            std::string id;
            VarType type;
            std::tie(id, type) = expression(node[i], depth + 1);
            if(type != f->paramList[i - 1].type) {
                Logger::getInstance().error(node, "the types of arguments does not match with those of parameters");
            }
            auto newId = local.varList[involkPreserve.front()].identifier;
            involkPreserve.pop_front();
            loadVar(id, "$t0", node[i + 1]);
            storeVar(newId, "$t0", node[i + 1]);
            ids.push_back(newId);
        }
        for(size_t i = 0; i < ids.size(); ++i) {
            auto addr = f->paramList.getAddress(f->paramList[i].identifier);
            loadVar(ids[i], "$t0", node[i + 1]);
            assert(((int64_t)addr) == (int64_t)i * 4ll);
            asmCode.push_back(std::string("sw") + " $t0, " + std::to_string(-4ll * (((int64_t)i) + 1ll)) + "($sp)");
        }
        asmCode.push_back(std::string("jal ") + f->entryLabel());
        return f;
    };

    auto printStatement = [&] (const ASTNode &node) {
        if(node.hasChild("string")) {
            std::string s = local.addStringLiteral(node.getChild("string"));
            asmCode.push_back(std::string("la $a0, ") + s);
            asmCode.push_back(std::string("li $v0, 4"));
            asmCode.push_back(std::string("syscall"));
        }
        if(!node.hasChild("expression"))
            return;
        std::string e;
        VarType t;
        std::tie(e, t) = expression(node.getChild("expression"), 0);
        loadVar(e, "$a0", node);
        if(t == VarCharType) {
            asmCode.push_back(std::string("sll $a0, $a0, 24"));
            asmCode.push_back(std::string("sra $a0, $a0, 24"));
            asmCode.push_back(std::string("li $v0, 11"));
            asmCode.push_back(std::string("syscall"));
        } else {
            asmCode.push_back(std::string("li $v0, 1"));
            asmCode.push_back(std::string("syscall"));
        }
    };

    auto scanStatement = [&] (const ASTNode &node) {
        for(const auto &c : node)
            readVar(c);
    };

    factor = [&] (const ASTNode &node, size_t depth) {
        const auto id = local.varList[tmpVariables[depth]].identifier;
        if(node.hasChild("child") && node.getChild("child").is("Expression")) {
            std::string val;
            VarType type;
            std::tie(val, type) = expression(node.getChild("child"), depth + 1);
            loadVar(val, "$t0", node.getChild("child"));
            storeVar(id, "$t0", node);
        } else if(node.hasChild("child") && node.getChild("child").is("InvolkExpression")) {
            const Function *func = involk(node.getChild("child"), depth + 1);
            if(!func)
                return std::make_tuple(id, VarIntType);
            if(func->node.is("VoidFunc")) {
                Logger::getInstance().error(node, "cannot involk void function");
                return std::make_tuple(id, VarIntType);
            }
            if(func->node.is("CharFunc")) {
                asmCode.push_back(std::string("sll $v0, $v0, 24"));
                asmCode.push_back(std::string("sra $v0, $v0, 24"));
            }
            storeVar(id, "$v0", node);
            return std::make_tuple(id, func->node.is("CharFunc") ? VarCharType : VarIntType);
        } else if(node.hasChild("int")) {
            int32_t v = node.getChild("int").valIter->getVal<int32_t>();
            asmCode.push_back(std::string("li $t0, ") + std::to_string(v));
            storeVar(id, "$t0", node);
        } else if(node.hasChild("char")) {
            int32_t v = node.getChild("char").valIter->getVal<char>();
            asmCode.push_back(std::string("li $t0, ") + std::to_string(v));
            storeVar(id, "$t0", node);
            return std::make_tuple(id, VarCharType);
        } else if(node.hasChild("index")) {
            std::string index;
            VarType _;
            std::tie(index, _) = expression(node.getChild("index"), depth + 1);
            const std::string &arr = node.getChild("identifier").valIter->str();
            loadArr(arr, index, id, node);
            const auto res = local.lookup(arr);
            return std::make_tuple(id, (res.type == TGlobalVariable || res.type == TLocalVariable) && (res.result.v->type == VarCharArray) ? VarCharType : VarIntType);
        } else if(node.hasChild("identifier")) {
            const std::string &v = node.getChild("identifier").valIter->str();
            if(local.lookup(v).type == TConstant) {
                asmCode.push_back(std::string("li $t0, ") + std::to_string(local.lookup(v).result.c->val));
                storeVar(id, "$t0", node);
                if(local.lookup(v).result.c->type == ConstCharType) {
                    return std::make_tuple(id, VarCharType);
                }
            } else {
                loadVar(v, "$t0", node);
                storeVar(id, "$t0", node);
                const auto res = local.lookup(v);
                return std::make_tuple(id, (res.type == TGlobalVariable || res.type == TLocalVariable || res.type == TParameter) && (res.result.v->type == VarCharType) ? VarCharType : VarIntType);
            }
        } else {
            assert(false);
        }
        return std::make_tuple(id, VarIntType);
    };

    item = [&] (const ASTNode &node, size_t depth) {
        if(node.getChildren().size() <= 1) {
            return factor(node.getChildren()[0], depth);
        }

        const auto id = local.varList[tmpVariables[depth]].identifier;
        std::string first;
        VarType _;
        std::tie(first, _) = factor(node.getChildren()[0], depth + 1);
        loadVar(first, "$t0", node);
        storeVar(id, "$t0", node);

        for(size_t i = 1; i < node.getChildren().size(); i += 2) {
            std::string rv;
            VarType _;
            std::tie(rv, _) = factor(node.getChildren()[i + 1], depth + 1);
            loadVar(rv, "$t0", node);
            loadVar(id, "$t1", node);
            if(std::string("/") == node.getChildren()[i].valIter->tokenType.indicator) {
                asmCode.push_back(std::string("div $t1, $t1, $t0"));
            } else {
                asmCode.push_back(std::string("mul $t1, $t1, $t0"));
            }
            storeVar(id, "$t1", node);
        }

        return std::make_tuple(id, VarIntType);
    };

    expression = [&] (const ASTNode &node, size_t depth) {
        if(node.getChildren().size() <= 1) {
            return item(node.getChildren()[0], depth);
        }
        const auto id = local.varList[tmpVariables[depth]].identifier;
        auto iter = node.begin();
        if(node.hasChild("sign")) {
            std::string first;
            VarType _;
            std::tie(first, _) = item(node.getChildren()[1], depth + 1);
            loadVar(first, "$t0", node);
            asmCode.push_back(std::string("subu $t0, $0, $t0"));
            storeVar(id, "$t0", node);
            iter += 2;
        } else {
            std::string first;
            VarType _;
            std::tie(first, _) = item(node.getChildren()[0], depth + 1);
            loadVar(first, "$t0", node);
            storeVar(id, "$t0", node);
            iter += 1;
        }
        for(; iter < node.getChildren().end(); iter += 2) {
            std::string rv;
            VarType _;
            std::tie(rv, _) = item(iter[1], depth + 1);
            loadVar(rv, "$t0", node);
            loadVar(id, "$t1", node);
            if(std::string("-") == iter[0].valIter->tokenType.indicator) {
                asmCode.push_back(std::string("subu $t1, $t1, $t0"));
            } else {
                asmCode.push_back(std::string("addu $t1, $t1, $t0"));
            }
            storeVar(id, "$t1", node);
        }
        return std::make_tuple(id, VarIntType);
    };

    auto condition = [&] (const std::string &label, bool rev, const ASTNode &node) {
        static std::map<std::string, std::string> od = {
            {"==", "beq"}, {"!=", "bne"}, {"<", "blt"}, {"<=", "ble"}, {">", "bgt"}, {">=", "bge"}
        };
        static std::map<std::string, std::string> rd = {
            {"==", "bne"}, {"!=", "beq"}, {"<", "bge"}, {"<=", "bgt"}, {">", "ble"}, {">=", "blt"}
        };
        auto &mapping = rev ? rd : od;
        if(node.hasChild("expressionB")) {
            std::string eA;
            VarType tA;
            std::tie(eA, tA) = expression(node.getChild("expressionA"), 0);
            loadVar(eA, "$s0", node.getChild("expressionA"));
            std::string eB;
            VarType tB;
            std::tie(eB, tB) = expression(node.getChild("expressionB"), 0);
            loadVar(eB, "$s1", node.getChild("expressionB"));
            if(tA != tB) {
                Logger::getInstance().error(node, "different types of expressions");
                return;
            }
            std::string ins = mapping[node.getChild("op").valIter->str()];
            asmCode.push_back(ins + " $s0, $s1, " + label);
        } else {
            std::string eA;
            VarType tA;
            std::tie(eA, tA) = expression(node.getChild("expressionA"), 0);
            loadVar(eA, "$s0", node.getChild("expressionA"));
            asmCode.push_back(std::string(rev ? "beqz" : "bnez") + " $s0, " + label);
        }
    };

    auto returnStatement = [&] (const ASTNode &node) {
        if(local.node.is("MainFunc")) {
            asmCode.push_back("j __exit_main");
            return;
        }
        if(!(node.hasChild("expression") ^ local.node.is("VoidFunc"))) {
            Logger::getInstance().error(node, local.node.is("VoidFunc") ?
                "return value in void function" : "return nothing in valued function");
            return;
        }
        if(node.hasChild("expression")) {
            std::string e;
            VarType t;
            std::tie(e, t) = expression(node.getChild("expression"), 0);
            if((t == VarCharType) ^ (local.node.is("CharFunc"))) {
                Logger::getInstance().error(node, "the type of returned value is not matched with definition");
                Logger::getInstance().note(local.definedAt, "function defined here");
                return;
            }
            loadVar(e, "$v0", node);
        }
        asmCode.push_back(std::string("lw $ra, ") + std::to_string(stackSize - local.paramList.space() - 4) + "($sp)");
        asmCode.push_back(std::string("lw $s0, ") + std::to_string(stackSize - local.paramList.space() - 8) + "($sp)");
        asmCode.push_back(std::string("lw $s1, ") + std::to_string(stackSize - local.paramList.space() - 12) + "($sp)");
        asmCode.push_back(std::string("addiu $sp, $sp, ") + std::to_string(stackSize));
        asmCode.push_back(std::string("jr $ra"));
    };

    auto newTempLabel = [&] {
        static size_t cnt = 0;
        return std::string("__label_") + std::to_string(cnt++);
    };

    std::function<void(const ASTNode &)> statement;

    auto dowhileStatement = [&] (const ASTNode &node) {
        auto l = newTempLabel();
        asmCode.push_back(l + ":");
        statement(node.getChild("statement"));
        condition(l, false, node.getChild("condition"));
    };

    auto ifStatement = [&] (const ASTNode &node) {
        if(node.hasChild("statementB")) {
            auto l = newTempLabel(), r = newTempLabel();
            condition(l, true, node.getChild("condition"));
            statement(node.getChild("statementA"));
            asmCode.push_back(std::string("j ") + r);
            asmCode.push_back(l + ":");
            statement(node.getChild("statementB"));
            asmCode.push_back(r + ":");
        } else {
            auto l = newTempLabel();
            condition(l, true, node.getChild("condition"));
            statement(node.getChild("statementA"));
            asmCode.push_back(l + ":");
        }
    };

    auto forStatement = [&] (const ASTNode &node) {
        auto id = node.getChild("idA").valIter->str();
        auto res = local.lookup(id);
        if((res.type != TLocalVariable && res.type != TGlobalVariable && res.type != TParameter) || (res.result.v->type != VarIntType && res.result.v->type != VarCharType)) {
            Logger::getInstance().error(node.getChild("idA"), "need a plain variable here");
            return;
        }
        const auto v = *res.result.v;
        std::string init;
        VarType type;
        std::tie(init, type) = expression(node.getChild("init"), 0);
        if(type != v.type) {
            Logger::getInstance().error(node.getChild("idA"), "unmatched type");
            return;
        }
        loadVar(init, "$t0", node.getChild("init"));
        storeVar(id, "$t0", node.getChild("idA"));
        auto l = newTempLabel(), r = newTempLabel();
        asmCode.push_back(l + ":");
        condition(r, true, node.getChild("condition"));
        statement(node.getChild("statement"));
        uint32_t step = node.getChild("step").valIter->getVal<uint32_t>();
        loadVar(id, "$t0", node.getChild("idC"));
        if(std::string("-") == node.getChild("op").valIter->tokenType.indicator) {
            asmCode.push_back(std::string("subu $t0, $t0, ") + std::to_string(step));
        } else {
            asmCode.push_back(std::string("addu $t0, $t0, ") + std::to_string(step));
        }
        storeVar(id, "$t0", node.getChild("idB"));
        asmCode.push_back(std::string("j ") + l);
        asmCode.push_back(r + ":");
    };

    auto assignmentStatement = [&] (const ASTNode &node) {
        if(node.hasChild("index")) {
            auto id = node.getChild("identifier").valIter->str();
            auto res = local.lookup(id);
            if((res.type != TLocalVariable && res.type != TGlobalVariable) || (res.result.v->type != VarIntArray && res.result.v->type != VarCharArray)) {
                Logger::getInstance().error(node.getChild("identifier"), "need an array here");
                return;
            }
            const auto v = *res.result.v;
            std::string e;
            VarType type;
            std::tie(e, type) = expression(node.getChild("expression"), 0);
            loadVar(e, "$s0", node.getChild("expression"));
            if((type == VarCharType && v.type == VarIntArray) || (type == VarIntType && v.type == VarCharArray)) {
                Logger::getInstance().error(node.getChild("identifier"), "unmatched type");
                return;
            }
            std::string i;
            VarType _;
            std::tie(i, _) = expression(node.getChild("index"), 0);
            loadVar(i, "$s1", node.getChild("index"));
            storeArr(id, "$s1", "$s0", node);
        } else {
            auto id = node.getChild("identifier").valIter->str();
            auto res = local.lookup(id);
            if((res.type != TLocalVariable && res.type != TGlobalVariable && res.type != TParameter) || (res.result.v->type != VarIntType && res.result.v->type != VarCharType)) {
                Logger::getInstance().error(node.getChild("identifier"), "need a plain variable here");
                return;
            }
            const auto v = *res.result.v;
            std::string e;
            VarType type;
            std::tie(e, type) = expression(node.getChild("expression"), 0);
            if(type != v.type) {
                Logger::getInstance().error(node.getChild("identifier"), "unmatched type");
                return;
            }
            loadVar(e, "$t0", node.getChild("expression"));
            storeVar(id, "$t0", node.getChild("identifier"));
        }
    };

    statement = [&] (const ASTNode &node) {
        for(const auto &c : node) {
            if(c.is("Statement")) {
                statement(c);
            } else if(c.is("AssignmentStatement")) {
                assignmentStatement(c);
            } else if(c.is("IfStatement")) {
                ifStatement(c);
            } else if(c.is("DoWhileStatement")) {
                dowhileStatement(c);
            } else if(c.is("ForStatement")) {
                forStatement(c);
            } else if(c.is("ReturnStatement")) {
                returnStatement(c);
            } else if(c.is("PrintStatement")) {
                printStatement(c);
            } else if(c.is("ScanStatement")) {
                scanStatement(c);
            } else if(c.is("InvolkStatement")) {
                involk(c, 0);
            }
        }
    };

    for(const auto &c : node) {
        if(c.is("Statement")) {
            statement(c);
        }
    }

    if(!local.node.is("MainFunc")) {
        asmCode.push_back(std::string("lw $ra, ") + std::to_string(stackSize - local.paramList.space() - 4) + "($sp)");
        asmCode.push_back(std::string("lw $s0, ") + std::to_string(stackSize - local.paramList.space() - 8) + "($sp)");
        asmCode.push_back(std::string("lw $s1, ") + std::to_string(stackSize - local.paramList.space() - 12) + "($sp)");
        asmCode.push_back(std::string("addiu $sp, $sp, ") + std::to_string(stackSize));
        asmCode.push_back(std::string("jr $ra"));
    } else {
        asmCode.push_back("__exit_main:");
    }
}
