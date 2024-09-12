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
        BINARY_LHS, BINARY_RHS, CALL_PATH, UNARY_VALUE, RETURN_END, LIST_NEXT, MAP_KEY,
        CALL_ARGS_NEXT, CALL_L_PAREN, CALL_ARG_EXPR, CALL_ARGS_COMMA,
        IF_TRUE, IF_FALSE, IF_ELSE,
        LIST_COMMA, LIST_EXPR,
        MAP_KEY_STRING, MAP_EQ, MAP_VALUE, MAP_COMMA,
        STATEMENT_EXPRESSION,
        DECLARATION_END,
        BLOCK_STATEMENT, GLOBAL_DECLARATION,
        FUNCTION_STATEMENT, LOOP_STATEMENT,
        PATH_IDENT,
        PATH_SCOPE_OR_END,
        USE_PATH,
        TRY_STATEMENT,
        TRY_MAYBE_RECV,
        RECV_STATEMENT,
        RECV_PATH,
        TERNARY_PREDICATE,
        TERNARY_LHS,
        TERNARY_RHS,
    };

    class Atom {
    public:
        exceptions::FilePosition position;
        virtual std::string to_string() const {
            return "???";
        }
        virtual ~Atom() = default;
    };
    /// A top-level item in the file, like a global variable or a function.
    class Item: public Atom {};
    // The entire file.
    struct Root final: Atom {
        std::vector<std::shared_ptr<Item>> items {};
        std::string to_string() const override {
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
    struct Block final: Statement {
        std::vector<std::shared_ptr<Statement>> statements {};

        std::string to_string() const override {
            std::stringstream ss;
            ss << "{";
            for (const auto& stmt : statements)
                ss << stmt->to_string() << " ";
            ss << "}";
            return ss.str();
        }
    };
    struct Expression: Atom {
        /// Assigns a value to the place this expression represents.
        virtual value::Value *pointer(runtime::Stackframe &frame) {
            throw exceptions::RuntimeError(frame, "expression does not support assignment: " + to_string());
        }
        /// Evaluates the rvalue and returns a result.
        virtual value::Value result(runtime::Stackframe & frame) {
            throw exceptions::RuntimeError(frame, "internal error: cannot evaluate expression: " + to_string());
        }
    };
    /// An identifier path.
    struct Path final: Expression {
        std::vector<std::string> members;

        std::string to_string() const override {
            if (members.empty()) return "<empty path>";

            std::stringstream ss;

            for (int i = 0; i < members.size(); i++) {
                if (i) ss << "::";
                ss << members[i];
            }
            return ss.str();
        }

        value::Value *pointer(runtime::Stackframe &frame) override;
        value::Value result(runtime::Stackframe & frame) override;

        explicit Path(std::vector<std::string> path = {}) : members(std::move(path)) {}
    };
    /// Importing a module.
    struct Use: Item {
        Path module;

        std::string to_string() const override {
            return "use " + module.to_string() + ";";
        };
    };
    /// An expression as a statement.
    struct ExpressionStatement final: Statement {
        std::shared_ptr<Expression> expr;

        std::string to_string() const override { return (expr ? expr->to_string() : "<nullptr>") + ";"; }
    };
    /// A literal value.
    struct Literal final: Expression {
        value::Value value;
        value::Value result(runtime::Stackframe & frame) override {
            return value;
        };
        Literal() = default;

        std::string to_string() const override {
            return value.raw_string();
        }

        explicit Literal(const value::Value &v) : value(v) {};
    };
    /// A binary expression.
    struct BinaryOp final: Expression {
        lexer::TokenType opr;
        std::shared_ptr<Expression> lhs = std::make_shared<Literal>();
        std::shared_ptr<Expression> rhs = std::make_shared<Literal>();

        value::Value *pointer(runtime::Stackframe &frame) override;
        value::Value result(runtime::Stackframe & frame) override;

        explicit BinaryOp(lexer::TokenType _opr): opr(_opr) {}
        std::string to_string() const override {
            return opr.to_string() + " " + lhs->to_string() + " " + rhs->to_string();
        }
    };
    /// A unary expression.
    struct UnaryOp final: Expression {
        lexer::TokenType opr;
        std::shared_ptr<Expression> value = std::make_shared<Literal>();

        value::Value result(runtime::Stackframe & frame) override;

        explicit UnaryOp(lexer::TokenType _opr): opr(_opr) {}

        std::string to_string() const override {
            return opr.to_string() + " " + value->to_string();
        }
    };
    /// A ternary expression.
    struct Ternary final: Expression {
        std::shared_ptr<Expression> predicate = std::make_shared<Literal>();
        std::shared_ptr<Expression> lhs = std::make_shared<Literal>();
        std::shared_ptr<Expression> rhs = std::make_shared<Literal>();

        value::Value *pointer(runtime::Stackframe &frame) override;
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() const override {
            return "? " + predicate->to_string() + " " + lhs->to_string() + " " + rhs->to_string();
        }
    };
    /// A function call.
    struct Call final: Expression {
        Path functionPath;
        std::vector<std::shared_ptr<Expression>> arguments {};
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() const override {
            std::ostringstream ss;
            ss << "$ " << functionPath.to_string() << " (";
            for (const auto& arg : arguments) {
                ss << arg->to_string() << ", ";
            }
            ss << ")";
            return ss.str();
        }
    };
    /// Breaking out of a loop.
    class Break final: public Statement {
        std::string to_string() const override {
            return "break";
        }
    };
    /// Continuing to the next iteration of a loop.
    class Continue final: public Statement {
        std::string to_string() const override {
            return "continue";
        }
    };
    /// Returning a value from a function.
    struct Return final: Statement {
        std::shared_ptr<Expression> value = std::make_shared<Literal>();
        Return() {
            value = std::make_shared<Literal>();
        }

        std::string to_string() const override {
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

        std::string to_string() const override {
            return ":= " + name + " " + value->to_string();
        }
    };
    /// A top level declaration of a function.
    struct Function final: Item {
        std::string name;
        std::vector<std::string> arguments {};
        std::shared_ptr<Statement> body;

        std::string to_string() const override {
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
    struct IfElse final: Statement {
        std::shared_ptr<Expression> predicate = std::make_shared<Literal>();
        std::shared_ptr<Statement> truePath;
        std::shared_ptr<Statement> falsePath;

        std::string to_string() const override {
            std::ostringstream ss;
            ss << "if " + predicate->to_string() << " "
                << (truePath ? truePath->to_string() : "<nullptr>")
                << " else " << (falsePath ? falsePath->to_string() : "<nullptr>");
            return ss.str();
        }
    };
    /// Error handling.
    struct TryRecover final: Statement {
        std::shared_ptr<Statement> happyPath;
        Path binding;
        std::shared_ptr<Statement> sadPath;

        std::string to_string() const override {
            std::ostringstream ss;
            ss << "try "
                << (happyPath ? happyPath->to_string() : "<nullptr>")
                << " recover " << binding.to_string() << " "
                << (sadPath ? sadPath->to_string() : "<nullptr>");
            return ss.str();
        }
    };
    /// Repeated code execution.
    struct Loop final: Statement {
        std::shared_ptr<Statement> body;
        std::string to_string() const override {
            return "loop " + (body ? body->to_string() : "<nullptr>");
        }
    };

    struct List final: Expression {
        std::vector<std::shared_ptr<Expression>> members;
        value::Value result(runtime::Stackframe & frame) override;

        std::string to_string() const override {
            std::ostringstream ss;
            ss << "[";
            for (const auto &member : members) {
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

        std::string to_string() const override {
            std::ostringstream ss;
            ss << "(";
            for (const auto& pair : pairs) {
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
        std::filesystem::path filename;

        Parser(std::filesystem::path filename) : filename(std::move(filename)) {
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
