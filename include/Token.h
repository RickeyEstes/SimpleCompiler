#ifndef TOKEN_H
#define TOKEN_H

#include "SourceCode.h"
#include "Logger.h"

#include <cctype>
#include <string>
#include <vector>


struct TokenType {
    const char *name, *indicator;
    void (*parse)(const SourceCode::const_iterator &iter, size_t stride, void *&res);
    std::string (*dumpVal)(const void *val);
};

extern const TokenType tokenTypes[];
extern const size_t numTokenTypes;

const TokenType &fetchTokenType(const std::string &s);
const TokenType *indicateTokenType(const SourceCode::const_iterator &iter);
size_t getTokenTypeId(const TokenType &k);

class Token {
    friend class Tokenizer;

public:
    const TokenType &tokenType;
protected:
    void *val;

public:
    SourceCode::const_iterator srcPos;
    size_t srcStride;
    std::string sliceStr;

    template<typename T>
    const T &getVal() const {
        return *(const T *)val;
    }

    bool is(const TokenType &t) const {
        return &t == &tokenType;
    }

    bool is(const std::string &s) const {
        return is(fetchTokenType(s));
    }

    const SourceCode::const_iterator &getPos() const {
        return srcPos;
    }

    Token(const TokenType &type, const SourceCode::const_iterator &iter, size_t stride):
            tokenType(type), srcPos(iter), srcStride(stride),
            sliceStr(srcPos.getSlice(srcStride)) {
        if(type.indicator) {
            if(sliceStr != type.indicator) {
                Logger::getInstance().error(iter, "fuck me");
            }
            assert(sliceStr == type.indicator);
        }
        if(type.parse)
            type.parse(iter, stride, val);
        else
            val = nullptr;
    }

    std::string str() const {
        return sliceStr;
    }

    std::string dump() const {
        assert(tokenType.name);
        std::string repr;
        if(tokenType.indicator)
            repr = tokenType.indicator;
        else if(tokenType.dumpVal)
            repr = tokenType.dumpVal(val);
        else
            repr = sliceStr;
        return std::to_string(getTokenTypeId(tokenType)) + " " +
            std::string(tokenType.name) + " " + repr;
    }
};


struct UnsignedInfo {
    uint32_t v;
    bool overflowed;
};


#endif // TOKEN_H
