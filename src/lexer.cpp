#include "lexer.h"

#include <iostream>
#include "exceptions.h"

using namespace lexer;

std::string TokenType::to_string() const {
    switch (value) {
        case KW_FUNCTION: return "Function";
        case KW_LET: return "Let";
        case KW_IF: return "If";
        case KW_ELSE: return "Else";
        case KW_LOOP: return "Loop";
        case KW_BREAK: return "Break";
        case KW_CONTINUE: return "Continue";
        case KW_RETURN: return "Return";
        case KW_TRUE: return "True";
        case KW_FALSE: return "False";
        case KW_NULL: return "Null";
        case PUNC_SEMICOLON: return "Semicolon";
        case PUNC_PLUS: return "Plus";
        case PUNC_MINUS: return "Minus";
        case PUNC_MULT: return "Mult";
        case PUNC_DIV: return "Div";
        case PUNC_MOD: return "Mod";
        case PUNC_DOT: return "Dot";
        case PUNC_COMMA: return "Comma";
        case PUNC_AND: return "And";
        case PUNC_OR: return "Or";
        case PUNC_EQ: return "Eq";
        case PUNC_NEQ: return "Neq";
        case PUNC_LEQ: return "Leq";
        case PUNC_GEQ: return "Geq";
        case PUNC_ASSIGN: return "Assign";
        case PUNC_BITAND: return "BitAnd";
        case PUNC_BITOR: return "BitOr";
        case PUNC_XOR: return "Xor";
        case PUNC_NOT: return "Not";
        case PUNC_L_PAREN: return "LParen";
        case PUNC_R_PAREN: return "RParen";
        case PUNC_L_BRACKET: return "LBracket";
        case PUNC_R_BRACKET: return "RBracket";
        case PUNC_L_BRACE: return "LBrace";
        case PUNC_R_BRACE: return "RBrace";
        case PUNC_L_ANGLE: return "LAngle";
        case PUNC_R_ANGLE: return "RAngle";
        case LIT_HEX_NUMBER: return "HexNumber";
        case LIT_BIN_NUMBER: return "BinNumber";
        case LIT_OCT_NUMBER: return "OctNumber";
        case LIT_DEC_NUMBER: return "DecNumber";
        case LIT_STRING: return "String";
        case COMMENT: return "Comment";
        case IDENTIFIER: return "Identifier";
        default: return "<UNKNOWN>";
    }
}

std::string Token::to_string() const {
    return type.to_string() + "(\"" + parent->substr(start, end - start) + "\")";
}

exceptions::FilePosition Token::getPosition() const {
    auto pos = exceptions::FilePosition { 1, 1 };
    for (size_t i = 0; i < std::min(start, parent->size()); i++) {
        if (parent->c_str()[i] == '\n') {
            pos.line++;
            pos.column = 1;
        } else
            pos.column++;
    }
    return pos;
}

bool Lexer::chompString(const std::string &needle, Token & token) {
    if (index + needle.size() > dataLength) { return false; }
    if (rawData.substr(index, needle.size()) == needle) {
        token.start = index;
        index += needle.size();
        token.end = index;
        return true;
    }
    return false;
}

void Lexer::skipWhitespace() {
    while (index <= dataLength) {
        // Get the current character, and check if it's whitespace
        const char current = byteAt(index);
        if (!isspace(current)) return;
        index++;
    }
}

Lexer::Lexer (std::string & data) : rawData(data), dataLength(data.size()) {
    // Check that the file is actually ASCII
    for (char chr : data) {
        if (!isascii(chr)) {
            throw std::invalid_argument("file must be pure ASCII");
        }
    }
    // Skip past initial whitespace in the file
    skipWhitespace();
}

/// Tries to match a string, and if it matches, sets the token type and returns true.
#define IF_CHOMP_RETURN(needle, ty) if ( chompString(needle, token) ) { \
        token.type = ty; \
        return true; \
    }

bool Lexer::advanceToken(Token & token) {
    skipWhitespace();

    token.start = index;
    token.end = index;
    if (atEnd()) { return false; }

    token.parent = &rawData;
    token.type = TokenType::UNRECOGNIZED;

    // Match against a map of predetermined strings to token types.

    // Keywords
    IF_CHOMP_RETURN("fn", TokenType::KW_FUNCTION);
    IF_CHOMP_RETURN("let", TokenType::KW_LET);
    IF_CHOMP_RETURN("if", TokenType::KW_IF);
    IF_CHOMP_RETURN("else", TokenType::KW_ELSE);
    IF_CHOMP_RETURN("loop", TokenType::KW_LOOP);
    IF_CHOMP_RETURN("break", TokenType::KW_BREAK);
    IF_CHOMP_RETURN("continue", TokenType::KW_CONTINUE);
    IF_CHOMP_RETURN("return", TokenType::KW_RETURN);
    IF_CHOMP_RETURN("true", TokenType::KW_TRUE);
    IF_CHOMP_RETURN("false", TokenType::KW_FALSE);

    // Match comments before we match punctuation
    if ( chompString("/*", token) ) {
        token.type = TokenType::COMMENT;
        // Save the start, since it gets overwritten by chompString
        auto start = token.start;
        while ( !chompString("*/", token) ) {
            if ( index + 2 >= dataLength ) {
                throw exceptions::SyntaxError::unexpectedEOF(token.getPosition());
            }
            index++;
        }
        token.start = start;
        token.end = index;
        return true;
    }

    // Punctuation
    IF_CHOMP_RETURN(";", TokenType::PUNC_SEMICOLON);
    IF_CHOMP_RETURN("+", TokenType::PUNC_PLUS);
    IF_CHOMP_RETURN("-", TokenType::PUNC_MINUS);
    IF_CHOMP_RETURN("*", TokenType::PUNC_MULT);
    IF_CHOMP_RETURN("/", TokenType::PUNC_DIV);
    IF_CHOMP_RETURN("%", TokenType::PUNC_MOD);
    IF_CHOMP_RETURN(".", TokenType::PUNC_DOT);
    IF_CHOMP_RETURN(",", TokenType::PUNC_COMMA);
    IF_CHOMP_RETURN("&&", TokenType::PUNC_AND);
    IF_CHOMP_RETURN("||", TokenType::PUNC_OR);
    IF_CHOMP_RETURN("==", TokenType::PUNC_EQ);
    IF_CHOMP_RETURN("!=", TokenType::PUNC_NEQ);
    IF_CHOMP_RETURN("<=", TokenType::PUNC_LEQ);
    IF_CHOMP_RETURN(">=", TokenType::PUNC_GEQ);
    IF_CHOMP_RETURN("=", TokenType::PUNC_ASSIGN);
    IF_CHOMP_RETURN("&", TokenType::PUNC_BITAND);
    IF_CHOMP_RETURN("|", TokenType::PUNC_BITOR);
    IF_CHOMP_RETURN("^", TokenType::PUNC_XOR);
    IF_CHOMP_RETURN("!", TokenType::PUNC_NOT);
    IF_CHOMP_RETURN("(", TokenType::PUNC_L_PAREN);
    IF_CHOMP_RETURN(")", TokenType::PUNC_R_PAREN);
    IF_CHOMP_RETURN("[", TokenType::PUNC_L_BRACKET);
    IF_CHOMP_RETURN("]", TokenType::PUNC_R_BRACKET);
    IF_CHOMP_RETURN("{", TokenType::PUNC_L_BRACE);
    IF_CHOMP_RETURN("}", TokenType::PUNC_R_BRACE);
    IF_CHOMP_RETURN("<", TokenType::PUNC_L_ANGLE);
    IF_CHOMP_RETURN(">", TokenType::PUNC_R_ANGLE);

    // Match against a hexadecimal number
    if ( chompString("0x", token) ) {
        token.type = TokenType::LIT_HEX_NUMBER;

        while ( !atEnd() && isxdigit(byteAt(index)))
            index++;
        token.end = index;
        return true;
    }

    // Match against a binary number
    if ( chompString("0b", token) ) {
        token.type = TokenType::LIT_BIN_NUMBER;

        while ( !atEnd() && (byteAt(index) == '0' || byteAt(index) == '1'))
            index++;
        token.end = index;
        return true;
    }

    // Match against an octal number
    if ( chompString("0o", token) ) {
        token.type = TokenType::LIT_OCT_NUMBER;

        while ( !atEnd() && byteAt(index) >= '0' && byteAt(index) <= '7')
            index++;
        token.end = index;
        return true;
    }

    auto currentByte = byteAt(index);

    // Match against a decimal number (optionally, with a point)
    if ( isdigit(currentByte) ) {

        token.type = TokenType::LIT_DEC_NUMBER;
        bool foundDecimalPoint = false;

        while (isdigit(byteAt(index)) || (!foundDecimalPoint && byteAt(index) == '.')) {
            foundDecimalPoint |= byteAt(index) == '.';
            index++; token.end++;
            if (atEnd()) return true;
        }
        return true;
    }

    // Match a string
    if ( currentByte == '"' ) {
        token.type = TokenType::LIT_STRING;
        bool lastWasEscape = false;
        index++; // Advance past the starting quote
        if (atEnd()) throw exceptions::SyntaxError::unexpectedEOF(token.getPosition());
        while ( lastWasEscape || byteAt(index) != '\"' ) {
            lastWasEscape = byteAt(index) == '\\';
            index++; // Advance past the inner character
            if (atEnd()) throw exceptions::SyntaxError::unexpectedEOF(token.getPosition());
        }
        index++; // Advance past the ending quote
        token.end = index;
        return true;
    }

    // Match an identifier
    if ( isalpha(currentByte) || currentByte == '_' ) {
        token.type = TokenType::IDENTIFIER;
        while ( !atEnd() ) {
            auto const thisByte = byteAt(index);
            if ( !(isalnum(thisByte) || thisByte == '_') )
                break;
            index++;
        }
        token.end = index;
        return true;
    }

    throw exceptions::SyntaxError(
        "unrecognized token", token.getPosition()
    );
}