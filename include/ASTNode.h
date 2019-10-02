#ifndef ASTNODE_H
#define ASTNODE_H

#include "Tokenizer.h"

#include <vector>
#include <map>
#include <string>
#include <iostream>


class ASTNode;


struct ASTNodeType {
    const char *name;
    void (*parse)(ASTNode &node, Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end);
};


extern const ASTNodeType nodeTypes[];
extern const size_t numNodeTypes;

const ASTNodeType &fetchNodeType(const std::string &s);


class ASTNode {
    friend class Parser;

protected:
    std::vector<ASTNode> children;
    std::map<std::string, size_t> semantical;

    void parse(Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
        startIter = endIter = iter;
        valIter = end;
        nodeType.parse(*this, iter, end);
#if 0
        auto reprRange = [] (const Tokenizer::const_iterator &begin, const Tokenizer::const_iterator &end) {
            auto f = [] (const Tokenizer::const_iterator &iter) {
                return std::string("(line ") + std::to_string(iter->getPos().getLineNumber() + 1) +
                    std::string(", column ") + std::to_string(iter->getPos().getColumnNumber() + 1) + std::string(")");
            };
            auto g = [] (const Tokenizer::const_iterator &iter) {
                return std::string("(line ") + std::to_string(iter->getPos().getLineNumber() + 1) +
                    std::string(", column ") + std::to_string(iter->getPos().getColumnNumber()) + std::string(")");
            };
            return f(begin) + std::string(" to ") + g(end);
        };
        std::cout << reprRange(startIter, endIter) + " is a(n) / are " + nodeType.name + "(s)" << std::endl;
#endif
    }

public:
    ASTNode(const std::string &type, Tokenizer::const_iterator iter,
            const Tokenizer::const_iterator &end): nodeType(fetchNodeType(type)) {
        parse(iter, end);
    }

    const ASTNodeType &nodeType;
    Tokenizer::const_iterator startIter, endIter, valIter;

    bool is(const std::string &s) const {
        return s == nodeType.name;
    }

    ASTNode &newChild(const std::string &t,
            Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
        children.emplace_back(ASTNode(t, iter, end));
        return children.back();
    }

    ASTNode &newChild(const std::string &t, const std::string &s,
            Tokenizer::const_iterator iter, const Tokenizer::const_iterator &end) {
        children.emplace_back(ASTNode(t, iter, end));
        semantical[s] = children.size() - 1;
        return children.back();
    }

    const ASTNode &getChild(const std::string &s) const {
        auto iter = semantical.find(s);
        assert(iter != semantical.end());
        return children[iter->second];
    }

    bool hasChild(const std::string &s) const {
        auto iter = semantical.find(s);
        return iter != semantical.end();
    }

    using const_iterator = std::vector<ASTNode>::const_iterator;

    const_iterator begin() const {
        return children.begin();
    }

    const_iterator end() const {
        return children.end();
    }

    const std::vector<ASTNode> &getChildren() const {
        return children;
    }

    const ASTNode &operator[](const size_t &k) const {
        return children[k];
    }
};


#endif // ASTNODE_H
