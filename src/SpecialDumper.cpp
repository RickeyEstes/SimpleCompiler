#include "SpecialDumper.h"
#include "Semantics.h"

#include <iostream>
#include <functional>
#include <regex>

static std::string escapeSlash(const std::string &s) {
    return regex_replace(s, std::regex("\\\\"), "\\\\");
}

static std::string escapePersent(const std::string &s) {
    return regex_replace(s, std::regex("%"), "%%");
}

template<>
void SpecialDumper::operator()(Program &local, const ASTNode &node) {
    ss << "#include <stdio.h>" << std::endl;

    auto dft = [&] (const ASTNode &node) {
        SourceCode::const_iterator a = node.startIter->srcPos, b = node.endIter->srcPos;
        for(auto i = a; i != b; ++i)
            ss << *i;
        ss << std::endl;
    };

    if(node.hasChild("const")) {
        dft(node.getChild("const"));
    }
    if(node.hasChild("var")) {
        dft(node.getChild("var"));
    }

    for(const Function &f : local.functions) {
        std::string t = f.node.is("VoidFunc") ? "void" : f.node.is("IntFunc") || f.node.is("MainFunc") ? "int" : "char";
        ss << t << " " << f.identifier << "(";
        bool first = true;
        for(const auto &v : f.paramList.variables) {
            if(first) {
                first = false;
            } else {
                ss << ", ";
            }
            if(v.type == VarIntType) {
                ss << "int ";
            } else {
                ss << "char ";
            }
            ss << v.identifier;
        }
        ss << ") {" << std::endl;
        ss << text[f.identifier] << std::endl;
        ss << "}" << std::endl;
    }
}

template<>
void SpecialDumper::operator()(Function &local, const ASTNode &node) {
    assert(node.is("Compound"));
    assert(text.find(local.identifier) == text.end());

    auto &code = text[local.identifier];

    auto dft = [&] (const ASTNode &node) {
        SourceCode::const_iterator a = node.startIter->srcPos, b = node.endIter->srcPos;
        for(auto i = a; i != b; ++i)
            code.push_back(*i);
    };

    if(node.hasChild("const")) {
        dft(node.getChild("const"));
    }
    if(node.hasChild("var")) {
        dft(node.getChild("var"));
    }

    auto to_cstrliteral = [] (const Token &t) {
        return escapePersent(escapeSlash(((const StringLiteral *)&t)->getLiteral()));
    };

    std::function<const Function *(const ASTNode &)> involk;
    std::function<VarType(const ASTNode &)> expression, item, factor;
    std::function<void(const ASTNode &)> statement;

    involk = [&] (const ASTNode &node) -> const Function * {
        const auto name = node.getChild("funcName").valIter->str();
        auto res = local.lookup(name);
        const Function *f = res.result.f;
        return f;
    };

    factor = [&] (const ASTNode &node) {
        if(node.hasChild("child") && node.getChild("child").is("Expression")) {
        } else if(node.hasChild("child") && node.getChild("child").is("InvolkExpression")) {
            const Function *func = involk(node.getChild("child"));
            return func->node.is("CharFunc") ? VarCharType : VarIntType;
        } else if(node.hasChild("int")) {
        } else if(node.hasChild("char")) {
            return VarCharType;
        } else if(node.hasChild("index")) {
            const std::string &arr = node.getChild("identifier").valIter->str();
            const auto res = local.lookup(arr);
            return (res.type == TGlobalVariable || res.type == TLocalVariable) && (res.result.v->type == VarCharArray) ? VarCharType : VarIntType;
        } else if(node.hasChild("identifier")) {
            const std::string &v = node.getChild("identifier").valIter->str();
            if(local.lookup(v).type == TConstant) {
                if(local.lookup(v).result.c->type == ConstCharType) {
                    return VarCharType;
                }
            } else {
                const auto res = local.lookup(v);
                return (res.type == TGlobalVariable || res.type == TLocalVariable || res.type == TParameter) && (res.result.v->type == VarCharType) ? VarCharType : VarIntType;
            }
        }
        return VarIntType;
    };

    item = [&] (const ASTNode &node) {
        if(node.getChildren().size() <= 1) {
            return factor(node.getChildren()[0]);
        }
        return VarIntType;
    };

    expression = [&] (const ASTNode &node) {
        if(node.getChildren().size() <= 1) {
            return item(node.getChildren()[0]);
        }
        return VarIntType;
    };

    auto printStatement = [&] (const ASTNode &node) {
        if(node.hasChild("string")) {
            std::string s = to_cstrliteral(*node.getChild("string").valIter);
            code += std::string("printf(\"") + s + "\");\n";
        }
        if(!node.hasChild("expression"))
            return;
        VarType t = expression(node.getChild("expression"));
        if(t == VarCharType) {
            code += std::string("printf(\"%c\", ");
        } else {
            code += std::string("printf(\"%d\", ");
        }
        dft(node.getChild("expression"));
        code += ");\n";
        code += "printf(\"\\n\");";
    };

    auto readVar = [&] (const ASTNode &node) {
        const auto id = node.valIter->str();
        auto res = local.lookup(id);
        switch(res.type) {
        case TParameter:
        case TGlobalVariable:
        case TLocalVariable: {
            const auto v = *res.result.v;
            if(v.type == VarIntType) {
                code += "scanf(\" %d\", &";
                code += id;
                code += ");";
            } else {
                code += "scanf(\" %c\", &";
                code += id;
                code += ");";
            }
            break;
        }
        default:
            break;
        }
    };

    auto scanStatement = [&] (const ASTNode &node) {
        for(const auto &c : node)
            readVar(c);
    };

    auto dowhileStatement = [&] (const ASTNode &node) {
        code += "do {\n";
        statement(node.getChild("statement"));
        code += "} while(";
        dft(node.getChild("condition"));
        code += ");\n";
    };

    auto ifStatement = [&] (const ASTNode &node) {
        if(node.hasChild("statementB")) {
            code += "if(";
            dft(node.getChild("condition"));
            code += ") {\n";
            statement(node.getChild("statementA"));
            code += "} else {\n";
            statement(node.getChild("statementB"));
            code += "}\n";
        } else {
            code += "if(";
            dft(node.getChild("condition"));
            code += ") {\n";
            statement(node.getChild("statementA"));
            code += "}\n";
        }
    };

    auto forStatement = [&] (const ASTNode &node) {
        auto id = node.getChild("idA").valIter->str();
        ((code += "for(") += id) += " = ";
        dft(node.getChild("init"));
        code += "; ";
        dft(node.getChild("condition"));
        uint32_t step = node.getChild("step").valIter->getVal<uint32_t>();
        (((((((code += "; ") += id) += " = ") += id) += " ") += node.getChild("op").valIter->tokenType.indicator) += " ") += std::to_string(step);
        code += ") {\n";
        statement(node.getChild("statement"));
        code += "}\n";
    };

    statement = [&] (const ASTNode &node) {
        for(const auto &c : node) {
            if(c.is("Statement")) {
                statement(c);
            } else if(c.is("AssignmentStatement")) {
                dft(c);
            } else if(c.is("IfStatement")) {
                ifStatement(c);
            } else if(c.is("DoWhileStatement")) {
                dowhileStatement(c);
            } else if(c.is("ForStatement")) {
                forStatement(c);
            } else if(c.is("ReturnStatement")) {
                dft(c);
            } else if(c.is("PrintStatement")) {
                printStatement(c);
            } else if(c.is("ScanStatement")) {
                scanStatement(c);
            } else if(c.is("InvolkStatement")) {
                dft(c);
            }
            code.push_back('\n');
        }
    };

    statement(node);
}
