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

        SyntaxError(std::string msg, const FilePosition pos, std::filesystem::path filename) : message(std::move(msg)), position(pos) {
            formatted = "syntax error at " + position.to_string() + " in file \"" + filename.generic_string() + "\": " + message;
        }

        static SyntaxError unexpectedEOF(const FilePosition position, std::filesystem::path filename) {
            return {"unexpected end of file", position, std::move(filename)};
        }

        static SyntaxError unexpectedToken(const lexer::Token &token, parsing::ParserState state, std::filesystem::path filename);
        static SyntaxError unexpectedToken(const lexer::Token &token, parsing::ParserState state, lexer::TokenType expected, std::filesystem::path filename);

        static SyntaxError invalidToken(const lexer::Token &token, const std::string &why, std::filesystem::path filename);

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
