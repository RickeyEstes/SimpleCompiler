#include "OptimizedDumper.h"

#include <set>
#include <map>

static bool isId(const std::string &id) {
    return !id.empty() && id[0] != '#' && id[0] != '-' && !isdigit(id[0]) && id[0] != '$';
}

void optAssignReg(Function &local,
        std::vector<std::vector<MC>> &entities,
        const std::vector<std::string> &labels,
        const std::vector<std::set<std::string>> &usage,
        OptimizedDumper &dumper) {

    auto &hasCall = dumper.hasCall[&local];
    hasCall = false;
    for(const auto &block : entities) {
        for(const auto &code: block)
        if(code.o == ocall)
            hasCall = true;
    }

    auto replaceToReg = [&] (size_t l, size_t r,
            const std::string var, const std::string reg) {
        for(size_t i = l; i < r; ++i) {
            // std::set<std::string> isomorph {var};
            // for(auto &code: entities[i]) {
            //     if(code.o == omov && code.a == var && code.dst[0] == '#')
            //         isomorph.insert(code.dst);
            //     if(isomorph.find(code.dst) != isomorph.end())
            //         code.dst = reg;
            //     if(isomorph.find(code.a) != isomorph.end())
            //         code.a = reg;
            //     if(isomorph.find(code.b) != isomorph.end())
            //         code.b = reg;
            // }
            for(auto &code: entities[i]) {
                if(code.dst == var)
                    code.dst = reg;
                if(code.a == var)
                    code.a = reg;
                if(code.b == var)
                    code.b = reg;
            }
        }
    };

    std::vector<std::vector<size_t>> nxt = optCalcNxt(entities, labels);

    for(const auto &u : nxt)
    for(const auto &i : u)
        assert(i != 0);

    auto &polluteAReg = dumper.polluteAReg[&local];

    const std::set<OP> pollution {
        oarg, ocall, opstr, opint, opchar
    };
    polluteAReg = false;
    for(const auto &block : entities) {
        for(const auto &code: block)
        if(pollution.find(code.o) != pollution.end()) {
            polluteAReg = true;
            break;
        }
        if(polluteAReg)
            break;
    }

    const auto &paramList = local.paramList;

    const std::set<OP> endOps {
        ojmp, obeq, obne, oblt, oble, obgt, obge,
        obeqz, obnez,
        ocall, oret,
        opstr, opint, opchar,
        orint, orchar
    };

    auto pollutedBlock = [&] (const std::vector<MC> &block) {
        for(const auto &code : block)
        if(pollution.find(code.o) != pollution.end())
            return true;
        return false;
    };
    auto usedAfter = [&] (size_t j, const std::string &param) {
        for(; j < entities.size(); ++j)
        if(usage[j].find(param) != usage[j].end())
            return true;
        return false;
    };

    if(polluteAReg) {
        if(!entities[0].empty())
        for(size_t i = 0; i < 4 && i < paramList.size(); ++i) {
            std::string param = paramList[i].identifier, reg = std::string("$a") + std::to_string(i);

            if(pollutedBlock(entities[0])) {
                if(usedAfter(0, param)) {
                    entities[0].insert(entities[0].begin(), MC{
                        omov, "", param, reg, ""
                    });
                }
                continue;
            }

            replaceToReg(0, 1, param, reg);

            if(usedAfter(1, param)) {
                if(endOps.find(entities[0].back().o) == endOps.end()) {
                    entities[0].insert(entities[0].end(), MC{
                        omov, "", param, reg, ""
                    });
                } else {
                    entities[0].insert(entities[0].end() - 1, MC{
                        omov, "", param, reg, ""
                    });
                }
            }
        }
    } else {
        for(size_t i = 0; i < 4 && i < paramList.size(); ++i) {
            std::string param = paramList[i].identifier, reg = std::string("$a") + std::to_string(i);
            replaceToReg(0, entities.size(), param, reg);
        }
    }

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Assigning argument registers for block " << i << std::endl;
        else
            std::cerr << "Assigning argument registers for block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

    for(size_t i = 0; i < entities.size(); ++i) {
        std::string t;
        if(!entities[i].empty() && entities[i].front().o == omovv0) {
            t = entities[i].front().dst;
            entities[i].front() = MC{
                omov, "", t, "$v0", ""
            };
            if(t[0] == '#')
                replaceToReg(i, i + 1, t, "$v0");
        }
        for(auto &code: entities[i])
            assert(code.o != omovv0);
    }

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Assigning v0 registers for block " << i << std::endl;
        else
            std::cerr << "Assigning v0 registers for block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

    std::map<std::string, double> weights;
    std::vector<double> logTimes;
    logTimes.resize(entities.size());

    for(size_t i = 0; i < entities.size(); ++i)
    for(size_t j : nxt[i])
    if(j <= i)
    for(size_t k = j; k <= i; ++k)
        ++logTimes[k];

    auto notGlobal = [&] (const std::string &id) {
        auto t = local.lookup(id).type;
        return id.substr(0, 8) == "tempVar$" || t == TParameter || t == TLocalVariable;
    };
    auto inStackFirst = [&] (const std::string &var) {
        auto iter = local.paramList.lookup.find(var);
        return iter != local.paramList.lookup.end() && iter->second >= 4;
    };

    for(size_t i = 0; i < entities.size(); ++i) {
        std::map<std::string, double> tmp;
        for(const auto &code : entities[i])
        if(code.o == omov && isId(code.a) && notGlobal(code.a))
            ++tmp[code.a];
        for(const auto &item : tmp) {
            weights[item.first] = pow(3.5, weights[item.first]) + item.second * pow(3.5, logTimes[i]);
            weights[item.first] = log(std::min(weights[item.first], 1e100)) / log(3.5);
        }
    }

    std::vector<std::string> sortedLocals;
    for(const auto &item : weights)
        sortedLocals.push_back(item.first);
    std::sort(sortedLocals.begin(), sortedLocals.end(), [&] (const std::string &a, const std::string &b) {
        return weights[a] > weights[b];
    });

    std::set<std::string> leftRegs {
        "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
        "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
    };
    auto &protectRegs = dumper.protectRegs[&local];

    for(size_t i = 0; i < 8 && i < sortedLocals.size(); ++i) {
        auto var = sortedLocals[i];
        std::string reg = std::string("$s") + std::to_string(i);
        leftRegs.erase(reg);
        protectRegs[reg] = var;
        if(inStackFirst(var)) {
            // bool found = false;
            // size_t i = 0;
            // for(; i < entities.size() && !found; ++i) {
            //     size_t j = 0;
            //     for(; j < entities[i].size() && !found; ++j) {
            //         auto &code = entities[i][j];
            //         if(code.o == omov && code.a == var) {
            //             assert(code.dst[0] == '#');
            //             found = true;
            //             replaceToReg(i, i + 1, code.dst, reg);
            //         } else {
            //             assert(code.dst != var && code.a != var && code.b != var);
            //         }
            //     }
            //     if(found)
            //     for(; j < entities[i].size(); ++j) {
            //         auto &code = entities[i][j];
            //         if(code.dst == var)
            //             code.dst = reg;
            //         if(code.a == var)
            //             code.a = reg;
            //         if(code.b == var)
            //             code.b = reg;
            //     }
            // }
            // assert(found);
            // replaceToReg(i, entities.size(), var, reg);
            replaceToReg(0, entities.size(), var, reg);
            entities[0].insert(entities[0].begin(), MC{
                omov, "", reg, var, ""
            });
        } else {
            replaceToReg(0, entities.size(), var, reg);
        }
    }

    auto usedAfterInline = [&] (const std::vector<MC> &block, size_t j, const std::string &t) {
        for(size_t i = j; i < block.size(); ++i) {
            if(block[i].a == t || block[i].b == t || block[i].dst == t)
                return true;
        }
        return false;
    };

    // for(const auto &item : protectRegs) {
    //     const auto &reg = item.first;
    //     for(size_t k = 0; k < entities.size(); ++k) {
    //         const auto &block = entities[k];
    //         for(size_t i = 0; i < block.size(); ++i) {
    //             const auto &code = block[i];
    //             if(code.o == omov && code.dst == reg && code.a[0] == '#') {
    //                 if(!usedAfterInline(block, i + 1, code.a))
    //                     replaceToReg(k, k + 1, code.a, code.dst);
    //             }
    //         }
    //     }
    // }

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Assigning saved registers for block " << i << std::endl;
        else
            std::cerr << "Assigning saved registers for block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

    const std::set<OP> geneDstOps {
        oli, oneg, omov, omovv0,
        oadd, osub, omul, odiv
    };

    auto lastUsage = [&] (const std::vector<MC> &block, const std::string &t) {
        for(size_t i = block.size() - 1; i < block.size(); --i) {
            if(block[i].a == t || block[i].b == t || block[i].dst == t)
                return i + 1;
        }
        return (size_t)0;
    };

    auto assignedBetweenInline = [&] (const std::vector<MC> &block, size_t l, size_t r, const std::string &t) {
        for(size_t i = l; i < block.size() && i < r; ++i) {
            const auto &code = block[i];
            std::string dst = code.o == oloadarr ? code.b :
                geneDstOps.find(code.o) != geneDstOps.end() ? code.dst : "";
            if(dst == t)
                return true;
        }
        return false;
    };

    std::set<std::string> additionVar;

    for(size_t k = 0; k < entities.size(); ++k) {
        auto &block = entities[k];
        auto tmpRegs = leftRegs;
        std::map<std::string, std::string> assignedTo;
        std::map<std::string, std::string> assignedWith;
        std::map<std::string, size_t> usedAt;
        size_t _newTmpVar = 0;
        auto newTmpVar = [&] () {
            auto v = std::string("tempReg$") + std::to_string(_newTmpVar++);
            additionVar.insert(v);
            return v;
        };
        auto findScape = [&] (const std::string &a, const std::string &b) {
            std::string id;
            size_t u = std::numeric_limits<size_t>::max();
            for(const auto &item : usedAt) {
                if(assignedTo[item.first] != a && assignedTo[item.first] != b) {
                    if(item.second <= u) {
                        u = item.second;
                        id = item.first;
                    }
                }
            }
            return id;
        };
        std::vector<std::vector<MC>> toAdd;
        toAdd.resize(block.size());

        auto reassignScape = [&] (size_t i, const std::string &c, const std::string &a, const std::string &b) {
            auto reg = findScape(a, b);
            auto scape = assignedTo[reg];
            assignedWith[scape] = newTmpVar();
            toAdd[i].push_back(MC{
                omov, "", assignedWith[scape], reg, ""
            });
            assignedTo[reg] = c;
            assignedWith[c] = reg;
        };

        auto reassignTmpRegs = [&] (const std::string &c) {
            auto reg = *tmpRegs.begin();
            tmpRegs.erase(tmpRegs.begin());
            assignedTo[reg] = c;
            assignedWith[c] = reg;
        };

        auto reassign = [&] (size_t i, const std::string &c, const std::string &a, const std::string &b) {
            if(!c.empty() && c[0] == '#' && assignedWith[c][0] != '$') {
                auto oldAss = assignedWith[c];
                if(tmpRegs.empty()) {
                    reassignScape(i, c, a, b);
                } else {
                    reassignTmpRegs(c);
                }
                toAdd[i].push_back(MC{
                    omov, "", assignedWith[c], oldAss, ""
                });
            }
        };

        for(size_t i = 0; i < block.size(); ++i) {
            auto &code = block[i];

            std::string dst = code.o == oloadarr ? code.b :
                geneDstOps.find(code.o) != geneDstOps.end() ? code.dst : "";

            if(!dst.empty() && dst[0] == '#') {
                assert(dst != code.a);
                assert(code.o == oloadarr || dst != code.b);
                assert(assignedWith.find(dst) == assignedWith.end());

                if(code.o == omov && code.a[0] == '$' && !assignedBetweenInline(block, i + 1, lastUsage(block, code.dst), code.a)) {
                    assert(dst == code.dst);
                    auto reg = code.a;
                    replaceToReg(k, k + 1, dst, reg);
                } else if(tmpRegs.empty()) {
                    reassignScape(i, dst, code.a, code.b);
                } else {
                    reassignTmpRegs(dst);
                }
            }

            reassign(i, code.dst, code.a, code.b);
            reassign(i, code.a, code.dst, code.b);
            reassign(i, code.b, code.a, code.dst);

            if(!code.dst.empty() && assignedWith.find(code.dst) != assignedWith.end())
                code.dst = assignedWith[code.dst];
            if(!code.a.empty() && assignedWith.find(code.a) != assignedWith.end())
                code.a = assignedWith[code.a];
            if(!code.b.empty() && assignedWith.find(code.b) != assignedWith.end())
                code.b = assignedWith[code.b];

            std::vector<std::string> _ {code.dst, code.a, code.b};
            for(const auto tmp : _)
            if(!tmp.empty() && leftRegs.find(tmp) != leftRegs.end()) {
                usedAt[tmp] = i;
            }

            std::vector<std::string> unused;
            for(const auto &item : assignedTo) {
                if(!usedAfterInline(block, i + 1, item.second)) {
                    unused.push_back(item.first);
                }
            }
            for(const auto &reg : unused) {
                assignedTo.erase(reg);
                usedAt.erase(reg);
                tmpRegs.insert(reg);
            }
        }
        std::vector<MC> refactor;
        for(size_t i = 0; i < block.size(); ++i) {
            const auto &code = block[i];
            for(const auto &c : toAdd[i])
                refactor.push_back(c);
            refactor.push_back(code);
        }
        block.swap(refactor);
    }

    for(const auto &var : additionVar)
        local.addVariable(Variable{var, local.definedAt, VarIntType});

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Assigning temp registers for block " << i << std::endl;
        else
            std::cerr << "Assigning temp registers for block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

    auto removeSelfRef = [&] (const std::vector<MC> &code) {
        std::vector<MC> nc;
        for(size_t i = 0; i < code.size(); ++i) {
            const auto &c = code[i];
            if(c.o == omov && c.dst == c.a) {
                #ifdef DEBUG
                std::cerr << "AGGRESSIVE Optimization removed: " << i
                    << " mov " << c.dst << " " << c.a << std::endl;
                #endif
                continue;
            }
            nc.push_back(c);
        }
        return nc;
    };

    for(size_t i = 0; i < entities.size(); ++i)
        entities[i] = removeSelfRef(entities[i]);

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Removed self reference from block " << i << std::endl;
        else
            std::cerr << "Removed self reference from block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

    auto valid = [&] (const std::string &s) {
        assert(s.empty() || s[0] != '#');
        auto res = local.lookup(s);
        if(res.type == TNotFound && s.substr(0, 8) == "tempVar$") {
            local.addVariable(Variable{s, local.definedAt, VarIntType});
        }
        if(isId(s)) {
            assert(local.lookup(s).type != TNotFound);
        } else if(!s.empty()) {
            assert(s[0] == '$' || s[0] == '-' || isdigit(s[0]));
        }
    };

    for(const auto &block : entities) {
        for(const auto &code: block) {
            assert(code.o != omovv0);
            valid(code.dst);
            valid(code.a);
            valid(code.b);
        }
    }


    dumper.optQuad[&local].codes = entities;
    dumper.optQuad[&local].labels = labels;


    for(auto &block : entities) {
        for(auto &code: block) {
            if(code.o == oarg) {
                assert(code.a[0] == '$');
                size_t a;
                sscanf(code.dst.c_str(), "%zu", &a);
                if(a < 4) {
                    code = MC{
                        omov, "", std::string("$a") + code.dst, code.a, ""
                    };
                }
            }
        }
    }


    const std::set<OP> useDstOps {
        oret, opint, opchar
    }, useAOps {
        omov, oneg,
        oadd, osub, omul, odiv,
        oloadarr, ostorearr,
        obeqz, obnez,
        obeq, obne, oblt, oble, obgt, obge,
        oarg
    }, useBOps {
        oadd, osub, omul, odiv,
        ostorearr,
        obeq, obne, oblt, oble, obgt, obge
    };

    auto ucCalc = [&] (const std::vector<MC> &code) {
        std::set<std::string> u, v;
        for(const auto &c : code) {
            if(useDstOps.find(c.o) != useDstOps.end() && c.dst[0] == '$' && v.find(c.dst) == v.end()) {
                u.insert(c.dst);
            }
            if(useAOps.find(c.o) != useAOps.end() && c.a[0] == '$' && v.find(c.a) == v.end()) {
                u.insert(c.a);
            }
            if(useBOps.find(c.o) != useBOps.end() && c.b[0] == '$' && v.find(c.b) == v.end()) {
                u.insert(c.b);
            }
            if(geneDstOps.find(c.o) != geneDstOps.end() && c.dst[0] == '$' && u.find(c.dst) == u.end()) {
                v.insert(c.dst);
            }
        }
        return std::make_tuple(u, v);
    };

    std::vector<std::set<std::string>> used, cover, restore;
    used.resize(entities.size());
    cover.resize(entities.size());
    for(size_t i = 0; i < entities.size(); ++i)
        std::tie(used[i], cover[i]) = ucCalc(entities[i]);
    restore = used;

    auto regProtection = [&] (size_t i) {
        size_t prevN = restore[i].size();
        for(size_t j : nxt[i]) {
            for(const auto &reg : restore[j])
            if(cover[j].find(reg) == cover[j].end())
                restore[i].insert(reg);
        }
        return prevN != restore[i].size();
    };

    bool opt = true;
    while(opt) {
        opt = false;
        for(size_t i = 0; i < entities.size(); ++i)
            opt |= regProtection(i);
    }

    std::vector<std::vector<std::string>> blockProtectRegs;
    blockProtectRegs.resize(entities.size());
    for(size_t i = 0; i < entities.size(); ++i) {
        auto &block = entities[i];
        if(block.empty())
            continue;
        auto &code = block.back();
        if(code.o != ocall)
            continue;
        for(const auto &item : protectRegs) {
            if(restore[i].find(item.first) != restore[i].end())
                blockProtectRegs[i].push_back(item.first);
        }
    }

    auto protectOnCall = [&] (size_t i) {
        if(entities[i].empty() || entities[i].back().o != ocall)
            return entities[i];
        std::vector<MC> nc(entities[i].begin(), entities[i].end() - 1);
        for(const auto &reg : blockProtectRegs[i]) {
            nc.emplace_back(MC{
                omov, "", protectRegs[reg], reg, ""
            });
        }
        nc.push_back(entities[i].back());
        for(const auto &reg : blockProtectRegs[i]) {
            nc.emplace_back(MC{
                omov, "", reg, protectRegs[reg], ""
            });
        }
        return nc;
    };

    for(size_t i = 0; i < entities.size(); ++i)
        entities[i] = protectOnCall(i);

    for(size_t i = 0; i < entities.size(); ++i)
        entities[i] = removeSelfRef(entities[i]);

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "optAssignReg for block " << i << std::endl;
        else
            std::cerr << "optAssignReg for block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

    auto isVar = [&] (const std::string &id) {
        auto res = local.lookup(id);
        if(res.type == TNotFound && id.substr(0, 8) == "tempVar$") {
            local.addVariable(Variable{id, local.definedAt, VarIntType});
            res = local.lookup(id);
        }
        if(res.type == TParameter)
            return true;
        if(res.type != TLocalVariable && res.type != TGlobalVariable)
            return false;
        return res.result.v->type == VarIntType || res.result.v->type == VarCharType;
    };
    auto isArr = [&] (const std::string &id) {
        auto res = local.lookup(id);
        if(res.type != TLocalVariable && res.type != TGlobalVariable)
            return false;
        return res.result.v->type == VarIntArray || res.result.v->type == VarCharArray;
    };
    auto isFunc = [&] (const std::string &id) {
        auto res = local.lookup(id);
        return res.type == TFunction;
    };
    auto isLocal = [&] (const std::string &id) {
        auto res = local.lookup(id);
        return res.type == TLocalVariable || res.type == TParameter;
    };

    auto &hasStInter = dumper.hasStInter[&local];
    hasStInter = false;
    for(size_t i = 0; i < entities.size(); ++i) {
        const auto &block = entities[i];
        for(const auto &code: block) {
            switch(code.o) {
            case oli:
                assert(code.lab.empty());
                assert(code.dst[0] == '$');
                assert(code.a[0] == '-' || isdigit(code.a[0]));
                assert(code.b.empty());
                break;
            case oneg:
                assert(code.lab.empty());
                assert(code.dst[0] == '$');
                assert(code.a[0] == '$');
                assert(code.b.empty());
                break;
            case omov:
                assert(code.lab.empty());
                assert(code.dst[0] == '$' || isVar(code.dst));
                assert(code.a[0] == '$' || isVar(code.a));
                assert(!(isVar(code.dst) && isVar(code.a)));
                if((isVar(code.dst) && isLocal(code.dst)) || (isVar(code.a) && isLocal(code.a)))
                    hasStInter = true;
                assert(code.b.empty());
                break;
            case oarg:
                assert(isFunc(code.lab));
                assert(isdigit(code.dst[0]));
                assert(code.a[0] == '$');
                assert(code.b.empty());
                {
                    size_t i;
                    sscanf(code.dst.c_str(), "%zu", &i);
                    assert(i >= 4);
                }
                break;
            case oadd:
            case osub:
            case omul:
            case odiv:
                assert(code.lab.empty());
                assert(code.dst[0] == '$');
                assert(code.a[0] == '$');
                assert(isdigit(code.b[0]) || code.b[0] == '-' || code.b[0] == '$');
                assert(code.o != osub || code.b[0] == '$');
                break;
            case oloadarr:
            case ostorearr:
                assert(isArr(code.lab));
                assert(code.dst.empty());
                assert(code.a[0] == '$');
                assert(code.b[0] == '$');
                break;
            case ojmp:
                assert(!code.lab.empty());
                assert(code.dst.empty());
                assert(code.a.empty());
                assert(code.b.empty());
                break;
            case obeq:
            case obne:
            case oblt:
            case oble:
            case obgt:
            case obge:
                assert(!code.lab.empty());
                assert(code.dst.empty());
                assert(code.a[0] == '$');
                assert(isdigit(code.b[0]) || code.b[0] == '-' || code.b[0] == '$');
                break;
            case obeqz:
            case obnez:
                assert(!code.lab.empty());
                assert(code.dst.empty());
                assert(code.a[0] == '$');
                assert(code.b.empty());
                break;
            case ocall:
                assert(isFunc(code.lab));
                assert(code.dst.empty());
                assert(code.a.empty());
                assert(code.b.empty());
                break;
            case oret:
                assert(code.lab.empty());
                assert(code.dst.empty() || code.dst[0] == '$' || isVar(code.dst)
                    || isdigit(code.dst[0]) || code.dst[0] == '-');
                assert(code.a.empty());
                assert(code.b.empty());
                break;
            case opstr:
                assert(!code.lab.empty());
                assert(code.dst.empty());
                assert(code.a.empty());
                assert(code.b.empty());
                break;
            case opint:
            case opchar:
                assert(code.lab.empty());
                assert(code.dst[0] == '$' || isdigit(code.dst[0]) || code.dst[0] == '-');
                assert(code.a.empty());
                assert(code.b.empty());
                break;
            case orint:
            case orchar:
                assert(code.lab.empty());
                assert(!code.dst.empty());
                assert(code.a.empty());
                assert(code.b.empty());
                break;
            case omovv0:
            default:
                assert(false);
                break;
            }
        }
    }
}
