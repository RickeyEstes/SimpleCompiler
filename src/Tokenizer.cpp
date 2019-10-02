#include "Tokenizer.h"
#include "Token.h"
#include "Logger.h"

#include <cstring>
#include <cctype>

void Tokenizer::parse() {
    SourceCode::const_iterator it = src.begin();
    while(it != src.end()) {
        if(isspace(*it)) {
            ++it;
        } else if(*it == '_' || isalpha(*it)) {
            auto j = it;
            ++j;
            size_t stride = 1;
            while(j != src.end() && (*j == '_' || isalnum(*j))) {
                ++j;
                ++stride;
            }
            const TokenType *type = indicateTokenType(it);
            if(type && strlen(type->indicator) == stride) {
                addToken(*type, it, stride);
            } else {
                addToken(fetchTokenType("Identifier"), it, stride);
            }
            it = j;
        } else if(isdigit(*it)) {
            auto j = it;
            ++j;
            size_t stride = 1;
            while(j != src.end() && isdigit(*j)) {
                ++j;
                ++stride;
            }
            addToken(fetchTokenType("UnsignedLiteral"), it, stride);
            it = j;
        } else if(*it == '\'') {
            auto j = it;
            ++j;
            size_t stride = 1;
            while(j != src.end() && *j != '\n' && *j != '\'') {
                ++j;
                ++stride;
            }
            if(j == src.end() || *j == '\n') {
                Logger::getInstance().error(j, "expected a(n) `'`");
            } else {
                ++j;
                ++stride;
                addToken(fetchTokenType("CharLiteral"), it, stride);
            }
            it = j;
        } else if(*it == '"') {
            auto j = it;
            ++j;
            size_t stride = 1;
            while(j != src.end() && *j != '\n' && *j != '"') {
                ++j;
                ++stride;
            }
            if(j == src.end() || *j == '\n') {
                Logger::getInstance().error(j, "expected a(n) `\"`");
            } else {
                ++j;
                ++stride;
                addToken(fetchTokenType("StringLiteral"), it, stride);
            }
            it = j;
        } else {
            const TokenType *type = indicateTokenType(it);
            if(type) {
                size_t stride = strlen(type->indicator);
                addToken(*type, it, stride);
                for(size_t j = 0; j < stride; ++j)
                    ++it;
            } else {
                Logger::getInstance().fatal(it, "unknown symbol");
                return;
                ++it;
            }
        }
    }
    addToken(fetchTokenType("EOF"), it, 0);
}
