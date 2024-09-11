#pragma once

#include <exception>
#include <sstream>
#include <string>
#include <utility>

namespace runtime {
    struct Stackframe;
}

namespace lexer {
    class TokenType;
}

namespace parsing {
    enum struct ParserState;
}

namespace exceptions {
    struct FilePosition {
        size_t line = 1;
        size_t column = 1;

        std::string to_string() const {
            std::ostringstream stream;
            stream << line << ":" << column;
            return stream.str();
        }
    };
}


namespace lexer { class Token; }
namespace exceptions {

    class SyntaxError: public std::exception {
        std::string formatted;
    public:
        std::string message;
        const FilePosition position;

        SyntaxError(std::string msg, const FilePosition pos) : message(std::move(msg)), position(pos) {
            formatted = "syntax error at " + position.to_string() + ": " + message;
        }

        static SyntaxError unexpectedEOF(const FilePosition position) {
            return {"unexpected end of file", position};
        }

        static SyntaxError unexpectedToken(const lexer::Token &token, parsing::ParserState state);
        static SyntaxError unexpectedToken(const lexer::Token &token, parsing::ParserState state, lexer::TokenType expected);

        static SyntaxError invalidToken(const lexer::Token &token, const std::string &why);

        const char * what() const noexcept override {
            return formatted.c_str();
        }
    };

    class InvalidAST final: public std::exception {
        std::string formatted;
    public:
        const FilePosition position;
        explicit InvalidAST(const FilePosition pos) : position(pos) {
            formatted = "encountered invalid AST at " + position.to_string() + " (this is probably a parser bug)";
        }

        const char * what() const noexcept override {
            return formatted.c_str();
        }
    };

    class RuntimeError: public std::exception {
        std::string formatted;
    public:
        std::string message;
        explicit RuntimeError(const runtime::Stackframe & frame, std::string msg);

        const char * what() const noexcept override { return formatted.c_str(); }
    };
}
