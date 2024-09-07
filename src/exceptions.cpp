#include "exceptions.h"
#include "lexer.h"

using namespace exceptions;

SyntaxError SyntaxError::unexpectedToken(const lexer::Token &token) {
    return {"unexpected token: " + token.to_string(), token.getPosition()};
}
