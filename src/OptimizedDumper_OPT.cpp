#include "OptimizedDumper.h"

static void optJump(std::vector<std::vector<MC>> &codes, const std::vector<std::string> &labels) {
    const std::set<OP> branchOps {
        ojmp, obeq, obne, oblt, oble, obgt, obge, obeqz, obnez
    };
    std::map<std::string, size_t> revLabels;
    for(size_t i = 0; i < labels.size(); ++i) {
        if(labels[i].size() > 0) {
            assert(revLabels.find(labels[i]) == revLabels.end());
            revLabels[labels[i]] = i;
        }
    }
    std::map<std::string, size_t> _firstMC;
    auto firstMC = [&] (const std::string &l) {
        if(_firstMC.find(l) == _firstMC.end()) {
            for(size_t i = revLabels[l]; i < codes.size(); ++i) {
                if(codes[i].size() <= 0)
                    continue;
                _firstMC[l] = i;
                break;
            }
            if(_firstMC.find(l) == _firstMC.end())
                _firstMC[l] = codes.size();
        }
        return _firstMC[l];
    };
    bool opt = true;
    while(opt) {
        opt = false;
        for(size_t i = codes.size() - 1; i >= 0 && i < codes.size(); --i) {
            if(codes[i].empty())
                continue;
            if(branchOps.find(codes[i].back().o) != branchOps.end()) {
                MC &c = codes[i].back();
                size_t fm = firstMC(c.lab);
                if(fm >= codes.size())
                    continue;
                MC &d = codes[fm].front();
                if(d.o != ojmp || d.lab == c.lab)
                    continue;
                #ifdef DEBUG
                std::cerr << "Optimized jump from " << c.lab <<
                    " to " << d.lab << " in block " << i << std::endl;
                #endif
                c.lab = d.lab;
                opt = true;
            }
        }
    }
}

std::vector<std::vector<size_t>> optCalcNxt(const std::vector<std::vector<MC>> &entities,
        const std::vector<std::string> &labels) {
    const std::set<OP> branchOps {
        ojmp, obeq, obne, oblt, oble, obgt, obge, obeqz, obnez
    };

    std::map<std::string, size_t> revLabels;
    for(size_t i = 0; i < entities.size(); ++i)
        revLabels[labels[i]] = i;

    std::vector<std::vector<size_t>> nxt;
    nxt.resize(entities.size());
    for(size_t i = 0; i < entities.size(); ++i) {
        if(entities[i].empty()) {
            if(i + 1 < entities.size())
                nxt[i].push_back(i + 1);
        } else {
            const auto &c = entities[i].back();
            if(i + 1 < entities.size() && c.o != oret)
                nxt[i].push_back(i + 1);
            if(branchOps.find(c.o) != branchOps.end())
                nxt[i].push_back(revLabels[c.lab]);
        }
    }

    return nxt;
}

void optimizeMC(Function &local, std::vector<std::vector<MC>> &codes,
        const std::vector<std::string> &labels, OptimizedDumper &dumper) {
    #ifdef DEBUG
    std::cerr << "Optimizing function " << local.identifier << std::endl;
    #endif

    optJump(codes, labels);

    std::vector<std::vector<MC>> entities;
    std::vector<std::map<std::string, size_t>> ieMap;
    std::vector<std::set<std::string>> usage;
    std::vector<std::set<std::string>> cover;

    entities.resize(codes.size());
    ieMap.resize(codes.size());
    usage.resize(codes.size());
    cover.resize(codes.size());

    for(size_t i = 0; i < codes.size(); ++i) {
        #ifdef DEBUG
        if(labels[i].empty())
            std::cerr << "Optimizing block " << i << std::endl;
        else
            std::cerr << "Optimizing block " << i << " (" << labels[i] << ")" << std::endl;
        #endif

        optDAG(local, codes[i], entities[i], ieMap[i], usage[i], cover[i]);
    }

    std::vector<std::set<std::string>> restore;

    optRestore(local, labels, entities, ieMap, usage, cover, restore);

    optAssignReg(local, entities, labels, usage, dumper);

    dumper.info[&local].codes = entities;
}
