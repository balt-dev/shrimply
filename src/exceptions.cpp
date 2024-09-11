

#include "exceptions.h"
#include "lexer.h"
#include "runtime.h"

using namespace exceptions;

SyntaxError SyntaxError::unexpectedToken(const lexer::Token &token, parsing::ParserState state) {
    return {"unexpected token [" + token.span() + "]", token.getPosition() };
}

SyntaxError SyntaxError::unexpectedToken(const lexer::Token &token, parsing::ParserState state, const lexer::TokenType expected) {
    return {"unexpected token [" + token.span() + "] (expected " + expected.to_string() + ")", token.getPosition() };
}

SyntaxError SyntaxError::invalidToken(const lexer::Token &token, const std::string& why) {
    return {"failed to parse token [" + token.span() + "]: " + why, token.getPosition() };
}

RuntimeError::RuntimeError(const runtime::Stackframe &frame, std::string msg) : message(std::move(msg)) {
    std::ostringstream ss;
    ss << "runtime error: " << message << std::endl
        << "backtrace:" << std::endl;

    auto currentFrame = &frame;
    while (currentFrame) {
        ss << "    " << currentFrame->sourcePos.to_string() << " in " << currentFrame->functionName << std::endl;
        currentFrame = currentFrame->parent;
    }

    formatted = ss.str();
}
