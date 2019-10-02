#include "OptimizedDumper.h"

#include <functional>

static std::string tempLab() {
    static size_t cnt = 0;
    return std::string("__label_") + std::to_string(cnt++);
}

void toMC(Function &local, const ASTNode &node, std::vector<std::vector<MC>> &codes, std::vector<std::string> &labels) {
    std::function<const Function *(const ASTNode &)> involk;
    std::function<std::tuple<std::string, VarType>(const ASTNode &, const std::string &)> expression, requireId, item, factor;

    size_t tvCnt = 0;
    auto tempVar = [&] {
        return std::string("tempVar$") + std::to_string(tvCnt++);
    };

    codes.emplace_back(std::vector<MC>());
    labels.emplace_back(local.entryLabel());
    size_t current = 0;

    auto newBlock = [&] (const std::string &label) {
        codes.emplace_back(std::vector<MC>());
        labels.emplace_back(label);
        ++current;
    };

    auto jump = [&] (const std::string &label) {
        codes[current].emplace_back(MC{
            ojmp, label, "", "", ""
        });
    };
    auto ret = [&] (const std::string &id) {
        codes[current].emplace_back(MC{
            oret, "", id, "", ""
        });
    };

    auto arr = [&] (OP o, const std::string &a, const std::string &i, const std::string &v) {
        codes[current].emplace_back(MC{
            o, a, "", i, v
        });
    };
    auto br = [&] (OP o, const std::string &a, const std::string &i, const std::string &v) {
        if(o == obeq && (v == "0" || v == "-0")) {
            arr(obeqz, a, i, "");
            return;
        }
        if(o == obne && (v == "0" || v == "-0")) {
            arr(obnez, a, i, "");
            return;
        }
        arr(o, a, i, v);
    };
    auto si = [&] (OP o, const std::string &a, const std::string &b) {
        if(o == omov && a == b) {
            return;
        }
        codes[current].emplace_back(MC{
            o, "", a, b, ""
        });
    };
    auto bi = [&] (OP o, const std::string &dst, const std::string &a, const std::string &b) {
        assert(!isdigit(a[0]) && a[0] != '-');
        if(o == oadd && (b == "0" || b == "-0")) {
            si(omov, dst, a);
            return;
        }
        if((o == omul || o == odiv) && b == "1") {
            si(omov, dst, a);
            return;
        }
        if((o == omul || o == odiv) && b == "-1") {
            si(oneg, dst, a);
            return;
        }
        if(o == omul && (b == "0" || b == "-0")) {
            si(oli, dst, "0");
            return;
        }
        codes[current].emplace_back(MC{
            o, "", dst, a, b
        });
    };

    involk = [&] (const ASTNode &node) -> const Function * {
        const auto &name = node.getChild("funcName").valIter->str();
        auto res = local.lookup(name);
        const Function *f = res.result.f;
        std::vector<std::string> ids;
        for(size_t i = 1; i < node.getChildren().size(); ++i) {
            auto tid = tempVar();
            std::string id;
            VarType type;
            std::tie(id, type) = requireId(node[i], tid);
            if(id != tid)
                si(omov, tid, id);
            ids.push_back(tid);
        }
        for(size_t i = 0; i < ids.size(); ++i) {
            codes[current].emplace_back(MC{
                oarg, f->identifier, std::to_string(i), ids[i], ""
            });
        }
        codes[current].emplace_back(MC{
            ocall, f->identifier, "", "", ""
        });
        newBlock("");
        return f;
    };

    auto printStatement = [&] (const ASTNode &node) {
        if(node.hasChild("string")) {
            std::string s = local.addStringLiteral(node.getChild("string"));
            codes[current].emplace_back(MC{
                opstr, s, "", "", ""
            });
            newBlock("");
        }
        if(!node.hasChild("expression"))
            return;
        std::string e, tid = tempVar();
        VarType t;
        std::tie(e, t) = expression(node.getChild("expression"), tid);
        if(t == VarCharType || t == VarCharImm) {
            codes[current].emplace_back(MC{
                opchar, "", e, "", ""
            });
        } else {
            codes[current].emplace_back(MC{
                opint, "", e, "", ""
            });
        }
        newBlock("");
    };

    auto scanStatement = [&] (const ASTNode &node) {
        for(const auto &c : node) {
            const auto &id = c.valIter->str();
            auto res = local.lookup(id);
            switch(res.type) {
            case TParameter:
            case TGlobalVariable:
            case TLocalVariable: {
                const auto &v = *res.result.v;
                codes[current].emplace_back(MC{
                    v.type == VarIntType ? orint : orchar, "", id, "", ""
                });
                newBlock("");
                si(omovv0, id, "READ");
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
        }
    };

    factor = [&] (const ASTNode &node, const std::string &id) {
        if(node.hasChild("child") && node.getChild("child").is("Expression")) {
            std::string val;
            VarType type;
            std::tie(val, type) = requireId(node.getChild("child"), id);
            return std::make_tuple(val, VarIntType);
        } else if(node.hasChild("child") && node.getChild("child").is("InvolkExpression")) {
            const Function *func = involk(node.getChild("child"));
            si(omovv0, id, "");
            return std::make_tuple(id, func->node.is("CharFunc") ? VarCharType : VarIntType);
        } else if(node.hasChild("int")) {
            int32_t v = node.getChild("int").valIter->getVal<int32_t>();
            return std::make_tuple(std::to_string(v), VarIntImm);
        } else if(node.hasChild("char")) {
            int32_t v = node.getChild("char").valIter->getVal<char>();
            return std::make_tuple(std::to_string(v), VarCharImm);
        } else if(node.hasChild("index")) {
            std::string index;
            VarType t;
            std::tie(index, t) = requireId(node.getChild("index"), tempVar());
            const std::string &lab = node.getChild("identifier").valIter->str();
            arr(oloadarr, lab, index, id);
            const auto &res = local.lookup(lab);
            auto vt = (res.type == TGlobalVariable || res.type == TLocalVariable) && (res.result.v->type == VarCharArray) ? VarCharType : VarIntType;
            return std::make_tuple(id, vt);
        } else if(node.hasChild("identifier")) {
            const std::string &v = node.getChild("identifier").valIter->str();
            const auto res = local.lookup(v);
            if(res.type == TConstant) {
                if(res.result.c->type == ConstCharType) {
                    return std::make_tuple(std::to_string(res.result.c->val), VarCharImm);
                } else {
                    return std::make_tuple(std::to_string(res.result.c->val), VarIntImm);
                }
            } else if(res.type == TGlobalVariable) {
                si(omov, id, v);
                return std::make_tuple(id, (res.result.v->type == VarCharType) ? VarCharType : VarIntType);
            } else {
                return std::make_tuple(v, (res.type == TGlobalVariable || res.type == TLocalVariable || res.type == TParameter) && (res.result.v->type == VarCharType) ? VarCharType : VarIntType);
            }
        }
        assert(false);
        return std::make_tuple(std::string("?"), VarCharImm);
    };

    item = [&] (const ASTNode &node, const std::string &id) {
        if(node.getChildren().size() <= 1) {
            return factor(node.getChildren()[0], id);
        }

        auto &children = node.getChildren();

        size_t iter = 0;
        int32_t imm = 1;
        std::vector<std::string> cp;
        for(; iter < children.size(); iter += 2) {
            std::string first;
            VarType t;
            std::tie(first, t) = factor(children[iter], tempVar());
            if(t == VarIntImm || t == VarCharImm) {
                int32_t v;
                sscanf(first.c_str(), "%d", &v);
                imm *= v;
            } else {
                cp.push_back(first);
            }
            if(iter + 1 >= children.size() || std::string("/") == children[iter + 1].valIter->tokenType.indicator) {
                ++iter;
                break;
            }
        }
        if(iter >= children.size() && cp.size() <= 0) {
            return std::make_tuple(std::to_string(imm), VarIntImm);
        }
        if(imm == 0)
            return std::make_tuple(std::string("0"), VarIntImm);
        if(imm == 1) {
            if(cp.size() <= 0) {
                if(iter >= children.size())
                    return std::make_tuple(std::string("1"), VarIntImm);
                si(oli, id, "1");
            } else if(cp.size() <= 1) {
                si(omov, id, cp[0]);
            } else if(cp.size() > 1) {
                bi(omul, id, cp[0], cp[1]);
                for(size_t i = 2; i < cp.size(); ++i) {
                    bi(omul, id, id, cp[i]);
                }
            }
        } else {
            if(cp.size() <= 0) {
                if(iter >= children.size())
                    return std::make_tuple(std::to_string(imm), VarIntImm);
                si(oli, id, std::to_string(imm));
            } else if(cp.size() <= 1) {
                bi(omul, id, cp[0], std::to_string(imm));
            } else if(cp.size() > 1) {
                bi(omul, id, cp[0], cp[1]);
                for(size_t i = 2; i < cp.size(); ++i) {
                    bi(omul, id, id, cp[i]);
                }
                bi(omul, id, id, std::to_string(imm));
            }
        }

        for(size_t i = iter; i < children.size(); i += 2) {
            std::string rv;
            VarType t;
            std::tie(rv, t) = factor(children[i + 1], tempVar());
            if(std::string("/") == children[i].valIter->tokenType.indicator) {
                bi(odiv, id, id, rv);
            } else {
                bi(omul, id, id, rv);
            }
        }

        return std::make_tuple(id, VarIntType);
    };

    expression = [&] (const ASTNode &node, const std::string &id) {
        if(node.getChildren().size() <= 1) {
            return item(node.getChildren()[0], id);
        }
        auto iter = node.begin();
        int32_t imm = 0;
        std::vector<std::string> cp;
        std::vector<bool> ng;
        if(node.hasChild("sign")) {
            std::string first;
            VarType t;
            std::tie(first, t) = item(node.getChildren()[1], tempVar());
            if(t == VarIntImm || t == VarCharImm) {
                int32_t v;
                sscanf(first.c_str(), "%d", &v);
                imm -= v;
            } else {
                cp.push_back(first);
                ng.push_back(true);
            }
            iter += 2;
        } else {
            std::string first;
            VarType t;
            std::tie(first, t) = item(node.getChildren()[0], tempVar());
            if(t == VarIntImm || t == VarCharImm) {
                int32_t v;
                sscanf(first.c_str(), "%d", &v);
                imm += v;
            } else {
                cp.push_back(first);
                ng.push_back(false);
            }
            iter += 1;
        }
        for(; iter < node.getChildren().end(); iter += 2) {
            std::string rv;
            VarType t;
            std::tie(rv, t) = item(iter[1], tempVar());

            if(t == VarIntImm || t == VarCharImm) {
                int32_t v;
                sscanf(rv.c_str(), "%d", &v);
                if(std::string("-") == iter[0].valIter->tokenType.indicator) {
                    imm -= v;
                } else {
                    imm += v;
                }
            } else {
                cp.push_back(rv);
                ng.push_back(std::string("-") == iter[0].valIter->tokenType.indicator);
            }
        }
        if(cp.size() <= 0) {
            return std::make_tuple(std::to_string(imm), VarIntImm);
        } else if(cp.size() <= 1) {
            if(!ng[0] && !imm) {
                return std::make_tuple(cp[0], VarIntType);
            }
            if(ng[0] || id != cp[0])
                si(ng[0] ? oneg : omov, id, cp[0]);
            if(imm)
                bi(oadd, id, id, std::to_string(imm));
        } else {
            size_t i;
            if(ng[0]) {
                si(oneg, id, cp[0]);
                i = 1;
            } else {
                bi(ng[1] ? osub : oadd, id, cp[0], cp[1]);
                i = 2;
            }
            for(; i < cp.size(); ++i) {
                bi(ng[i] ? osub : oadd, id, id, cp[i]);
            }
            if(imm)
                bi(oadd, id, id, std::to_string(imm));
        }
        return std::make_tuple(id, VarIntType);
    };

    requireId = [&] (const ASTNode &node, const std::string &id) {
        std::string v;
        VarType t;
        std::tie(v, t) = expression(node, id);
        if(t == VarIntImm || t == VarCharImm) {
            si(oli, id, v);
            return std::make_tuple(id, t == VarCharImm ? VarCharType : VarIntType);
        }
        return std::make_tuple(v, t);
    };

    auto condition = [&] (const std::string &label, bool rev, const ASTNode &node) {
        static std::map<std::string, OP> od = {
            {"==", obeq}, {"!=", obne}, {"<", oblt}, {"<=", oble}, {">", obgt}, {">=", obge}
        };
        static std::map<std::string, OP> rd = {
            {"==", obne}, {"!=", obeq}, {"<", obge}, {"<=", obgt}, {">", oble}, {">=", oblt}
        };
        static std::map<std::string, std::string> sw = {
            {"==", "=="}, {"!=", "!="}, {"<", ">"}, {"<=", ">="}, {">", "<"}, {">=", "<="}
        };
        if(node.hasChild("expressionB")) {
            std::string eA;
            VarType tA;
            std::tie(eA, tA) = expression(node.getChild("expressionA"), tempVar());
            std::string eB;
            VarType tB;
            std::tie(eB, tB) = expression(node.getChild("expressionB"), tempVar());
            auto op = node.getChild("op").valIter->str();
            if((tA == VarIntImm || tA == VarCharImm)) {
                std::swap(eA, eB);
                std::swap(tA, tB);
                op = sw[op];
            }
            auto &mapping = rev ? rd : od;
            if((tA == VarIntImm || tA == VarCharImm) && (tB == VarIntImm || tB == VarCharImm)) {
                int32_t va, vb;
                sscanf(eA.c_str(), "%d", &va);
                sscanf(eB.c_str(), "%d", &vb);
                bool res = false;
                OP oop = mapping[op];
                switch(oop) {
                case obeq:
                    res = va == vb;
                    break;
                case obne:
                    res = va != vb;
                    break;
                case oblt:
                    res = va < vb;
                    break;
                case oble:
                    res = va <= vb;
                    break;
                case obgt:
                    res = va > vb;
                    break;
                case obge:
                    res = va >= vb;
                    break;
                default:
                    assert(false);
                    break;
                }
                if(res) {
                    jump(label);
                    newBlock("");
                }
                return;
            }
            OP ins = mapping[op];
            br(ins, label, eA, eB);
            newBlock("");
        } else {
            std::string eA;
            VarType tA;
            std::tie(eA, tA) = requireId(node.getChild("expressionA"), tempVar());
            br(rev ? obeqz : obnez, label, eA, "");
            newBlock("");
        }
    };

    auto returnStatement = [&] (const ASTNode &node) {
        if(local.node.is("MainFunc")) {
            jump(local.endLabel());
            newBlock("");
            return;
        }
        if(node.hasChild("expression")) {
            std::string e;
            VarType t;
            std::tie(e, t) = expression(node.getChild("expression"), tempVar());
            ret(e);
            newBlock("");
        } else {
            ret("");
            newBlock("");
        }
    };

    std::function<void(const ASTNode &)> statement;

    auto dowhileStatement = [&] (const ASTNode &node) {
        auto l = tempLab();
        newBlock(l);
        statement(node.getChild("statement"));
        condition(l, false, node.getChild("condition"));
    };

    auto ifStatement = [&] (const ASTNode &node) {
        if(node.hasChild("statementB")) {
            auto l = tempLab(), r = tempLab();
            condition(l, true, node.getChild("condition"));
            statement(node.getChild("statementA"));
            jump(r);
            newBlock(l);
            statement(node.getChild("statementB"));
            newBlock(r);
        } else {
            auto l = tempLab();
            condition(l, true, node.getChild("condition"));
            statement(node.getChild("statementA"));
            newBlock(l);
        }
    };

    auto forStatement = [&] (const ASTNode &node) {
        auto id = node.getChild("idA").valIter->str();
        auto res = local.lookup(id);
        if((res.type != TLocalVariable && res.type != TGlobalVariable && res.type != TParameter) || (res.result.v->type != VarIntType && res.result.v->type != VarCharType)) {
            Logger::getInstance().error(node.getChild("idA"), "need a plain variable here");
            return;
        }
        const auto &v = *res.result.v;
        std::string init;
        VarType type;
        std::tie(init, type) = requireId(node.getChild("init"), tempVar());
        si(omov, id, init);
        if(type != v.type) {
            Logger::getInstance().error(node.getChild("idA"), "unmatched type");
            return;
        }
        auto l = tempLab(), r = tempLab();
        newBlock(l);
        condition(r, true, node.getChild("condition"));
        statement(node.getChild("statement"));
        int32_t step = (int32_t)node.getChild("step").valIter->getVal<uint32_t>();
        if(std::string("-") == node.getChild("op").valIter->tokenType.indicator)
            step = -step;
        bi(oadd, id, id, std::to_string(step));
        jump(l);
        newBlock(r);
    };

    auto assignmentStatement = [&] (const ASTNode &node) {
        if(node.hasChild("index")) {
            auto id = node.getChild("identifier").valIter->str();
            auto res = local.lookup(id);
            if((res.type != TLocalVariable && res.type != TGlobalVariable) || (res.result.v->type != VarIntArray && res.result.v->type != VarCharArray)) {
                Logger::getInstance().error(node.getChild("identifier"), "need an array here");
                return;
            }
            const auto &v = *res.result.v;
            std::string e;
            VarType type;
            std::tie(e, type) = requireId(node.getChild("expression"), tempVar());
            if((type == VarIntType) && (v.type == VarCharArray)) {
                Logger::getInstance().error(node.getChild("identifier"), "unmatched type");
                return;
            }
            std::string i;
            VarType _;
            std::tie(i, _) = requireId(node.getChild("index"), tempVar());
            arr(ostorearr, id, i, e);
        } else {
            auto id = node.getChild("identifier").valIter->str();
            auto res = local.lookup(id);
            if((res.type != TLocalVariable && res.type != TGlobalVariable && res.type != TParameter) || (res.result.v->type != VarIntType && res.result.v->type != VarCharType)) {
                Logger::getInstance().error(node.getChild("identifier"), "need a plain variable here");
                return;
            }
            const auto &v = *res.result.v;
            std::string e;
            VarType type;
            std::tie(e, type) = requireId(node.getChild("expression"), tempVar());
            if((type == VarIntType) && (v.type == VarCharType)) {
                Logger::getInstance().error(node.getChild("identifier"), "unmatched type");
                return;
            }
            if(id != e) {
                si(omov, id, e);
            }
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
                involk(c);
            }
        }
    };

    for(const auto &c : node) {
        if(c.is("Statement")) {
            statement(c);
        }
    }

    if(local.node.is("MainFunc"))
        newBlock(local.endLabel());
}
