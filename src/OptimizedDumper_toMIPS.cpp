#include "OptimizedDumper.h"

struct _code_ {
    std::string s;
    _code_(): s() {}
    _code_(std::string _s): s(_s) {}
    _code_ operator<<(const char *t) const {
        return _code_(s + std::string(t));
    }
    _code_ operator<<(const std::string &t) const {
        return _code_(s + t);
    }
    template<class T>
    _code_ operator<<(const T &t) const {
        return _code_(s + std::to_string(t));
    }
};

void optToMIPS(Function &local, OptimizedDumper &dumper) {
    const auto &entities = dumper.info[&local].codes;
    const auto &labels = dumper.info[&local].labels;

    auto calcArgRela = [&] (size_t i) {
        return -((int64_t)i + 1ll) * 4ll;
    };
    auto calcArgInst = [&] (const std::string &funcName, size_t i) {
        auto res = local.lookup(funcName);
        assert(res.type == TFunction);
        assert(res.result.f->paramList[i].type == VarCharType || res.result.f->paramList[i].type == VarIntType);
        return res.result.f->paramList[i].type == VarCharType ? "sb" : "sw";
    };

    auto hasStInter = dumper.hasStInter[&local];
    auto hasCall = dumper.hasCall[&local];
    size_t stackSize = local.varList.space() + 4 + local.paramList.space();

    std::map<std::string, std::pair<size_t, char>> rela;
    for(size_t i = 0; i < local.varList.size(); ++i)
        rela[local.varList[i].identifier] = {local.varList.getAddress(i),
            local.varList[i].type == VarCharType || local.varList[i].type == VarCharArray ? 'b' : 'w'};
    rela["$ra"] = {local.varList.space(), 'w'};
    for(size_t i = 0; i < local.paramList.size(); ++i)
        rela[local.paramList[i].identifier] = {stackSize - local.paramList.getAddress(i) - 4,
            local.paramList[i].type == VarCharType ? 'b' : 'w'};

    auto &asmCode = dumper.text[local.identifier];

    auto C = _code_();
    auto W = [&] (const _code_ &c) {
        asmCode.push_back(c.s);
    };

    std::map<OP, std::string> repr {
        {oli, "li"}, {oneg, "neg"},
        {oadd, "addu"}, {osub, "subu"}, {omul, "mul"}, {odiv, "div"},
        {obeq, "beq"}, {obne, "bne"}, {oblt, "blt"},
        {oble, "ble"}, {obgt, "bgt"}, {obge, "bge"},
        {obeqz, "beqz"}, {obnez, "bnez"}
    };

    auto syscall = [&] (int32_t n) {
        W(C << "li $v0, " << n);
        W(C << "syscall");
    };

    auto ret = [&] (const std::string &dst) {
        if(local.node.is("MainFunc")) {
            W(C << "j " << local.endLabel());
            return;
        }
        if(hasCall)
            W(C << "lw $ra, " << rela["$ra"].first << "($sp)");
        if(hasCall || hasStInter)
            W(C << "addiu $sp, $sp, " << stackSize);
        if(!dst.empty()) {
            if(dst[0] == '$') {
                W(C << "move $v0, " << dst);
            } else if(isdigit(dst[0]) || dst[0] == '-') {
                W(C << "li $v0, " << dst);
            } else {
                if(rela.find(dst) == rela.end()) {
                    auto res = local.lookup(dst);
                    assert(res.type == TGlobalVariable);
                    assert(res.result.v->type == VarIntType || res.result.v->type == VarCharType);
                    auto ins = res.result.v->type == VarCharType ? "lb" : "lw";
                    W(C << ins << " " << "$v0" << ", " << res.result.v->getLabel());
                } else {
                    auto ins = rela[dst].second == 'b' ? "lb" : "lw";
                    W(C << ins << " " << "$v0" << ", " << rela[dst].first << "($sp)");
                }
            }
        }
        W(C << "jr $ra");
    };

    std::map<std::string, size_t> tp {
        {"2", 1}, {"4", 2}, {"8", 3}, {"16", 4}, {"32", 5},
        {"64", 6}, {"128", 7}, {"256", 8}, {"512", 9}, {"1024", 10},
        {"2048", 11}, {"4096", 12}, {"8192", 13}, {"16384", 14},
        {"32768", 15}, {"65536", 16}, {"131072", 17}, {"262144", 18},
        {"524288", 19}, {"1048576", 20}, {"2097152", 21},
        {"4194304", 22}, {"8388608", 23}, {"16777216", 24},
        {"33554432", 25}, {"67108864", 26}, {"134217728", 27},
        {"268435456", 28}, {"536870912", 29}, {"1073741824", 30},
        {"2147483648", 31}
    };

    for(size_t i = 0; i < entities.size(); ++i) {
        const auto &block = entities[i];
        if(!labels[i].empty())
            W(C << labels[i] << ":");

        if(i == 0) {
            if(hasCall || hasStInter)
                W(C << "addiu $sp, $sp, -" << stackSize);
            if(hasCall && !local.node.is("MainFunc"))
                W(C << "sw $ra, " << rela["$ra"].first << "($sp)");
        }

        for(const auto &code: block) {
            switch(code.o) {
            case oneg:
            case oli:
                W(C << repr[code.o] << " " << code.dst << ", " << code.a);
                break;
            case omov:
                if(code.dst[0] == '$' && code.a[0] == '$') {
                    W(C << "move " << code.dst << ", " << code.a);
                    break;
                }
                if(code.dst[0] == '$') {
                    if(rela.find(code.a) == rela.end()) {
                        auto res = local.lookup(code.a);
                        assert(res.type == TGlobalVariable);
                        assert(res.result.v->type == VarIntType || res.result.v->type == VarCharType);
                        auto ins = res.result.v->type == VarCharType ? "lb" : "lw";
                        W(C << ins << " " << code.dst << ", " << res.result.v->getLabel());
                    } else {
                        auto ins = rela[code.a].second == 'b' ? "lb" : "lw";
                        W(C << ins << " " << code.dst << ", " << rela[code.a].first << "($sp)");
                    }
                } else {
                    if(rela.find(code.dst) == rela.end()) {
                        auto res = local.lookup(code.dst);
                        assert(res.type == TGlobalVariable);
                        assert(res.result.v->type == VarIntType || res.result.v->type == VarCharType);
                        auto ins = res.result.v->type == VarCharType ? "sb" : "sw";
                        W(C << ins << " " << code.a << ", " << res.result.v->getLabel());
                    } else {
                        auto ins = rela[code.dst].second == 'b' ? "sb" : "sw";
                        W(C << ins << " " << code.a << ", " << rela[code.dst].first << "($sp)");
                    }
                }
                break;
            case oarg:
                {
                    size_t i;
                    sscanf(code.dst.c_str(), "%zu", &i);
                    W(C << calcArgInst(code.lab, i) << " " << code.a << ", " << calcArgRela(i) << "($sp)");
                }
                break;
            case oadd:
            case osub:
            case omul:
            case odiv:
                if(code.o == oadd && (isdigit(code.b[0]) || code.b[0] == '-')) {
                    W(C << "addiu " << code.dst << ", " << code.a << ", " << code.b);
                    break;
                }
                if(code.o == omul && tp.find(code.b) != tp.end()) {
                    W(C << "sll " << code.dst << ", " << code.a << ", " << tp[code.b]);
                    break;
                }
                if(code.o == odiv && tp.find(code.b) != tp.end()) {
                    W(C << "sra " << code.dst << ", " << code.a << ", " << tp[code.b]);
                    break;
                }
                W(C << repr[code.o] << " " << code.dst << ", " << code.a << ", " << code.b);
                break;
            case oloadarr:
                if(rela.find(code.lab) == rela.end()) {
                    auto res = local.lookup(code.lab);
                    assert(res.type == TGlobalVariable);
                    assert(res.result.v->type == VarIntArray || res.result.v->type == VarCharArray);
                    if(res.result.v->type == VarIntArray) {
                        W(C << "sll $t9, " << code.a << ", 2");
                        W(C << "lw " << code.b << ", " << res.result.v->getLabel() << "($t9)");
                    } else {
                        W(C << "lb " << code.b << ", " << res.result.v->getLabel() << "(" << code.a << ")");
                    }
                } else {
                    if(rela[code.lab].second == 'w') {
                        W(C << "sll $t9, " << code.a << ", 2");
                        W(C << "addu $t9, $t9, $sp");
                        W(C << "lw " << code.b << ", " << rela[code.lab].first << "($t9)");
                    } else {
                        W(C << "addu $t9, " << code.a << ", $sp");
                        W(C << "lb " << code.b << ", " << rela[code.lab].first << "($t9)");
                    }
                }
                break;
            case ostorearr:
                if(rela.find(code.lab) == rela.end()) {
                    auto res = local.lookup(code.lab);
                    assert(res.type == TGlobalVariable);
                    assert(res.result.v->type == VarIntArray || res.result.v->type == VarCharArray);
                    if(res.result.v->type == VarIntArray) {
                        W(C << "sll $t9, " << code.a << ", 2");
                        W(C << "sw " << code.b << ", " << res.result.v->getLabel() << "($t9)");
                    } else {
                        W(C << "sb " << code.b << ", " << res.result.v->getLabel() << "(" << code.a << ")");
                    }
                } else {
                    if(rela[code.lab].second == 'w') {
                        W(C << "sll $t9, " << code.a << ", 2");
                        W(C << "addu $t9, $t9, $sp");
                        W(C << "sw " << code.b << ", " << rela[code.lab].first << "($t9)");
                    } else {
                        W(C << "addu $t9, " << code.a << ", $sp");
                        W(C << "sb " << code.b << ", " << rela[code.lab].first << "($t9)");
                    }
                }
                break;
            case ojmp:
                W(C << "j " << code.lab);
                break;
            case obeq:
            case obne:
            case oblt:
            case oble:
            case obgt:
            case obge:
                if(code.b == "0" || code.b == "-0")
                    W(C << repr[code.o] << "z " << code.a << ", " << code.lab);
                else
                    W(C << repr[code.o] << " " << code.a << ", " << code.b << ", " << code.lab);
                break;
            case obeqz:
            case obnez:
                W(C << repr[code.o] << " " << code.a << ", " << code.lab);
                break;
            case ocall:
                W(C << "jal " << local.lookup(code.lab).result.f->entryLabel());
                break;
            case oret:
                ret(code.dst);
                break;
            case opstr:
                W(C << "la $a0, " << code.lab);
                syscall(4);
                break;
            case opint:
                if(isdigit(code.dst[0]) || code.dst[0] == '-')
                    W(C << "li $a0, " << code.dst);
                else
                    W(C << "move $a0, " << code.dst);
                syscall(1);
                break;
            case opchar:
                if(isdigit(code.dst[0]) || code.dst[0] == '-')
                    W(C << "li $a0, " << code.dst);
                else
                    W(C << "move $a0, " << code.dst);
                syscall(11);
                break;
            case orint:
                syscall(5);
                break;
            case orchar:
                syscall(12);
                break;
            case omovv0:
            default:
                assert(false);
                break;
            }
        }
        W(C);
    }

    if(!local.node.is("MainFunc"))
        ret("");
}
