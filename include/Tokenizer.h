#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "SourceCode.h"
#include "Token.h"

#include <vector>
#include <string>
#include <istream>

class Tokenizer {
public:
    Tokenizer(const SourceCode &source): src(source) {
        parse();
    }

    ~Tokenizer() {
        for(auto &token : tokens) {
            if(token.val) {
                free(token.val);
                token.val = nullptr;
            }
        }
    }

    using const_iterator = std::vector<Token>::const_iterator;

    const_iterator begin() const {
        return tokens.begin();
    }

    const_iterator end() const {
        return tokens.end() - 1;
    }

    const std::vector<Token> &getTokens() const {
        return tokens;
    }

    const SourceCode &getSourceCode() const {
        return src;
    }
protected:
    const SourceCode &src;
    std::vector<Token> tokens;

    void parse();
    void addToken(const TokenType &type, const SourceCode::const_iterator &it, size_t stride) {
        tokens.emplace_back(Token(type, it, stride));
    }
};

#endif // TOKENIZER_H
