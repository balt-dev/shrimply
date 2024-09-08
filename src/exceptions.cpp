#include "exceptions.h"
#include "lexer.h"

using namespace exceptions;

SyntaxError SyntaxError::unexpectedToken(const lexer::Token &token, parsing::ParserState state) {
    std::ostringstream stream;
    stream << "unexpected token in parser state " << (int) state << ": " << token.to_string();
    return {stream.str(), token.getPosition()};
}
