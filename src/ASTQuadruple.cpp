#include "ASTNode.h"
#include "ASTQuadruple.h"

#include <iostream>

static std::string tempId() {
    static unsigned cnt = 0;
    return std::string("$temp") + std::to_string(cnt++);
}

static std::string tempLabel() {
    static unsigned cnt = 0;
    return std::string("%Label") + std::to_string(cnt++);
}

static std::string dumpQExpression(const ASTNode &node, std::ostream &stream) {
    switch(&node.nodeType - nodeTypes) {
    case 21:
        if(node.getChildren().size() <= 1) {
            return dumpQExpression(node.getChildren()[0], stream);
        }
        {
            auto iter = node.begin();
            auto id = tempId();
            if(node.hasChild("sign")) {
                auto first = dumpQExpression(node.getChildren()[1], stream);
                stream << id << " = 0 - " << first << std::endl;
                iter += 2;
            } else {
                auto first = dumpQExpression(node.getChildren()[0], stream);
                stream << id << " = " << first << std::endl;
                iter += 1;
            }
            for(; iter < node.getChildren().end(); iter += 2) {
                auto rv = dumpQExpression(iter[1], stream);
                stream << id << " = " << id << " " << iter[0].valIter->tokenType.indicator
                    << " " << rv << std::endl;
            }
            return id;
        }
    case 31:
        if(node.getChildren().size() <= 1) {
            return dumpQExpression(node.getChildren()[0], stream);
        }
        {
            auto id = tempId(), first = dumpQExpression(node.getChildren()[0], stream);
            stream << id << " = " << first << std::endl;
            for(size_t i = 1; i < node.getChildren().size(); i += 2) {
                auto rv = dumpQExpression(node.getChildren()[i + 1], stream);
                stream << id << " = " << id << " " << node.getChildren()[i].valIter->tokenType.indicator
                    << " " << rv << std::endl;
            }
            return id;
        }
        break;
    case 29:
        for(size_t i = 1; i < node.getChildren().size(); ++i) {
            auto id = dumpQExpression(node.getChildren()[i], stream);
            stream << "push " << id << std::endl;
        }
        {
            auto id = tempId();
            stream << "call " << node.getChild("funcName").valIter->str() << std::endl;
            stream << id << " = RET" << std::endl;
            return id;
        }
        break;
    case 22:
        if(node.hasChild("int")) {
            return std::to_string(node.getChild("int").valIter->getVal<int32_t>());
        } else if(node.hasChild("char")) {
            return std::to_string((int)node.getChild("char").valIter->getVal<char>());
        } else if(node.hasChild("child")) {
            return dumpQExpression(node.getChild("child"), stream);
        } else if(node.hasChild("identifier") && node.hasChild("index")) {
            auto id = tempId(), index = dumpQExpression(node.getChild("index"), stream);
            stream << id << " = " << node.getChild("identifier").valIter->str() << "[" << index << "]" << std::endl;
            return id;
        } else if(node.hasChild("identifier")) {
            return node.getChild("identifier").valIter->str();
        }
        assert(false);
        break;
    default:
        break;
    }
    assert(false);
    return "";
}

void dumpQuadruple(const ASTNode &node, std::ostream &stream) {
    switch(&node.nodeType - nodeTypes) {
    case 30:
        for(size_t i = 1; i < node.getChildren().size(); ++i) {
            auto id = dumpQExpression(node.getChildren()[i], stream);
            stream << "push " << id << std::endl;
        }
        stream << "call " << node.getChild("funcName").valIter->str() << std::endl;
        break;
    case 34:
        if(node.hasChild("string"))
            stream << "PRINT " << node.getChild("string").valIter->str() << std::endl;
        if(node.hasChild("expression")) {
            auto e = dumpQExpression(node.getChild("expression"), stream);
            stream << "PRINT " << e << std::endl;
        }
        break;
    case 37:
        for(const ASTNode &c : node) {
            stream << c.valIter->str() << " = READ" << std::endl;
        }
        break;
    case 23:
        {
            auto id = node.getChild("idA").valIter->str();
            auto init = dumpQExpression(node.getChild("init"), stream);
            stream << id << " = " << init << std::endl;
            auto entry = tempLabel(), endfor = tempLabel();
            stream << entry << ":";
            dumpQuadruple(node.getChild("condition"), stream);
            stream << "BZ " << endfor << std::endl;
            dumpQuadruple(node.getChild("statement"), stream);
            stream << id << " = " << id << " " << node.getChild("op").valIter->tokenType.indicator
                << " " << node.getChild("step").valIter->getVal<UnsignedInfo>().v << std::endl;
            stream << "GOTO " << entry << std::endl;
            stream << endfor << ":";
        }
        break;
    case 20:
        {
            auto label = tempLabel();
            stream << label << ":";
            dumpQuadruple(node.getChild("statement"), stream);
            dumpQuadruple(node.getChild("condition"), stream);
            stream << "BNZ " << label << std::endl;
        }
        break;
    case 25:
        if(node.hasChild("statementB")) {
            auto entryB = tempLabel(), endif = tempLabel();
            dumpQuadruple(node.getChild("condition"), stream);
            stream << "BZ " << entryB << std::endl;
            dumpQuadruple(node.getChild("statementA"), stream);
            stream << "GOTO " << endif << std::endl;
            stream << entryB << ":";
            dumpQuadruple(node.getChild("statementB"), stream);
            stream << endif << ":";
        } else {
            auto endif = tempLabel();
            dumpQuadruple(node.getChild("condition"), stream);
            stream << "BZ " << endif << std::endl;
            dumpQuadruple(node.getChild("statementA"), stream);
            stream << endif << ":";
        }
        break;
    case 16:
        if(node.hasChild("expressionB")) {
            auto u = dumpQExpression(node.getChild("expressionA"), stream), v = dumpQExpression(node.getChild("expressionB"), stream);
            stream << u << " " << node.getChild("op").valIter->tokenType.indicator << " " << v << std::endl;
        } else {
            auto u = dumpQExpression(node.getChild("expressionA"), stream);
            stream << u << " != 0" << std::endl;
        }
        break;
    case 11:
        if(node.hasChild("index")) {
            auto i = dumpQExpression(node.getChild("index"), stream), e = dumpQExpression(node.getChild("expression"), stream);
            stream << node.getChild("identifier").valIter->str() << "[" << i << "]" << " = " << e << std::endl;
        } else {
            auto e = dumpQExpression(node.getChild("expression"), stream);
            stream << node.getChild("identifier").valIter->str() << " = " << e << std::endl;
        }
        break;
    case 19:
        for(size_t i = 0; i < node.getChildren().size(); i += 2) {
            stream << "const int " << node.getChildren()[i].valIter->str() << " = " << node.getChildren()[i + 1].valIter->getVal<int32_t>() << std::endl;
        }
        break;
    case 17:
        for(size_t i = 0; i < node.getChildren().size(); i += 2) {
            stream << "const char " << node.getChildren()[i].valIter->str() << " = " << (int)node.getChildren()[i + 1].valIter->getVal<char>() << std::endl;
        }
        break;
    case 44:
        for(const ASTNode &c : node) {
            if(c.is("Identifier"))
                stream << "var int " << c.valIter->str() << std::endl;
            else if(c.is("VarArrayDecl"))
                stream << "var int " << c.getChildren()[0].valIter->str() << "[" << c.getChildren()[1].valIter->getVal<UnsignedInfo>().v << "]" << std::endl;
            else
                assert(false);
        }
        break;
    case 42:
        for(const ASTNode &c : node) {
            if(c.is("Identifier"))
                stream << "var char " << c.valIter->str() << std::endl;
            else if(c.is("VarArrayDecl"))
                stream << "var char " << c.getChildren()[0].valIter->str() << "[" << c.getChildren()[1].valIter->getVal<UnsignedInfo>().v << "]" << std::endl;
            else
                assert(false);
        }
        break;
    case 35:
    case 43:
    case 18:
    case 15:
    case 38:
        for(const ASTNode &c : node) {
            dumpQuadruple(c, stream);
        }
        break;
    case 33:
        for(size_t i = 0; i < node.getChildren().size(); i += 2) {
            stream << "para " << node.getChildren()[i].valIter->tokenType.indicator
                << " " << node.getChildren()[i + 1].valIter->str() << std::endl;
        }
        break;
    case 45:
        stream << "void " << node.valIter->str() << "()" << std::endl;
        dumpQuadruple(node.getChild("parameters"), stream);
        dumpQuadruple(node.getChild("compound"), stream);
        stream << "ret // end of function" << std::endl;
        break;
    case 26:
        stream << "int " << node.valIter->str() << "()" << std::endl;
        dumpQuadruple(node.getChild("parameters"), stream);
        dumpQuadruple(node.getChild("compound"), stream);
        stream << "ret 0 // end of function" << std::endl;
        break;
    case 12:
        stream << "char " << node.valIter->str() << "()" << std::endl;
        dumpQuadruple(node.getChild("parameters"), stream);
        dumpQuadruple(node.getChild("compound"), stream);
        stream << "ret 0 // end of function" << std::endl;
        break;
    case 32:
        stream << "void main()" << std::endl;
        dumpQuadruple(node.getChild("compound"), stream);
        stream << "ret // end of function" << std::endl;
        break;
    case 36:
        if(node.hasChild("expression")) {
            auto id = dumpQExpression(node.getChild("expression"), stream);
            stream << "ret " << id << std::endl;
        } else {
            stream << "ret" << std::endl;
        }
        break;
    default:
        assert(false);
        break;
    }
}
