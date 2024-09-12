#include <filesystem>
#include <utility>

#include "exceptions.h"
#include "lexer.h"
#include "runtime.h"


using namespace exceptions;

SyntaxError SyntaxError::unexpectedToken(const lexer::Token &token, parsing::ParserState state, std::filesystem::path filename) {
    return {"unexpected token [" + token.span() + "]", token.getPosition(), std::move(filename) };
}

SyntaxError SyntaxError::unexpectedToken(const lexer::Token &token, parsing::ParserState state, const lexer::TokenType expected, std::filesystem::path filename) {
    return {"unexpected token [" + token.span() + "] (expected " + expected.to_string() + ")", token.getPosition(), std::move(filename) };
}

SyntaxError SyntaxError::invalidToken(const lexer::Token &token, const std::string& why, std::filesystem::path filename) {
    return {"failed to parse token [" + token.span() + "]: " + why, token.getPosition(), std::move(filename) };
}

RuntimeError::RuntimeError(const runtime::Stackframe &frame, std::string msg) : message(std::move(msg)) {
    std::ostringstream ss;
    ss << "runtime error: " << message << std::endl
        << "backtrace:" << std::endl;

    auto currentFrame = &frame;
    while (currentFrame) {
        ss << "    " << currentFrame->sourcePos.to_string()
            << " in " << currentFrame->functionName
            << " (module " << currentFrame->root->moduleName << ")"
            << std::endl;
        currentFrame = currentFrame->parent;
    }

    formatted = ss.str();
}
