#pragma once

#include <exception>
#include <string>
#include <utility>

namespace lexer { class Token; }

namespace exceptions {
    struct FilePosition {
        size_t line;
        size_t column;

        std::string to_string() const { return line + ":" + column; }
    };

    class SyntaxError: std::exception {
    public:
        std::string message;
        const FilePosition position;

        SyntaxError(std::string msg, const FilePosition pos) : message(std::move(msg)), position(pos) {}

        static SyntaxError unexpectedEOF(const FilePosition position) {
            return SyntaxError("unexpected end of file", position);
        }

        static SyntaxError unexpectedToken(const lexer::Token &token);

        const char * what() const noexcept override {
            return ("syntax error at " + position.to_string() + ": " + message).c_str();
        }
    };

    class InvalidAST final: std::exception {
    public:
        const FilePosition position;
        explicit InvalidAST(const FilePosition pos) : position(pos) {}

        const char * what() const noexcept override {
            return ("encountered invalid AST at " + position.to_string() + " (this is probably a parser bug)").c_str();
        }
    };

    class RuntimeError final: std::exception {
    public:
        std::string message;
        explicit RuntimeError(std::string msg) : message(std::move(msg)) {}

        const char * what() const noexcept override { return ("runtime error: " + message).c_str(); }
    };
}
