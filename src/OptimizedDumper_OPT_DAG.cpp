#include "OptimizedDumper.h"

#include <set>
#include <map>

static bool isId(const std::string &id) {
    return !id.empty() && id[0] != '#' && id[0] != '-' && !isdigit(id[0]) && id[0] != '$';
}

void optDAG(const Function &local, std::vector<MC> &codes,
        std::vector<MC> &entities, std::map<std::string, size_t> &ie,
        std::set<std::string> &usage, std::set<std::string> &cover) {
    const std::set<OP> calcOps {
        oli, oneg, omov, oarg,
        oadd, osub, omul, odiv,
        oloadarr, ostorearr
    }, endOps {
        ojmp, obeq, obne, oblt, oble, obgt, obge,
        obeqz, obnez,
        ocall, oret,
        opstr, opint, opchar,
        orint, orchar
    };
    
    if(codes.empty())
        return;
    
    size_t n = calcOps.find(codes.back().o) != calcOps.end() ? codes.size() : codes.size() - 1;

    if(n)
        assert(codes[0].o == omovv0 || calcOps.find(codes[0].o) != calcOps.end());
    for(size_t i = 1; i < n; ++i)
        assert(calcOps.find(codes[i].o) != calcOps.end());

    auto toLabel = [] (const size_t &k) -> std::string {
        return std::string("#") + std::to_string(k);
    };

    auto toCSeq = [] (const MC &c) -> std::string {
        return std::to_string(size_t(c.o))
            + "~" + c.lab + "~" + c.dst
            + "~" + c.a + "~" + c.b;
    };

    std::map<std::string, size_t> revE;

    auto use = [&] (const std::string &id) {
        assert(isId(id));
        if(ie.find(id) == ie.end()) {
            auto neid = entities.size();
            entities.emplace_back(MC{
                omov, "", toLabel(neid), id, ""
            });
            usage.insert(id);
            ie[id] = neid;
        }
        return ie[id];
    };

    for(size_t i = 0; i < codes.size(); ++i) {
        const MC &c = codes[i];
        switch(c.o) {
        case omovv0: {
            auto eid = entities.size();
            entities.push_back(c);

            cover.insert(c.dst);
            ie[c.dst] = eid;

            MC cc = c;
            cc.dst = toLabel(eid);
            entities[eid] = cc;
            break;
        }
        case oli: {
            MC cc = c;
            cc.dst = "";
            if(revE.find(toCSeq(cc)) != revE.end()) {
                #ifdef DEBUG
                std::cerr << "Optimized same li: li " << c.dst << " " << c.a << std::endl;
                #endif
                cover.insert(c.dst);
                ie[c.dst] = revE[toCSeq(cc)];
                break;
            }

            auto eid = entities.size();
            entities.push_back(c);

            cover.insert(c.dst);
            ie[c.dst] = eid;

            revE[toCSeq(cc)] = eid;

            cc.dst = toLabel(eid);
            entities[eid] = cc;
            break;
        }
        case omov: {
            use(c.a);

            cover.insert(c.dst);
            ie[c.dst] = ie[c.a];
            break;
        }
        case oarg: {
            use(c.a);

            MC cc = c;
            cc.a = toLabel(ie[c.a]);
            entities.push_back(cc);
            break;
        }
        case oneg: {
            use(c.a);

            if(ie.find(c.a) != ie.end() && entities[ie[c.a]].o == oneg) {
                #ifdef DEBUG
                std::cerr << "Optimized continuous negs: neg " << c.dst << " " << c.a << std::endl;
                #endif
                cover.insert(c.dst);
                ie[c.dst] = ie[c.a];
                break;
            }

            auto eid = entities.size();
            entities.push_back(c);

            cover.insert(c.dst);
            ie[c.dst] = eid;

            MC cc = c;
            cc.dst = toLabel(eid);
            cc.a = toLabel(ie[c.a]);
            entities[eid] = cc;
            break;
        }
        case oloadarr: {
            use(c.a);

            auto eid = entities.size();
            entities.push_back(c);

            cover.insert(c.b);
            ie[c.b] = eid;

            MC cc = c;
            cc.a = toLabel(ie[c.a]);
            cc.b = toLabel(eid);
            entities[eid] = cc;
            break;
        }
        case ostorearr: {
            MC cc = c;
            cc.a = toLabel(use(c.a));
            cc.b = toLabel(use(c.b));
            entities.push_back(cc);
            break;
        }
        case oadd:
        case osub:
        case omul: {
            MC ca = c;
            ca.dst = "";
            ca.a = toLabel(use(ca.a));
            if(isId(ca.b))
                ca.b = toLabel(use(ca.b));

            if(revE.find(toCSeq(ca)) != revE.end()) {
                #ifdef DEBUG
                std::cerr << "Optimized same calc: " << c.o << " "
                    << c.dst << " " << c.a << " " << c.b << std::endl;
                #endif
                cover.insert(c.dst);
                ie[c.dst] = revE[toCSeq(ca)];
                break;
            }

            if(ca.b[0] == '#') {
                MC cb = ca;
                std::swap(cb.a, cb.b);
                if(revE.find(toCSeq(cb)) != revE.end()) {
                    #ifdef DEBUG
                    std::cerr << "Optimized same calc: " << c.o << " "
                        << c.dst << " " << c.a << " " << c.b << std::endl;
                    #endif
                    cover.insert(c.dst);
                    ie[c.dst] = revE[toCSeq(cb)];
                    break;
                }
            }

            if(c.o == osub && ca.a == ca.b && c.a != c.dst && c.b != c.dst) {
                MC cc {oli, "", "", "0", ""};
                if(revE.find(toCSeq(cc)) != revE.end()) {
                    #ifdef DEBUG
                    std::cerr << "Optimized same li 0: sub " << c.dst
                        << " " << c.a << " " << c.b << std::endl;
                    #endif
                    cover.insert(c.dst);
                    ie[c.dst] = revE[toCSeq(cc)];
                    break;
                }
                #ifdef DEBUG
                std::cerr << "Optimized sub same: sub " << c.dst
                    << " " << c.a << " " << c.b << std::endl;
                #endif

                auto eid = entities.size();
                entities.push_back(c);

                cover.insert(c.dst);
                ie[c.dst] = eid;

                revE[toCSeq(cc)] = eid;

                cc.dst = toLabel(eid);
                entities[eid] = cc;
                break;
            }

            auto eid = entities.size();
            entities.push_back(c);

            cover.insert(c.dst);
            ie[c.dst] = eid;

            revE[toCSeq(ca)] = eid;

            ca.dst = toLabel(eid);
            entities[eid] = ca;
            break;
        }
        case odiv: {
            MC ca = c;
            ca.dst = "";
            ca.a = toLabel(use(ca.a));
            if(isId(ca.b))
                ca.b = toLabel(use(ca.b));

            if(revE.find(toCSeq(ca)) != revE.end()) {
                #ifdef DEBUG
                std::cerr << "Optimized same div: div "
                    << c.dst << " " << c.a << " " << c.b << std::endl;
                #endif
                cover.insert(c.dst);
                ie[c.dst] = revE[toCSeq(ca)];
                break;
            }

            auto eid = entities.size();
            entities.push_back(c);

            cover.insert(c.dst);
            ie[c.dst] = eid;

            revE[toCSeq(ca)] = eid;

            ca.dst = toLabel(eid);
            entities[eid] = ca;
            break;
        }
        case ojmp:
        case ocall:
        case opstr:
            entities.push_back(c);
            break;
        case obeq:
        case obne:
        case oblt:
        case oble:
        case obgt:
        case obge: {
            MC cc = c;
            cc.a = toLabel(use(c.a));
            if(isId(c.b))
                cc.b = toLabel(use(c.b));
            entities.push_back(cc);
            break;
        }
        case obeqz:
        case obnez: {
            MC cc = c;
            cc.a = toLabel(use(c.a));
            entities.push_back(cc);
            break;
        }
        case orchar:
        case orint:
            entities.push_back(c);
            break;
        case oret:
            if(c.dst.empty() || !isId(c.dst)) {
                entities.push_back(c);
                break;
            }
        case opchar:
        case opint: {
            if(c.dst[0] == '-' || isdigit(c.dst[0])) {
                entities.push_back(c);
                break;
            }
            use(c.dst);

            auto eid = entities.size();
            entities.push_back(c);

            MC cc = c;
            cc.dst = toLabel(ie[c.dst]);
            entities[eid] = cc;
            break;
        }
        default:
            assert(false);
            break;
        }
    }

#ifdef DEBUG
    std::cerr << std::endl;
    for(const auto &c : entities) {
        c.toQuad(std::cerr, "", local);
    }
    std::cerr << std::endl;
    for(const auto &item : ie) {
        if(cover.find(item.first) == cover.end())
            continue;
        std::cerr << item.first << " <- #" << item.second << std::endl;
    }
    std::cerr << std::endl;
#endif
}
