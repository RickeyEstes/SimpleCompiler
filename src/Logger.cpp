#include "Logger.h"
#include "Token.h"
#include "Tokenizer.h"
#include "ASTNode.h"

#include <cassert>
#include <iostream>

template<>
void Logger::echo(const std::string &prefix, const SourceCode::const_iterator &pos, const std::string &prompt) {
    ss << prefix << " at line " << pos.getLineNumber() + 1 << " column " << pos.getColumnNumber() + 1 << ":" << std::endl;
    ss << pos.getCurrentLine() << std::endl;
    ss << std::string(pos.getColumnNumber(), ' ') << '^' << std::endl;
    ss << prompt << std::endl << std::endl;
}

template<>
void Logger::echo(const std::string &prefix, const Token &pos, const std::string &prompt) {
    echo(prefix, pos.getPos(), prompt);
}

template<>
void Logger::echo(const std::string &prefix, const Tokenizer::const_iterator &pos, const std::string &prompt) {
    echo(prefix, pos->getPos(), prompt);
}

template<>
void Logger::echo(const std::string &prefix, const ASTNode &pos, const std::string &prompt) {
    echo(prefix, pos.startIter, prompt);
}
