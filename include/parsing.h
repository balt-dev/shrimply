#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "value.h"
#include "lexer.h"

namespace runtime {
    struct Stackframe;
}

namespace parsing {
    /// The parser's current state.
    enum struct ParserState {
        ROOT, DECLARATION_IDENT, FUNCTION_IDENT, FUNCTION_OPEN_PAREN, ARGLIST_NEXT, BLOCK, ARGLIST_COMMA,
        STATEMENT, BLOCK_START, DECLARATION_ASSIGN_OR_END,
        EXPRESSION, STATEMENT_SEMICOLON, RETURN_EXPRESSION_OR_END, IF_PREDICATE,
        BINARY_LHS, BINARY_RHS, CALL_IDENT, UNARY_VALUE, RETURN_END, LIST_NEXT, MAP_KEY,
        CALL_ARGS_NEXT, CALL_L_PAREN, CALL_ARG_EXPR, CALL_ARGS_COMMA,
        IF_TRUE, IF_FALSE, IF_ELSE,
        LIST_COMMA, LIST_EXPR,
        MAP_KEY_STRING, MAP_EQ, MAP_VALUE, MAP_COMMA,
        STATEMENT_EXPRESSION,
        DECLARATION_END,
        BLOCK_STATEMENT, GLOBAL_DECLARATION,
        FUNCTION_STATEMENT, LOOP_STATEMENT,
    };

    class Atom {
    public:
        exceptions::FilePosition position;
        virtual std::string to_string() {
            return "???";
        }
        virtual ~Atom() = default;
    };
    /// A top-level item in the file, like a global variable or a function.
    class Item: public Atom {};
    // The entire file.
    class Root final: public Atom {
    public:
        std::vector<std::shared_ptr<Item>> items {};
        std::string to_string() override {
            std::stringstream str;
            for (const auto& item : items) {
                str << item->to_string() << "; ";
            }
            return str.str();
        }
    };
    /// A statement, like a variable declaration or return.
    class Statement: public Atom {};
    /// A syntactic block of statements.
    class Block final: public Statement {
    public:
        std::vector<std::shared_ptr<Statement>> statements {};

        std::string to_string() override {
            std::stringstream ss;
            ss << "{";
            for (const auto& stmt : statements)
                ss << stmt->to_string() << " ";
            ss << "}";
            return ss.str();
        }
    };
    class Expression: public Atom {
    public:
        /// Assigns a value to the place this expression represents.
        virtual value::Value *pointer(runtime::Stackframe &frame) {
            throw exceptions::RuntimeError(frame, "expression does not support assignment: " + to_string());
        }
        /// Evaluates the rvalue and returns a result.
        virtual value::Value result(runtime::Stackframe & frame) {
            throw std::runtime_error("internal runtime error: cannot evaluate expression: " + to_string());
        }
    };
    class ExpressionStatement: public Statement {
    public:
        std::shared_ptr<Expression> expr;

        std::string to_string() override { return (expr ? expr->to_string() : "<nullptr>") + ";"; }
    };
    /// A literal value.
    class Literal final: public Expression {
    public:
        value::Value value;
        value::Value result(runtime::Stackframe & frame) override {
            return value;
        };
        Literal() = default;

        std::string to_string() override {
            return value.raw_string();
        }

        explicit Literal(const value::Value &v) : value(v) {};
    };
    /// A binary expression.
    class BinaryOp final: public Expression {
    public:
        lexer::TokenType opr;
        std::shared_ptr<Expression> lhs = std::make_shared<Literal>();
        std::shared_ptr<Expression> rhs = std::make_shared<Literal>();

        value::Value *pointer(runtime::Stackframe &frame) override;
        value::Value result(runtime::Stackframe & frame) override;

        explicit BinaryOp(lexer::TokenType _opr): opr(_opr) {}
        std::string to_string() override {
            return opr.to_string() + " " + lhs->to_string() + " " + rhs->to_string();
        }
    };
    /// A unary expression.
    class UnaryOp final: public Expression {
    public:
        lexer::TokenType opr;
        std::shared_ptr<Expression> value = std::make_shared<Literal>();

        value::Value result(runtime::Stackframe & frame) override;

        explicit UnaryOp(lexer::TokenType _opr): opr(_opr) {}

        std::string to_string() override {
            return opr.to_string() + " " + value->to_string();
        }
    };
    /// A function call.
    class Call final: public Expression {
    public:
        std::string functionName;
        std::vector<std::shared_ptr<Expression>> arguments {};
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() override {
            std::ostringstream ss;
            ss << "$ " << functionName << " (";
            for (const auto& arg : arguments) {
                ss << arg->to_string() << ", ";
            }
            ss << ")";
            return ss.str();
        }
    };
    /// An identifier.
    class Identifier final: public Expression {
    public:
        std::string name;

        value::Value *pointer(runtime::Stackframe &frame) override;
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() override {
            return name;
        }

        explicit Identifier(std::string str = "") : name(std::move(str)) {}
    };
    /// Breaking out of a loop.
    class Break final: public Statement {
        std::string to_string() override {
            return "break";
        }
    };
    /// Continuing to the next iteration of a loop.
    class Continue final: public Statement {
        std::string to_string() override {
            return "continue";
        }
    };
    /// Returning a value from a function.
    class Return final: public Statement {
    public:
        std::shared_ptr<Expression> value = std::make_shared<Literal>();
        Return() {
            value = std::make_shared<Literal>();
        }

        std::string to_string() override {
            return "return " + value->to_string();
        }
    };
    /// A declaration of a variable to a value;
    class Declaration final: public Statement, public Item {
    public:
        std::string name;
        std::shared_ptr<Expression> value = std::make_shared<Literal>();;
        Declaration() {
            value = std::make_shared<Literal>();
        }

        std::string to_string() override {
            return ":= " + name + " " + value->to_string();
        }
    };
    /// A top level declaration of a function.
    class Function final: public Item {
    public:
        std::string name;
        std::vector<std::string> arguments {};
        std::shared_ptr<Statement> body;

        std::string to_string() override {
            std::ostringstream ss;
            ss << "fn " + name + "(";
            for (const auto& arg : arguments) {
                ss << arg << ", ";
            }
            ss << ") " << (body ? body->to_string() : "<nullptr>");
            return ss.str();
        }
    };
    /// Alternation based on a predicate.
    class IfElse final: public Statement {
    public:
        std::shared_ptr<Expression> predicate = std::make_shared<Literal>();
        std::shared_ptr<Statement> truePath;
        std::shared_ptr<Statement> falsePath;

        std::string to_string() override {
            std::ostringstream ss;
            ss << "if " + predicate->to_string() << " "
                << (truePath ? truePath->to_string() : "<nullptr>")
                << " else " << (falsePath ? falsePath->to_string() : "<nullptr>");
            return ss.str();
        }
    };
    /// Repeated code execution.
    class Loop final: public Statement {
    public:
        std::shared_ptr<Statement> body;
        std::string to_string() override {
            return "loop " + (body ? body->to_string() : "<nullptr>");
        }
    };

    class List final: public Expression {
    public:
        std::vector<std::shared_ptr<Expression>> members;
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() override {
            std::ostringstream ss;
            ss << "[";
            for (auto member : members) {
                ss << member->to_string() << ", ";
            }
            ss << "]";
            return ss.str();
        }
    };

    class Map final: public Expression {
        friend Parser;
        std::string nextKey;
    public:
        std::unordered_map<std::string, std::shared_ptr<Expression>> pairs;
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() override {
            std::ostringstream ss;
            ss << "(";
            for (auto pair : pairs) {
                ss << "\"" << pair.first << "\" = " << pair.second->to_string() << ", ";
            }
            ss << ")";
            return ss.str();
        }
    };


    class Parser {
    public:
        lexer::Token lastToken;
        std::vector<ParserState> stateStack { ParserState::ROOT };
        std::shared_ptr<Root> syntaxTree;
        std::vector<std::shared_ptr<Atom>> treeCursor;

        Parser() {
            const auto root = std::make_shared<Root>();
            syntaxTree = root;
            treeCursor.push_back(root);
        }

        /// @brief Advances the parser by one token.
        /// @param token The next token in the file.
        void advance(lexer::Token token);

        /// @brief Attempts to build the AST from complete input.
        /// @throw void Will throw if the AST is incomplete (i.e. more tokens are expected.)
        Root getSyntaxTree();
    };

}
