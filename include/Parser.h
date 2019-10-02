#ifndef PARSER_H
#define PARSER_H

#include "Tokenizer.h"
#include "ASTNode.h"

class Parser
{
public:
    Parser(const Tokenizer &_tokenizer):
            tokenizer(_tokenizer), root("Program", tokenizer.begin(), tokenizer.end()) {}

    const ASTNode &getRoot() const {
        return root;
    }
protected:
    const Tokenizer &tokenizer;
    ASTNode root;
};

#endif // PARSER_H
