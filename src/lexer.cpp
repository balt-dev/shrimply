#include "lexer.h"

#include <iostream>
#include <utility>
#include "exceptions.h"

using namespace lexer;

std::string TokenType::to_string() const {
    switch (value) {
        case KW_FUNCTION: return "fn";
        case KW_IF: return "if";
        case KW_ELSE: return "else";
        case KW_LOOP: return "loop";
        case KW_BREAK: return "break";
        case KW_CONTINUE: return "continue";
        case KW_RETURN: return "return";
        case KW_TRUE: return "true";
        case KW_FALSE: return "false";
        case KW_NULL: return "null";
        case KW_TRY: return "try";
        case KW_RECOVER: return "recover";
        case PUNC_SEMICOLON: return ";";
        case PUNC_SCOPE: return "::";
        case PUNC_CALL: return "$";
        case PUNC_PLUS: return "+";
        case PUNC_MINUS: return "-";
        case PUNC_MULT: return "*";
        case PUNC_DIV: return "/";
        case PUNC_MOD: return "%";
        case PUNC_INDEX: return ".";
        case PUNC_COMMA: return ",";
        case PUNC_AND: return "&&";
        case PUNC_OR: return "||";
        case PUNC_DOUBLE_EQ: return "==";
        case PUNC_NEQ: return "!=";
        case PUNC_LEQ: return "<=";
        case PUNC_GEQ: return ">=";
        case PUNC_EQ: return "=";
        case PUNC_AMPERSAND: return "&";
        case PUNC_BITOR: return "|";
        case PUNC_XOR: return "^";
        case PUNC_NOT: return "!";
        case PUNC_L_BRACKET: return "[";
        case PUNC_R_BRACKET: return "]";
        case PUNC_L_PAREN: return "(";
        case PUNC_R_PAREN: return ")";
        case PUNC_L_BRACE: return "{";
        case PUNC_R_BRACE: return "}";
        case PUNC_LT: return "<";
        case PUNC_GT: return ">";
        case LIT_HEX_NUMBER: return "16#";
        case LIT_BIN_NUMBER: return "2#";
        case LIT_OCT_NUMBER: return "8#";
        case LIT_DEC_NUMBER: return "10#";
        case LIT_STRING: return "String";
        case COMMENT: return "Comment";
        case IDENTIFIER: return "Identifier";
        case PUNC_DECLARATION: return ":=";
        case END_OF_FILE: return "<EOF>";
        default: return "???";
    }
}

std::string Token::span() const {
    if (!parent)
        throw exceptions::SyntaxError(
            "internal error: tried to convert a token with a nullptr parent to a string",
            position, "???"
        );
    return parent->substr(start, end - start);
}

std::string Token::display() const {
    return type.to_string() + "(\"" + span() + "\")";
}

bool Lexer::chompString(const std::string &needle, Token & token) {
    if (index + needle.size() > dataLength) { return false; }
    if (rawData.substr(index, needle.size()) == needle) {
        token.start = index;
        index += needle.size() - 1;
        incrementPosition();
        token.end = index;
        return true;
    }
    return false;
}

void Lexer::skipWhitespace() {
    while (index <= dataLength) {
        // Get the current character, and check if it's whitespace
        const char current = rawData[index];
        if (!isspace(current)) return;
        incrementPosition();
    }
}

Lexer::Lexer (std::string & data, std::filesystem::path  filename) : rawData(data), dataLength(data.size()), filename(std::move(filename)) {
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

void Lexer::incrementPosition() {
    index++;
    if (rawData[index] == '\n') {
        position.line++;
        position.column = 0;
    }
    position.column++;
}


bool Lexer::advanceToken(Token & token) {
    skipWhitespace();

    token.start = index;
    token.end = index;
    token.parent = &rawData;

    if (atEnd()) {
        token.type = TokenType::END_OF_FILE;
        return false;
    }

    token.type = TokenType::UNRECOGNIZED;
    token.position = position;

    // Match against a map of predetermined strings to token types.

    // Keywords
    IF_CHOMP_RETURN("fn", TokenType::KW_FUNCTION);
    IF_CHOMP_RETURN("if", TokenType::KW_IF);
    IF_CHOMP_RETURN("else", TokenType::KW_ELSE);
    IF_CHOMP_RETURN("loop", TokenType::KW_LOOP);
    IF_CHOMP_RETURN("break", TokenType::KW_BREAK);
    IF_CHOMP_RETURN("continue", TokenType::KW_CONTINUE);
    IF_CHOMP_RETURN("return", TokenType::KW_RETURN);
    IF_CHOMP_RETURN("true", TokenType::KW_TRUE);
    IF_CHOMP_RETURN("false", TokenType::KW_FALSE);
    IF_CHOMP_RETURN("null", TokenType::KW_NULL);
    IF_CHOMP_RETURN("inf", TokenType::KW_INFINITY);
    IF_CHOMP_RETURN("-inf", TokenType::KW_NEG_INFINITY);
    IF_CHOMP_RETURN("nan", TokenType::KW_NAN);
    IF_CHOMP_RETURN("try", TokenType::KW_TRY);
    IF_CHOMP_RETURN("recover", TokenType::KW_RECOVER);
    IF_CHOMP_RETURN("use", TokenType::KW_USE);

    // Match comments before we match punctuation
    if ( chompString("/*", token) ) {
        token.type = TokenType::COMMENT;
        // Save the start, since it gets overwritten by chompString
        auto start = token.start;
        while ( !chompString("*/", token) ) {
            if ( index + 2 >= dataLength ) {
                throw exceptions::SyntaxError::unexpectedEOF(token.getPosition(), filename);
            }
            incrementPosition();
        }
        token.start = start;
        token.end = index;
        return true;
    }

    // Punctuation
    IF_CHOMP_RETURN(";", TokenType::PUNC_SEMICOLON);
    IF_CHOMP_RETURN(":=", TokenType::PUNC_DECLARATION);
    IF_CHOMP_RETURN("::", TokenType::PUNC_SCOPE);
    IF_CHOMP_RETURN("$", TokenType::PUNC_CALL);
    IF_CHOMP_RETURN("+", TokenType::PUNC_PLUS);
    IF_CHOMP_RETURN("*", TokenType::PUNC_MULT);
    IF_CHOMP_RETURN("/", TokenType::PUNC_DIV);
    IF_CHOMP_RETURN("%", TokenType::PUNC_MOD);
    IF_CHOMP_RETURN(".", TokenType::PUNC_INDEX);
    IF_CHOMP_RETURN(",", TokenType::PUNC_COMMA);
    IF_CHOMP_RETURN("&&", TokenType::PUNC_AND);
    IF_CHOMP_RETURN("||", TokenType::PUNC_OR);
    IF_CHOMP_RETURN("==", TokenType::PUNC_DOUBLE_EQ);
    IF_CHOMP_RETURN("!=", TokenType::PUNC_NEQ);
    IF_CHOMP_RETURN("<=", TokenType::PUNC_LEQ);
    IF_CHOMP_RETURN(">=", TokenType::PUNC_GEQ);
    IF_CHOMP_RETURN("=", TokenType::PUNC_EQ);
    IF_CHOMP_RETURN("&", TokenType::PUNC_AMPERSAND);
    IF_CHOMP_RETURN("|", TokenType::PUNC_BITOR);
    IF_CHOMP_RETURN("^", TokenType::PUNC_XOR);
    IF_CHOMP_RETURN("!", TokenType::PUNC_NOT);
    IF_CHOMP_RETURN("[", TokenType::PUNC_L_BRACKET);
    IF_CHOMP_RETURN("]", TokenType::PUNC_R_BRACKET);
    IF_CHOMP_RETURN("{", TokenType::PUNC_L_BRACE);
    IF_CHOMP_RETURN("}", TokenType::PUNC_R_BRACE);
    IF_CHOMP_RETURN("(", TokenType::PUNC_L_PAREN);
    IF_CHOMP_RETURN(")", TokenType::PUNC_R_PAREN);
    IF_CHOMP_RETURN("<", TokenType::PUNC_LT);
    IF_CHOMP_RETURN(">", TokenType::PUNC_GT);

    // Match against a hexadecimal number
    if ( chompString("0x", token) ) {
        token.type = TokenType::LIT_HEX_NUMBER;

        while ( !atEnd() && isxdigit(rawData[index]))
            incrementPosition();

        token.end = index;
        return true;
    }

    // Match against a binary number
    if ( chompString("0b", token) ) {
        token.type = TokenType::LIT_BIN_NUMBER;

        while ( !atEnd() && (rawData[index] == '0' || rawData[index] == '1'))
            incrementPosition();

        token.end = index;
        return true;
    }

    // Match against an octal number
    if ( chompString("0o", token) ) {
        token.type = TokenType::LIT_OCT_NUMBER;

        while ( !atEnd() && rawData[index] >= '0' && rawData[index] <= '7')
            incrementPosition();
        token.end = index;
        return true;
    }

    auto currentByte = rawData[index];

    // Match against a decimal number (optionally, with a point)
    if ( isdigit(currentByte) || (
        index + 1 < dataLength &&
        currentByte == '-' &&
        isdigit(rawData[index + 1])
    ) ) {
        if (currentByte == '-') index++;
        token.type = TokenType::LIT_DEC_NUMBER;
        bool foundDecimalPoint = false;

        while (isdigit(rawData[index]) || (!foundDecimalPoint && rawData[index] == '.')) {
            foundDecimalPoint |= rawData[index] == '.';
            incrementPosition(); token.end++;
            if (atEnd()) return true;
        }
        return true;
    }

    IF_CHOMP_RETURN("-", TokenType::PUNC_MINUS);

    // Match a string
    if ( currentByte == '"' ) {
        token.type = TokenType::LIT_STRING;
        bool lastWasEscape = false;
        incrementPosition(); // Advance past the starting quote
        if (atEnd()) throw exceptions::SyntaxError::unexpectedEOF(token.getPosition(), filename);
        while ( lastWasEscape || rawData[index] != '\"' ) {
            lastWasEscape = rawData[index] == '\\';
            incrementPosition(); // Advance past the inner character
            if (atEnd()) throw exceptions::SyntaxError::unexpectedEOF(token.getPosition(), filename);
        }
        incrementPosition(); // Advance past the ending quote
        token.end = index;
        return true;
    }

    // Match an identifier
    if ( isalpha(currentByte) || currentByte == '_' ) {
        token.type = TokenType::IDENTIFIER;
        while ( !atEnd() ) {
            auto const thisByte = rawData[index];
            if ( !(isalnum(thisByte) || thisByte == '_') )
                break;
            incrementPosition();
        }
        token.end = index;
        return true;
    }

    throw exceptions::SyntaxError(
        "unrecognized token", token.getPosition(), filename
    );
}