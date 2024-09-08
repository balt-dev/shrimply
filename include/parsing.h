#pragma once
#include <memory>
#include <string>
#include <vector>

#include "value.h"
#include "lexer.h"

namespace parsing {
    /// The parser's current state.
    enum struct ParserState {
        ROOT, DECLARATION_IDENT, FUNCTION_IDENT, FUNCTION_OPEN_PAREN, ARGLIST_ROOT, ARGLIST_NEXT, BLOCK, ARGLIST_COMMA,
        STATEMENT, FUNCTION_BLOCK, BLOCK_START
    };

    class Atom {
        // HACK: force this to be polymorphic
        virtual void __HACK() {}
    };
    /// A top-level item in the file, like a global variable or a function.
    class Item: public Atom {
    public:
        virtual ~Item() = default;
    };
    // The entire file.
    class Root final: public Atom {
    public:
        std::vector<std::shared_ptr<Item>> items;
    };
    /// A statement, like a variable declaration or return.
    class Statement: public Atom {};
    class Expression: public Atom {
        /// Assigns a value to the place this expression represents.
        virtual void assign(value::AbstractValue value) {
            throw exceptions::RuntimeError("expression is not an lvalue");
        }
        /// Evaluates the rvalue and returns a result.
        virtual value::AbstractValue result() {
            throw exceptions::RuntimeError("expression is not an rvalue");
        }
    };
    /// A literal value.
    class Literal final: public Atom, public Expression {
    public:
        value::AbstractValue value;
        value::AbstractValue result() override { return value; };

        Literal() = default;

        explicit Literal(const value::AbstractValue& v) : value(v) {};
    };
    /// A function call.
    class Call final: public Atom, public Expression {
    public:
        std::string functionName;
        std::vector<std::shared_ptr<Expression>> arguments;
        void assign(value::AbstractValue value) override;
        value::AbstractValue result() override;
    };
    /// An identifier.
    class Identifier final: public Atom, public Expression {
    public:
        std::string name;
        void assign(value::AbstractValue value) override;
        value::AbstractValue result() override;
    };
    /// An index into an lvalue using an rvalue.
    class Index final: public Atom, public Expression {
    public:
        std::shared_ptr<Expression> target;
        std::shared_ptr<Expression> index;
        void assign(value::AbstractValue value) override;
        value::AbstractValue result() override;
    };
    /// Breaking out of a loop.
    class Break final: public Statement, public Atom {};
    /// Continuing to the next iteration of a loop.
    class Continue final: public Statement, public Atom {};
    /// Returning a value from a function.
    class Return final: public Statement, public Atom {
    public:
        std::shared_ptr<Expression> value;
    };
    /// A declaration of a variable to a value;
    class Declaration final: public Statement, public Item, public Atom {
    public:
        std::string name;
        std::shared_ptr<Expression> value;
    };
    /// An assignment of an lvalue to an rvalue.
    class Assignment final: public Statement, public Atom {
    public:
        std::shared_ptr<Expression> lhs;
        std::shared_ptr<Expression> rhs;
    };
    /// A top level declaration of a function.
    class Function final: public Item, public Atom {
    public:
        std::string name;
        std::vector<std::string> arguments;
        std::vector<std::shared_ptr<Statement>> body;
    };

    class Parser {
        lexer::Token lastToken;
        std::vector<ParserState> stateStack { ParserState::ROOT };
        std::shared_ptr<Root> syntaxTree;
        std::vector<std::shared_ptr<Atom>> treeCursor;

    public:
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
