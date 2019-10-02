#include "ASTNode.h"
#include "Logger.h"

#include <unordered_map>
#include <iostream>


static bool isFunc(Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    if(end - iter <= 2)
        return false;
    const Token &a = *iter++, &b = *iter++, &c = *iter++;
    if((a.is("int") || a.is("char")) && b.is("Identifier") && c.is("("))
        return true;
    if(a.is("void") && (b.is("Identifier") || b.is("main")) && c.is("("))
        return true;
    return false;
}


static void skipAfter(Tokenizer::const_iterator &iter, const Tokenizer::const_iterator &end, const std::string &type) {
    while(iter < end && !iter->is(type))
        ++iter;
    if(iter < end && iter->is(type))
        ++iter;
}


static void skipFloweyBody(Tokenizer::const_iterator &iter, const Tokenizer::const_iterator &end) {
    assert(iter < end && iter->is("{"));
    int cnt = 1;
    for(++iter; iter < end && cnt > 0; ++iter) {
        if(iter->is("{"))
            ++cnt;
        if(iter->is("}"))
            --cnt;
    }
}


static void parseAssignmentStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseCompound(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseCondition(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseConstCharDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseConstDesc(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseConstIntDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseDoWhileStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseExpression(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseFactor(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseForStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseFunction(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseIfStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseIntLiteral(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseInvolkExpression(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseInvolkStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseItem(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseMainFunc(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseParameters(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parsePrintStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseProgram(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseReturnStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseScanStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseSingleToken(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseVarArrayDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseVarCharDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseVarDesc(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
static void parseVarIntDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);


static void parseSingleToken(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    node.endIter = iter + 1;
    assert(iter < end && iter->is(node.nodeType.name));
}


static void parseConstCharDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    assert(iter < end && iter->is("const") && iter + 1 < end && iter[1].is("char"));
    node.valIter = iter + 1;
    iter += 2;
    do {
        if(iter >= end || !iter->is("Identifier")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) identifier here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        }
        node.newChild("Identifier", iter, end);
        if(iter + 1 >= end || !iter[1].is("=")) {
            Logger::getInstance().error(iter[1].getPos(), "expect a(n) `=' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        }
        if(iter + 2 >= end || !iter[2].is("CharLiteral")) {
            Logger::getInstance().error(iter[2].getPos(), "expect a(n) char literal here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        }
        {
            iter = node.newChild("CharLiteral", iter + 2, end).endIter;
        }
        if(iter < end && iter->is(",")) {
            ++iter;
            continue;
        }
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "char constant declaration ends unexpectedly");
        } else if(!iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        } else {
            ++iter;
        }
        break;
    } while(true);
    node.endIter = iter;
}


static void parseIntLiteral(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    assert(iter < end);
    if(iter->is("-")) {
        node.newChild("-", "negative", iter, end);
    }
    if(iter->is("+") || iter->is("-")) {
        ++iter;
    }
    if(iter < end && iter->is("UnsignedLiteral")) {
        node.valIter = iter;
        node.newChild("UnsignedLiteral", "unsigned", iter, end);
        UnsignedInfo *info = (UnsignedInfo *)&node.getChild("unsigned").valIter->getVal<UnsignedInfo>();
        if(node.hasChild("negative")) {
            if(info->v > 2147483648u) {
                if(!info->overflowed) {
                    Logger::getInstance().warn(iter->getPos(), "integer literal overflowed");
                    info->overflowed = true;
                }
                info->v = -2147483648;
            } else {
                info->v = -info->v;
            }
        } else {
            if(info->v > 2147483647u) {
                if(!info->overflowed) {
                    Logger::getInstance().warn(iter->getPos(), "integer literal overflowed");
                    info->overflowed = true;
                }
                info->v = 2147483647;
            }
        }
        ++iter;
    } else {
        Logger::getInstance().error(iter->getPos(), "expect a(n) unsigned literal here");
    }
    node.endIter = iter;
}


static void parseConstIntDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    assert(iter < end && iter->is("const") && iter + 1 < end && iter[1].is("int"));
    node.valIter = iter + 1;
    iter += 2;
    do {
        if(iter >= end || !iter->is("Identifier")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) identifier here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        }
        node.newChild("Identifier", iter, end);
        if(iter + 1 >= end || !iter[1].is("=")) {
            Logger::getInstance().error(iter[1].getPos(), "expect a(n) `=' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        }
        if(iter + 2 >= end || !(iter[2].is("+") || iter[2].is("-") || iter[2].is("UnsignedLiteral"))) {
            Logger::getInstance().error(iter[2].getPos(), "expect a(n) `+', `-' or integer here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("IntLiteral", iter + 2, end).endIter;
        if(iter < end && iter->is(",")) {
            ++iter;
            continue;
        }
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "integer constant declaration ends unexpectedly");
        } else if(!iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        } else {
            ++iter;
        }
        break;
    } while(true);
    node.endIter = iter;
}


static void parseConstDesc(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter < end && iter->is("const"));
    do {
        if(iter + 1 < end && iter[1].is("int")) {
            iter = node.newChild("ConstIntDecl", iter, end).endIter;
        } else if(iter + 1 < end && iter[1].is("char")) {
            iter = node.newChild("ConstCharDecl", iter, end).endIter;
        } else if(iter + 1 < end) {
            Logger::getInstance().error(iter[1].getPos(), "unknown constant type");
            skipAfter(iter, end, ";");
        } else {
            Logger::getInstance().error(iter->getPos(), "constant declaration ends unexpectedly");
            break;
        }
    } while(iter < end && iter->is("const"));
    node.endIter = iter;
}


static void parseVarIntDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter < end && iter->is("int"));
    ++iter;
    do {
        if(iter + 4 <= end && iter[0].is("Identifier") && iter[1].is("[") && iter[2].is("UnsignedLiteral") && iter[3].is("]")) {
            iter = node.newChild("VarArrayDecl", iter, end).endIter;
        } else if(iter < end && iter->is("Identifier")) {
            iter = node.newChild("Identifier", iter, end).endIter;
        } else {
            Logger::getInstance().error(iter->getPos(), "expect a(n) identifier here");
            skipAfter(iter, end, ";");
            return;
        }
        if(iter < end && iter->is(",")) {
            ++iter;
            continue;
        }
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "integer constant declaration ends unexpectedly");
        } else if(!iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        } else {
            ++iter;
        }
        break;
    } while(true);
    node.endIter = iter;
}


static void parseParameters(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = end;
    if(iter->is(")")) {
        node.endIter = iter;
        return;
    }
    do {
        if(iter + 2 <= end && (iter[0].is("int") || iter[0].is("char")) && iter[1].is("Identifier")) {
            node.newChild(iter[0].is("int") ? "IntTypename" : "CharTypename", iter, end);
            node.newChild("Identifier", iter + 1, end);
            if(iter + 2 < end && iter[2].is(",")) {
                iter += 3;
                continue;
            } else {
                iter += 2;
            }
        }
        if(!iter->is(")"))
            Logger::getInstance().error(iter->getPos(), "expect parameter(s) here");
        break;
    } while(true);
    node.endIter = iter;
}


static void parseVarCharDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter < end && iter->is("char"));
    ++iter;
    do {
        if(iter + 4 <= end && iter[0].is("Identifier") && iter[1].is("[") && iter[2].is("UnsignedLiteral") && iter[3].is("]")) {
            iter = node.newChild("VarArrayDecl", iter, end).endIter;
        } else if(iter < end && iter->is("Identifier")) {
            iter = node.newChild("Identifier", iter, end).endIter;
        } else {
            Logger::getInstance().error(iter->getPos(), "expect a(n) identifier here");
            skipAfter(iter, end, ";");
            return;
        }
        if(iter < end && iter->is(",")) {
            ++iter;
            continue;
        }
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "integer constant declaration ends unexpectedly");
        } else if(!iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        } else {
            ++iter;
        }
        break;
    } while(true);
    node.endIter = iter;
}


static void parseVarArrayDecl(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    assert(iter + 4 <= end && iter[0].is("Identifier") && iter[1].is("[") && iter[2].is("UnsignedLiteral") && iter[3].is("]"));
    node.newChild("Identifier", iter, end);
    node.newChild("UnsignedLiteral", iter + 2, end);
    node.startIter = iter;
    node.valIter = iter;
    node.endIter = iter + 4;
}


static void parseVarDesc(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter < end && (iter->is("int") || iter->is("char")));
    do {
        if(isFunc(iter, end))
            break;
        if(iter->is("int")) {
            iter = node.newChild("VarIntDecl", iter, end).endIter;
        } else if(iter->is("char")) {
            iter = node.newChild("VarCharDecl", iter, end).endIter;
        }
    } while(iter < end && (iter->is("int") || iter->is("char")));
    node.endIter = iter;
}


static void parseFunction(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter + 1;
    std::string retTypeName(node.nodeType.name);
    retTypeName.erase(retTypeName.find("Func"));
    retTypeName += "Typename";
    assert(iter < end && iter->is(retTypeName) && iter[1].is("Identifier") && iter[2].is("("));
    node.newChild("Identifier", "name", iter + 1, end);
    iter = node.newChild("Parameters", "parameters", iter + 3, end).endIter;
    if(iter >= end || !iter->is(")")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
        if(iter < end && iter->is("{"))
            skipFloweyBody(iter, end);
        else if(iter < end && iter->is(";"))
            ++iter;
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    if(iter >= end || !iter->is("{")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `{' here");
        if(iter < end && iter->is(";"))
            ++iter;
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "function stops here unexpectedly");
        node.endIter = iter;
        return;
    }
    iter = node.newChild("Compound", "compound", iter, end).endIter;
    if(iter >= end || !iter->is("}")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `}' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    node.endIter = iter;
}


static void parseCompound(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = end;
    if(iter < end && iter->is("const")) {
        iter = node.newChild("ConstDesc", "const", iter, end).endIter;
    }
    if(iter < end && (iter->is("int") || iter->is("char")) && !isFunc(iter, end)) {
        iter = node.newChild("VarDesc", "var", iter, end).endIter;
    }
    while(iter < end && !iter->is("}")) {
        iter = node.newChild("Statement", iter, end).endIter;
    }
    node.endIter = iter;
}


static void parseReturnStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("return"));
    if(iter + 1 < end && iter[1].is(";")) {
        node.endIter = iter + 2;
        return;
    }
    if(iter + 1 < end && iter[1].is("(")) {
        iter = node.newChild("Expression", "expression", iter + 2, end).endIter;
        if(iter >= end || !iter->is(")")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
        if(iter >= end || !iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    } else {
        Logger::getInstance().error(iter[1].getPos(), "expect a(n) expression surrounded by round brackets");
        skipAfter(iter, end, ";");
        node.endIter = iter;
        return;
    }
    node.endIter = iter;
}


static void parsePrintStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("printf"));
    if(iter + 1 < end && iter[1].is("(")) {
        if(iter + 2 < end && iter[2].is("StringLiteral")) {
            iter = node.newChild("StringLiteral", "string", iter + 2, end).endIter;
            if(iter + 1 < end && iter->is(",")) {
                ++iter;
                if(iter >= end) {
                    Logger::getInstance().error(iter->getPos(), "print ends unexpectedly");
                    node.endIter = iter;
                    return;
                }
                iter = node.newChild("Expression", "expression", iter, end).endIter;
            }
        } else if(iter + 2 < end) {
            iter = node.newChild("Expression", "expression", iter + 2, end).endIter;
        }
        if(iter >= end || !iter->is(")")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
        if(iter >= end || !iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    } else {
        Logger::getInstance().error(iter[1].getPos(), "expect arguments surrounded by round brackets");
        skipAfter(iter, end, ";");
        node.endIter = iter;
        return;
    }
    node.endIter = iter;
}


static void parseScanStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("scanf"));
    if(iter + 1 < end && iter[1].is("(")) {
        iter += 2;
        do {
            if(iter >= end || !iter->is("Identifier")) {
                Logger::getInstance().error(iter->getPos(), "expect a(n) identifier here");
                skipAfter(iter, end, ";");
                node.endIter = iter;
                return;
            }
            iter = node.newChild("Identifier", iter, end).endIter;
            if(iter < end && iter->is(",")) {
                ++iter;
                continue;
            }
            break;
        } while(true);
        if(iter >= end || !iter->is(")")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
        if(iter >= end || !iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    } else {
        Logger::getInstance().error(iter[1].getPos(), "expect arguments surrounded by round brackets");
        skipAfter(iter, end, ";");
        node.endIter = iter;
        return;
    }
    node.endIter = iter;
}


static void parseInvolkStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("Identifier"));
    node.newChild("Identifier", "funcName", iter, end);
    if(iter + 1 < end && iter[1].is("(")) {
        iter += 2;
        if(iter < end && iter->is(")")) {
            ++iter;
            if(iter >= end || !iter->is(";")) {
                Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
            } else {
                ++iter;
            }
            node.endIter = iter;
            return;
        }
        do {
            if(iter >= end) {
                Logger::getInstance().error(iter->getPos(), "involk ends unexpectedly");
                node.endIter = iter;
                return;
            }
            iter = node.newChild("Expression", iter, end).endIter;
            if(iter < end && iter->is(",")) {
                ++iter;
                continue;
            }
            break;
        } while(true);
        if(iter >= end || !iter->is(")")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
        if(iter >= end || !iter->is(";")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    } else {
        Logger::getInstance().error(iter[1].getPos(), "expect arguments surrounded by round brackets");
        skipAfter(iter, end, ";");
        node.endIter = iter;
        return;
    }
    node.endIter = iter;
}


static void parseInvolkExpression(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter + 1 < end && iter->is("Identifier") && iter[1].is("("));
    node.newChild("Identifier", "funcName", iter, end);
    iter += 2;
    do {
        if(iter < end && iter->is(")")) {
            ++iter;
            node.endIter = iter;
            return;
        }
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "involk ends unexpectedly");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("Expression", iter, end).endIter;
        if(iter < end && iter->is(",")) {
            ++iter;
            continue;
        }
        break;
    } while(true);
    if(iter >= end || !iter->is(")")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    node.endIter = iter;
}


static void parseDoWhileStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("do"));
    ++iter;
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "do-while ends unexpectedly");
        node.endIter = iter;
        return;
    }
    iter = node.newChild("Statement", "statement", iter, end).endIter;
    if(iter >= end || !iter->is("while")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) while here");
        skipAfter(iter, end, ")");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    if(iter >= end || !iter->is("(")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `(' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "do-while ends unexpectedly");
        node.endIter = iter;
        return;
    }
    iter = node.newChild("Condition", "condition", iter, end).endIter;
    if(iter >= end || !iter->is(")")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    node.endIter = iter;
}


static void parseForStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("for"));
    ++iter;
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
        node.endIter = iter;
        return;
    }
    if(iter >= end || !iter->is("(")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `(' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Identifier", "idA", iter, end).endIter;
    if(iter >= end || !iter->is("=")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `=' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Expression", "init", iter, end).endIter;
    if(iter >= end || !iter->is(";")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Condition", "condition", iter, end).endIter;
    if(iter >= end || !iter->is(";")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    if(iter >= end || !iter->is("Identifier")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) identifier here");
        node.endIter = iter;
        return;
    }
    iter = node.newChild("Identifier", "idB", iter, end).endIter;
    if(node.getChild("idB").valIter->str() != node.getChild("idA").valIter->str()) {
        Logger::getInstance().error(node.getChild("idB").startIter->getPos(), "expect the same identifier as previous one");
    }
    if(iter >= end || !iter->is("=")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `=' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Identifier", "idC", iter, end).endIter;
    if(node.getChild("idC").valIter->str() != node.getChild("idA").valIter->str() ||
            node.getChild("idC").valIter->str() != node.getChild("idB").valIter->str()) {
        Logger::getInstance().error(node.getChild("idC").startIter->getPos(), "expect the same identifier as previous one");
    }
    if(iter >= end || (!iter->is("+") && !iter->is("-"))) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `+' or `-' here");
        node.endIter = iter;
        return;
    } else {
        iter = node.newChild(iter->tokenType.indicator, "op", iter, end).endIter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("UnsignedLiteral", "step", iter, end).endIter;
    if(iter >= end || !iter->is(")")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "for ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Statement", "statement", iter, end).endIter;
    node.endIter = iter;
}


static void parseCondition(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    static const char *op[] = {"<", "<=", ">", ">=", "==", "!="};
    node.startIter = iter;
    node.valIter = iter;
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "condition ends unexpectedly");
        node.endIter = iter;
        return;
    }
    iter = node.newChild("Expression", "expressionA", iter, end).endIter;
    if(iter < end) {
        bool dual = false;
        for(unsigned i = 0; i < sizeof(op) / sizeof(const char *); ++i) {
            if(iter->is(op[i])) {
                iter = node.newChild(op[i], "op", iter, end).endIter;
                node.valIter = iter;
                dual = true;
                break;
            }
        }
        if(dual) {
            if(iter >= end) {
                Logger::getInstance().error(iter->getPos(), "condition ends unexpectedly");
                node.endIter = iter;
                return;
            }
            iter = node.newChild("Expression", "expressionB", iter, end).endIter;
        }
    }
    node.endIter = iter;
}


static void parseIfStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter->is("if"));
    ++iter;
    if(iter >= end || !iter->is("(")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `(' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "if ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Condition", "condition", iter, end).endIter;
    if(iter >= end || !iter->is(")")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "if ends unexpectedly");
            node.endIter = iter;
            return;
        }
    }
    iter = node.newChild("Statement", "statementA", iter, end).endIter;
    if(iter < end && iter->is("else")) {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "else ends unexpectedly");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("Statement", "statementB", iter, end).endIter;
    }
    node.endIter = iter;
}


static void parseAssignmentStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter < end && iter->is("Identifier"));
    iter = node.newChild("Identifier", "identifier", iter, end).endIter;
    if(iter < end && iter->is("[")) {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "assignment ends unexpectedly");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("Expression", "index", iter, end).endIter;
        if(iter >= end || !iter->is("]")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `]' here");
            skipAfter(iter, end, ";");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    }
    if(iter >= end || !iter->is("=")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `=' here");
        skipAfter(iter, end, ";");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    iter = node.newChild("Expression", "expression", iter, end).endIter;
    if(iter >= end || !iter->is(";")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    node.endIter = iter;
}


static void parseStatement(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = end;
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `;' here");
        node.endIter = iter;
        return;
    }
    if(iter->is(";")) {
        node.valIter = iter;
        ++iter;
    } else if(iter->is("if")) {
        iter = node.newChild("IfStatement", iter, end).endIter;
    } else if(iter->is("do")) {
        iter = node.newChild("DoWhileStatement", iter, end).endIter;
    } else if(iter->is("for")) {
        iter = node.newChild("ForStatement", iter, end).endIter;
    } else if(iter->is("return")) {
        iter = node.newChild("ReturnStatement", iter, end).endIter;
    } else if(iter->is("printf")) {
        iter = node.newChild("PrintStatement", iter, end).endIter;
    } else if(iter->is("scanf")) {
        iter = node.newChild("ScanStatement", iter, end).endIter;
    } else if(iter + 1 < end && iter->is("Identifier") && (iter[1].is("=") || iter[1].is("["))) {
        iter = node.newChild("AssignmentStatement", iter, end).endIter;
    } else if(iter + 1 < end && iter->is("Identifier") && iter[1].is("(")) {
        iter = node.newChild("InvolkStatement", iter, end).endIter;
    } else if(iter->is("{")) {
        ++iter;
        while(iter < end && !iter->is("}")) {
            iter = node.newChild("Statement", iter, end).endIter;
        }
        if(iter < end && iter->is("}"))
            ++iter;
        else
            Logger::getInstance().error(iter->getPos(), "expect a(n) `}' here");
    } else {
        Logger::getInstance().error(iter->getPos(), "expect a(n) valid statement here");
        while(iter < end && !iter->is(";") && !iter->is("}"))
            ++iter;
        if(iter < end && iter->is(";"))
            ++iter;
    }
    node.endIter = iter;
}


static void parseMainFunc(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter + 1;
    assert(iter < end && iter->is("void") && iter[1].is("main") && iter[2].is("("));
    node.newChild("main", "name", iter + 1, end);
    if(iter + 3 < end && !iter[3].is(")")) {
        Logger::getInstance().error(iter->getPos(), "main should not have parameters");
        iter = node.newChild("Parameters", iter + 3, end).endIter;
    } else {
        iter += 3;
    }
    if(iter >= end || !iter->is(")")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
        if(iter < end && iter->is("{"))
            skipFloweyBody(iter, end);
        else if(iter < end && iter->is(";"))
            ++iter;
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    if(iter >= end || !iter->is("{")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `{' here");
        if(iter < end && iter->is(";"))
            ++iter;
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "main function stops here unexpectedly");
        node.endIter = iter;
        return;
    }
    iter = node.newChild("Compound", "compound", iter, end).endIter;
    if(iter >= end || !iter->is("}")) {
        Logger::getInstance().error(iter->getPos(), "expect a(n) `}' here");
        node.endIter = iter;
        return;
    } else {
        ++iter;
    }
    node.endIter = iter;
}


static void parseExpression(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    assert(iter < end);
    if(iter->is("-")) {
        iter = node.newChild(iter->tokenType.indicator, "sign", iter, end).endIter;
    } else if(iter->is("+")) {
        ++iter;
    }
    do {
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "expression ends unexpectedly");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("Item", iter, end).endIter;
        if(iter < end && (iter->is("+") || iter->is("-"))) {
            iter = node.newChild(iter->tokenType.indicator, iter, end).endIter;
            continue;
        }
        break;
    } while(true);
    node.endIter = iter;
}


static void parseItem(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    do {
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "item ends unexpectedly");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("Factor", iter, end).endIter;
        if(iter < end && (iter->is("*") || iter->is("/"))) {
            iter = node.newChild(iter->tokenType.indicator, iter, end).endIter;
            continue;
        }
        break;
    } while(true);
    node.endIter = iter;
}


static void parseFactor(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = iter;
    if(iter >= end) {
        Logger::getInstance().error(iter->getPos(), "factor ends unexpectedly");
        node.endIter = iter;
        return;
    }
    if(iter->is("+") || iter->is("-") || iter->is("UnsignedLiteral")) {
        iter = node.newChild("IntLiteral", "int", iter, end).endIter;
    } else if(iter->is("CharLiteral")) {
        iter = node.newChild("CharLiteral", "char", iter, end).endIter;
    } else if(iter->is("(")) {
        ++iter;
        if(iter >= end) {
            Logger::getInstance().error(iter->getPos(), "factor ends unexpectedly");
            node.endIter = iter;
            return;
        }
        iter = node.newChild("Expression", "child", iter, end).endIter;
        if(iter >= end || !iter->is(")")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `)' here");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    } else if(iter + 1 < end && iter->is("Identifier") && iter[1].is("(")) {
        iter = node.newChild("InvolkExpression", "child", iter, end).endIter;
    } else if(iter + 1 < end && iter->is("Identifier") && iter[1].is("[")) {
        iter = node.newChild("Identifier", "identifier", iter, end).endIter;
        ++iter;
        iter = node.newChild("Expression", "index", iter, end).endIter;
        if(iter >= end || !iter->is("]")) {
            Logger::getInstance().error(iter->getPos(), "expect a(n) `]' here");
            node.endIter = iter;
            return;
        } else {
            ++iter;
        }
    } else if(iter->is("Identifier")) {
        iter = node.newChild("Identifier", "identifier", iter, end).endIter;
    } else {
        Logger::getInstance().error(iter->getPos(), "no valid factor is found");
        node.endIter = iter;
        return;
    }
    node.endIter = iter;
}


static void parseProgram(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
    node.startIter = iter;
    node.valIter = end;
    if(iter < end && iter->is("const")) {
        iter = node.newChild("ConstDesc", "const", iter, end).endIter;
    }
    if(iter < end && (iter->is("int") || iter->is("char")) && !isFunc(iter, end)) {
        iter = node.newChild("VarDesc", "var", iter, end).endIter;
    }
    while(iter < end && isFunc(iter, end)) {
        if(iter->is("void")) {
            if(iter[1].is("main")) {
                node.valIter = iter;
                iter = node.newChild("MainFunc", iter, end).endIter;
                break;
            } else {
                iter = node.newChild("VoidFunc", iter, end).endIter;
            }
        } else if(iter->is("int")) {
            iter = node.newChild("IntFunc", iter, end).endIter;
        } else if(iter->is("char")) {
            iter = node.newChild("CharFunc", iter, end).endIter;
        } else {
            assert(false);
        }
    }
    if(iter < end && node.valIter < end) {
        Logger::getInstance().error(iter->getPos(), "program should be ended here");
    } else if(node.valIter >= end && !Logger::getInstance().hasError) {
        Logger::getInstance().error(iter->getPos(), "main is not found");
    } else if(iter < end && !Logger::getInstance().hasError) {
        Logger::getInstance().error(iter->getPos(), "unexpected token");
    }
    node.endIter = iter;
}


const ASTNodeType nodeTypes[] = {
    {"!=", parseSingleToken},
    {"*", parseSingleToken},
    {"+", parseSingleToken},
    {"-", parseSingleToken},
    {"/", parseSingleToken},
    {"<", parseSingleToken},
    {"<=", parseSingleToken},
    {"==", parseSingleToken},
    {">", parseSingleToken},
    {">=", parseSingleToken},
    {"main", parseSingleToken},
    {"AssignmentStatement", parseAssignmentStatement},
    {"CharFunc", parseFunction},
    {"CharLiteral", parseSingleToken},
    {"CharTypename", parseSingleToken},
    {"Compound", parseCompound},
    {"Condition", parseCondition},
    {"ConstCharDecl", parseConstCharDecl},
    {"ConstDesc", parseConstDesc},
    {"ConstIntDecl", parseConstIntDecl},
    {"DoWhileStatement", parseDoWhileStatement},
    {"Expression", parseExpression},
    {"Factor", parseFactor},
    {"ForStatement", parseForStatement},
    {"Identifier", parseSingleToken},
    {"IfStatement", parseIfStatement},
    {"IntFunc", parseFunction},
    {"IntLiteral", parseIntLiteral},
    {"IntTypename", parseSingleToken},
    {"InvolkExpression", parseInvolkExpression},
    {"InvolkStatement", parseInvolkStatement},
    {"Item", parseItem},
    {"MainFunc", parseMainFunc},
    {"Parameters", parseParameters},
    {"PrintStatement", parsePrintStatement},
    {"Program", parseProgram},
    {"ReturnStatement", parseReturnStatement},
    {"ScanStatement", parseScanStatement},
    {"Statement", parseStatement},
    {"StringLiteral", parseSingleToken},
    {"UnsignedLiteral", parseSingleToken},
    {"VarArrayDecl", parseVarArrayDecl},
    {"VarCharDecl", parseVarCharDecl},
    {"VarDesc", parseVarDesc},
    {"VarIntDecl", parseVarIntDecl},
    {"VoidFunc", parseFunction},
};


const size_t numNodeTypes = sizeof(nodeTypes) / sizeof(ASTNodeType);


const ASTNodeType &fetchNodeType(const std::string &s) {
    static std::unordered_map<std::string, const ASTNodeType *> mp;
    if(mp.empty()) {
        for(size_t i = 0; i < numNodeTypes; ++i) {
            mp[nodeTypes[i].name] = &nodeTypes[i];
        }
    }
    if(mp.find(s) == mp.end()) {
        printf("%s\n", s.c_str());
    }
    assert(mp.find(s) != mp.end());
    return *mp[s];
}
