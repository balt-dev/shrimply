#pragma once
#include <string>

#include "exceptions.h"

/// Contains the lexer for parsing programs.
/// This simply turns a string into a list of tokens.
namespace lexer {
    class Lexer;

    // A type that a token can be.
    class TokenType {
    public:
        enum Value {
            UNRECOGNIZED, // no valid token found

            // --- Keywords ---
            KW_FUNCTION, // fn
            KW_LET, // let
            KW_IF, // if
            KW_ELSE, // else
            KW_LOOP, // loop
            KW_BREAK, // break
            KW_CONTINUE, // continue
            KW_RETURN, // return
            KW_TRUE, // true
            KW_FALSE, // false
            KW_NULL, // null

            // --- Punctuation ---
            PUNC_SEMICOLON, // ;
            PUNC_PLUS, // +
            PUNC_MINUS, // -
            PUNC_MULT, // *
            PUNC_DIV, // /
            PUNC_MOD, // %
            PUNC_DOT, // .
            PUNC_COMMA, // ,
            PUNC_AND, // &&
            PUNC_OR, // ||
            PUNC_EQ, // ==
            PUNC_NEQ, // !=
            PUNC_LEQ, // <=
            PUNC_GEQ, // >=
            PUNC_ASSIGN, // =
            PUNC_BITAND, // &
            PUNC_BITOR, // |
            PUNC_XOR, // ^
            PUNC_NOT, // !
            PUNC_L_PAREN, // (
            PUNC_R_PAREN, // )
            PUNC_L_BRACKET, // [
            PUNC_R_BRACKET, // ]
            PUNC_L_BRACE, // {
            PUNC_R_BRACE, // }
            PUNC_L_ANGLE, // <
            PUNC_R_ANGLE, // >

            // --- Literals ---
            LIT_HEX_NUMBER, // 0x[\dA-Fa-f]+
            LIT_BIN_NUMBER, // 0b[01]+
            LIT_OCT_NUMBER, // 0o[0-7]+
            LIT_DEC_NUMBER, // \d+\.\d*
            LIT_STRING, // "(?:(?:(?<=\\).)|(?:[^"]))*"

            // --- Miscellaneous ---
            COMMENT, //  /\*.*?\*/
            IDENTIFIER // [A-Za-z_][A-Za-z_\d]*
        };
        TokenType() = default;
        explicit TokenType(Value val) : value(val) {}
        constexpr explicit operator Value() const { return value; }
        explicit operator bool() const = delete;

        /// @brief Converts the enum variant to a printable string.
        std::string to_string() const;

        TokenType &operator=(Value value) {
            *this = TokenType(value);
            return *this;
        }
        Value inner() const { return value; }

        bool operator==(const Value & val) const { return val == value; }
        bool operator!=(const Value & val) const { return !(*this == val); }

    private:
        Value value;
    };

    class Token {
        friend Lexer;

        std::string * parent;
        size_t start;
        size_t end;
        TokenType type;
    public:
        Token() : parent(nullptr), start(0), end(0), type() {}

        Token(std::string * p, const size_t s, const size_t e, const TokenType t)
            : parent(p), start(s), end(e), type(t)
        {}
        /// @brief Returns the file position of the token.
        exceptions::FilePosition getPosition() const;
        /// @brief Returns the type of the token.
        TokenType getType() const { return type; }
        /// @brief Returns the start index of the token.
        size_t getStart() const { return start; }
        /// @brief Returns the end index of the token.
        size_t getEnd() const { return end; }
        /// @brief Returns the substring of the parent string that the token spans over.
        std::string to_string() const;
    };

    class Lexer {
        std::string & rawData;
        // We store the data length so we don't have to keep computing it
        const size_t dataLength;
        size_t index = 0;

        // fuck utf8 support i need o(1)
        char byteAt(const size_t index) const {
            const char* rawPointer = rawData.c_str();
            return rawPointer[index];
        }


        /// @brief Matches a string in the raw data, advancing if it is found.
        /// @param needle The string to match.
        /// @param token A token to write to. This is left untouched if the function returns false.
        /// @return Whether the string was found.
        bool chompString(const std::string& needle, Token & token);

    public:
        explicit Lexer(std::string &data);

        /// @brief Skips over whitespace in the data string.
        void skipWhitespace();

        /// @brief Returns whether the lexer is at the end of the string.
        bool atEnd() const {
            return index >= dataLength;
        }

        /// @brief Advances the lexer by one token.
        /// @param token Out parameter to store the parsed token in.
        /// @return Boolean dictating whether the end of the file has not been reached.
        /// @throw exceptions::SyntaxError
        bool advanceToken(Token & token);
    };

}

