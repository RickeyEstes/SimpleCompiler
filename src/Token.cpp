#include "Token.h"
#include "Logger.h"

#include <unordered_map>
#include <cstring>

static void parseUnsigned(const SourceCode::const_iterator &iter, size_t stride, void *&_res);
static void parseChar(const SourceCode::const_iterator &iter, size_t stride, void *&_res);
static void parseString(const SourceCode::const_iterator &iter, size_t stride, void *&_res);
static std::string dumpUnsigned(const void *_p);
static std::string dumpChar(const void *_p);
static std::string dumpString(const void *_p);

const TokenType tokenTypes[] = {
    {"Identifier", nullptr, nullptr, nullptr},
    {"UnsignedLiteral", nullptr, parseUnsigned, dumpUnsigned},
    {"CharLiteral", nullptr, parseChar, dumpChar},
    {"StringLiteral", nullptr, parseString, dumpString},
    {"Plus", "+", nullptr, nullptr},
    {"Minus", "-", nullptr, nullptr},
    {"Multiplication", "*", nullptr, nullptr},
    {"Division", "/", nullptr, nullptr},
    {"Assignment", "=", nullptr, nullptr},
    {"Less", "<", nullptr, nullptr},
    {"LessOrEqual", "<=", nullptr, nullptr},
    {"Greater", ">", nullptr, nullptr},
    {"GreaterOrEqual", ">=", nullptr, nullptr},
    {"Equal", "==", nullptr, nullptr},
    {"NotEqual", "!=", nullptr, nullptr},
    {"Comma", ",", nullptr, nullptr},
    {"SemiColon", ";", nullptr, nullptr},
    {"LeftRoundBracket", "(", nullptr, nullptr},
    {"RightRoundBracket", ")", nullptr, nullptr},
    {"LeftSquareBracket", "[", nullptr, nullptr},
    {"RightSquareBracket", "]", nullptr, nullptr},
    {"LeftCurlyBracket", "{", nullptr, nullptr},
    {"RightCurlyBracket", "}", nullptr, nullptr},
    {"If", "if", nullptr, nullptr},
    {"Else", "else", nullptr, nullptr},
    {"Do", "do", nullptr, nullptr},
    {"While", "while", nullptr, nullptr},
    {"For", "for", nullptr, nullptr},
    {"ConstDeclare", "const", nullptr, nullptr},
    {"IntTypename", "int", nullptr, nullptr},
    {"CharTypename", "char", nullptr, nullptr},
    {"VoidTypename", "void", nullptr, nullptr},
    {"MainFunction", "main", nullptr, nullptr},
    {"Return", "return", nullptr, nullptr},
    {"Print", "printf", nullptr, nullptr},
    {"Scan", "scanf", nullptr, nullptr},
    {"EOF", nullptr, nullptr, nullptr},
};

const size_t numTokenTypes = sizeof(tokenTypes) / sizeof(TokenType);

const TokenType &fetchTokenType(const std::string &s) {
    static std::unordered_map<std::string, const TokenType *> mp;
    if(mp.empty()) {
        for(size_t i = 0; i < numTokenTypes; ++i) {
            mp[tokenTypes[i].name] = &tokenTypes[i];
            if(tokenTypes[i].indicator)
                mp[tokenTypes[i].indicator] = &tokenTypes[i];
        }
    }
    assert(mp.find(s) != mp.end());
    return *mp[s];
}

static bool match(const SourceCode::const_iterator &iter, const char *s) {
    SourceCode::const_iterator it(iter);
    for(; *s && it.good(); ++s, ++it) {
        if(*s != *it)
            return false;
    }
    return true;
}

const TokenType *indicateTokenType(const SourceCode::const_iterator &iter) {
    // TODO: use Trie
    const TokenType *res = nullptr;
    for(size_t i = 0; i < numTokenTypes; ++i)
    if(tokenTypes[i].indicator) {
        const TokenType &tt = tokenTypes[i];
        if(match(iter, tt.indicator) && (!res || strlen(res->indicator) < strlen(tt.indicator))) {
            res = &tt;
        }
    }
    return res;
}

size_t getTokenTypeId(const TokenType &k) {
    return &k - tokenTypes;
}

static void parseUnsigned(const SourceCode::const_iterator &iter, size_t stride, void *&_res) {
    std::string s = iter.getSlice(stride);
    _res = malloc(sizeof(UnsignedInfo));
    UnsignedInfo *res = (UnsignedInfo *)_res;
    res->v = 0;
    res->overflowed = false;
    for(const auto &c : s) {
        if(res->v > (std::numeric_limits<uint32_t>::max() - (c - '0')) / 10u) {
            res->overflowed = true;
            res->v = std::numeric_limits<uint32_t>::max();
            Logger::getInstance().warn(iter, "unsigned integer literal overflowed");
            break;
        }
        res->v = res->v * 10u + (c - '0');
    }
}

static std::string dumpUnsigned(const void *_p) {
    assert(_p);
    if(((const UnsignedInfo *)_p)->overflowed)
        return "OVERFLOWED";
    return std::to_string(((const UnsignedInfo *)_p)->v);
}

static void parseChar(const SourceCode::const_iterator &iter, size_t stride, void *&_res) {
    std::string s = iter.getSlice(stride);
    _res = malloc(sizeof(char));
    char *res = (char *)_res;
    assert(s.size() >= 3 && s.front() == '\'' && s.back() == '\'');
    if(s.size() != 3) {
        Logger::getInstance().error(iter, "invalid char literal");
        *res = s[1];
    } else {
        *res = s[1];
        SourceCode::const_iterator it(iter);
        ++it;
        if(*res != '+' && *res != '-' && *res != '*' && *res != '/' && *res != '_' && !isalnum(*res))
            Logger::getInstance().error(it, "char literal is not acceptable");
    }
}

static std::string dumpChar(const void *_p) {
    assert(_p);
    return std::string("'") + std::string(1, *(const char *)_p) + std::string("'");
}

static void parseString(const SourceCode::const_iterator &iter, size_t stride, void *&_res) {
    std::string s = iter.getSlice(stride);
    assert(s.size() >= 2 && s.front() == '"' && s.back() == '"');
    _res = malloc(sizeof(char) * (s.size() - 2 + 1));
    char *res = (char *)_res;
    strcpy(res, s.substr(1, s.size() - 2).c_str());

    SourceCode::const_iterator it(iter);
    ++it;
    for(size_t i = 0; i < s.size() - 2; ++i) {
        if(res[i] != 32 && res[i] != 33 && !(35 <= res[i] && res[i] <= 126)) {
            Logger::getInstance().error(it, "invalid element in string literal");
            break;
        }
        ++it;
    }
}

static std::string dumpString(const void *_p) {
    assert(_p);
    return std::string("\"") + std::string((const char *)_p) + std::string("\"");
}
