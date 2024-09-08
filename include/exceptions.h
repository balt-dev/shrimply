#pragma once

#include <exception>
#include <sstream>
#include <string>
#include <utility>

namespace parsing {
    enum struct ParserState;
}

namespace lexer { class Token; }

namespace exceptions {
    struct FilePosition {
        size_t line;
        size_t column;

        std::string to_string() const {
            std::ostringstream stream;
            stream << line << ":" << column;
            return stream.str();
        }
    };

    class SyntaxError: public std::exception {
        std::string formatted;
    public:
        std::string message;
        const FilePosition position;

        SyntaxError(std::string msg, const FilePosition pos) : message(std::move(msg)), position(pos) {
            formatted = "syntax error at " + position.to_string() + ": " + message;
        }

        static SyntaxError unexpectedEOF(const FilePosition position) {
            return SyntaxError("unexpected end of file", position);
        }

        static SyntaxError unexpectedToken(const lexer::Token &token, parsing::ParserState state);

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

    class RuntimeError final: public std::exception {
        std::string formatted;
    public:
        std::string message;
        explicit RuntimeError(std::string msg) : message(std::move(msg)) {
            formatted = "runtime error: " + message;
        }

        const char * what() const noexcept override { return formatted.c_str(); }
    };
}
