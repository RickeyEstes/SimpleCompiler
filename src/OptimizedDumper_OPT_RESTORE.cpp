#include "OptimizedDumper.h"

#include <set>
#include <map>

void optRestore(const Function &local,
        const std::vector<std::string> &labels,
        std::vector<std::vector<MC>> &entities,
        const std::vector<std::map<std::string, size_t>> &ieMap,
        const std::vector<std::set<std::string>> &usage,
        const std::vector<std::set<std::string>> &cover,
        std::vector<std::set<std::string>> &restore) {
    std::vector<std::vector<size_t>> nxt = optCalcNxt(entities, labels);

    auto update = [&] (std::set<std::string> &cur, const std::set<std::string> &cU,
            const std::set<std::string> &cR, const std::set<std::string> &cC) -> bool {
        size_t prevN = cur.size();
        for(const auto &i : cU)
            cur.insert(i);
        for(const auto &i : cR)
        if(cC.find(i) == cC.end())
            cur.insert(i);
        return cur.size() != prevN;
    };

    restore.resize(entities.size());

    bool opt = true;
    while(opt) {
        opt = false;
        for(size_t i = 0; i < entities.size(); ++i)
        for(size_t j : nxt[i])
            opt |= update(restore[i], usage[j], restore[j], cover[j]);
    }

    for(size_t i = 0; i < entities.size(); ++i)
    for(const auto &id : cover[i])
    if(local.lookup(id).type == TGlobalVariable)
        restore[i].insert(id);

    const std::set<OP> retainOps {
        oli, oneg, omov, omovv0, oarg,
        oadd, osub, omul, odiv,
        oloadarr, ostorearr
    }, endOps {
        ojmp, obeq, obne, oblt, oble, obgt, obge,
        obeqz, obnez,
        ocall, oret,
        opstr, opint, opchar,
        orint, orchar
    };

    // auto recover = [&] (const std::vector<MC> &code, const std::set<std::string> &cv,
    //         const std::map<std::string, size_t> &ie, const std::set<std::string> &rst) {
    //     std::vector<std::vector<std::string>> rc;
    //     rc.resize(code.size());
    //     for(const auto &item : ie)
    //     if(rst.find(item.first) != rst.end() && cv.find(item.first) != cv.end())
    //         rc[item.second].push_back(item.first);
    //     std::vector<MC> nc;
    //     for(size_t i = 0; i < code.size(); ++i) {
    //         nc.push_back(code[i]);
    //         for(const auto &id : rc[i]) {
    //             nc.emplace_back(MC{
    //                 omov, "", id, std::string("#") + std::to_string(i), ""
    //             });
    //         }
    //     }
    //     return nc;
    // };

    auto recover = [&] (const std::vector<MC> &code, const std::set<std::string> &cv,
            const std::map<std::string, size_t> &ie, const std::set<std::string> &rst) {
        std::set<std::string> rc;
        for(const auto &item : ie)
        if(rst.find(item.first) != rst.end() && cv.find(item.first) != cv.end())
            rc.insert(item.first);
        std::vector<MC> nc;
        for(size_t i = 0; i < code.size(); ++i) {
            if(endOps.find(code[i].o) != endOps.end()) {
                for(const auto &id : rc) {
                    auto iter = ie.find(id);
                    assert(iter != ie.end());
                    nc.emplace_back(MC{
                        omov, "", id, std::string("#") + std::to_string(iter->second), ""
                    });
                }
                rc.clear();
                assert(i == code.size() - 1);
            }
            nc.push_back(code[i]);
        }
        if(!rc.empty()) {
            for(const auto &id : rc) {
                auto iter = ie.find(id);
                assert(iter != ie.end());
                nc.emplace_back(MC{
                    omov, "", id, std::string("#") + std::to_string(iter->second), ""
                });
            }
            rc.clear();
        }
        return nc;
    };


    for(size_t i = 0; i < entities.size(); ++i)
        entities[i] = recover(entities[i], cover[i], ieMap[i], restore[i]);

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Restoring block " << i << std::endl;
        else
            std::cerr << "Restoring block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif

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
    }, geneDstOps {
        oli, oneg, omov, omovv0,
        oadd, osub, omul, odiv
    };

    auto inlineUsing = [&] (const std::vector<MC> &code) {
        std::set<std::string> u;
        for(const auto &c : code) {
            if(useDstOps.find(c.o) != useDstOps.end() && c.dst[0] == '#') {
                u.insert(c.dst);
            }
            if(useAOps.find(c.o) != useAOps.end() && c.a[0] == '#') {
                u.insert(c.a);
            }
            if(useBOps.find(c.o) != useBOps.end() && c.b[0] == '#') {
                u.insert(c.b);
            }
        }
        return u;
    };

    auto removeUnused = [&] (const std::vector<MC> &code, const std::set<std::string> &prob) {
        std::vector<MC> nc;
        for(size_t i = 0; i < code.size(); ++i) {
            const auto &c = code[i];
            if(c.o == oloadarr && c.b[0] == '#' && prob.find(c.b) == prob.end()) {
                #ifdef DEBUG
                std::cerr << "AGGRESSIVE Optimization removed: " << i
                    << " loadarr " << c.b << " " << c.lab << " " << c.a << std::endl;
                #endif
                continue;
            }
            if(geneDstOps.find(c.o) != geneDstOps.end()
                    && c.dst[0] == '#' && prob.find(c.dst) == prob.end()) {
                #ifdef DEBUG
                std::cerr << "AGGRESSIVE Optimization removed: " << i << " "
                    << c.o << " " << c.dst << " " << c.a << " " << c.b << std::endl;
                #endif
                continue;
            }
            nc.push_back(c);
        }
        return nc;
    };

    opt = true;
    while(opt) {
        opt = false;
        for(size_t i = 0; i < entities.size(); ++i) {
            size_t prevN = entities[i].size();
            entities[i] = removeUnused(entities[i], inlineUsing(entities[i]));
            if(entities[i].size() != prevN)
                opt = true;
        }
    }

#ifdef DEBUG
    std::cerr << std::endl;
    for(size_t i = 0; i < entities.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Restoring block " << i << std::endl;
        else
            std::cerr << "Restoring block " << i << " (" << labels[i] << ")" << std::endl;
        #endif
        for(const auto &c : entities[i])
            c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
#endif
}
