#include "parsing.h"
#include "lexer.h"

using namespace parsing;
using lexer::TokenType;

#define PUSH_ITEM(type) { \
    std::shared_ptr<Atom> abstractPtr = std::make_shared<type>(); \
    syntaxTree.push_back(abstractPtr); \
    treeHead = abstractPtr; \
}
#define POP_ITEM(type, name) \
    syntaxTree.pop_back(); \
    treeHead = syntaxTree.back(); \
    TRY_DOWNCAST(name, type, treeHead);
#define PUSH_AND_BREAK(state, item) PUSH_ITEM(item); stateStack.push_back(ParserState::state); break;
#define SWAP_AND_BREAK(state) stateStack.back() = ParserState::state; break;
#define EXPECT_TYPE(expectedType) if (type != TokenType::expectedType) THROW_UNEXPECTED;
#define MATCH_TYPE(expectedType) case TokenType::expectedType:
#define THROW_UNEXPECTED throw exceptions::SyntaxError::unexpectedToken(token);
#define EXPECT_PUSH_BREAK(expected, state, item) EXPECT_TYPE(expected); PUSH_AND_BREAK(state, item);
#define EXPECT_SWAP_BREAK(expected, state) EXPECT_TYPE(expected); SWAP_AND_BREAK(state);
#define PUSH_IF_MATCH(expected, state, item) MATCH_TYPE(expected) PUSH_AND_BREAK(state, item);
#define SWAP_IF_MATCH(expected, state) MATCH_TYPE(expected); SWAP_AND_BREAK(state);
#define TRY_DOWNCAST(name, type, ptr) std::shared_ptr<type> name; { \
    auto val = std::dynamic_pointer_cast<type>(ptr); \
    if (!val) throw exceptions::InvalidAST(token.getPosition()); \
    name = val; \
}


class ArgList final: public Atom {
public:
    std::vector<std::string> arguments;
};

class Block final: public Atom {
public:
    std::vector<Statement> statements;
};

void Parser::advance(lexer::Token token) {
    auto const type = token.getType();
    auto head = treeHead;
    if (type == TokenType::COMMENT) return; // Ignore comments

    switch (stateStack.back()) {
        // Note: I've used blocks around the cases to make them distinct variable scopes.
        case ParserState::ROOT: {
            switch (type.inner()) {
                PUSH_IF_MATCH(KW_LET, DECLARATION_IDENT, Declaration);
                PUSH_IF_MATCH(KW_FUNCTION, FUNCTION_IDENT, Function);
                default: THROW_UNEXPECTED;
            } break;
        }
        case ParserState::FUNCTION_IDENT: {
            EXPECT_SWAP_BREAK(IDENTIFIER, FUNCTION_OPEN_PAREN);
        }
        case ParserState::FUNCTION_OPEN_PAREN: {
            EXPECT_TYPE(PUNC_L_PAREN);
            PUSH_ITEM(ArgList);
            SWAP_AND_BREAK(ARGLIST_NEXT);
        }
        case ParserState::ARGLIST_NEXT: {
            switch (type.inner()) {
                case TokenType::IDENTIFIER: {
                    TRY_DOWNCAST(argList, ArgList, head);
                    argList->arguments.push_back(token.to_string());
                    SWAP_AND_BREAK(ARGLIST_COMMA);
                }
                case TokenType::PUNC_R_PAREN: {
                    TRY_DOWNCAST(completeArgList, ArgList, head);
                    const auto args = completeArgList->arguments;
                    POP_ITEM(Function, fnHead);
                    fnHead->arguments = args;
                    PUSH_AND_BREAK(BLOCK, Block);
                }
                default: THROW_UNEXPECTED
            }
        }
        case ParserState::ARGLIST_COMMA: {
            EXPECT_SWAP_BREAK(PUNC_COMMA, ARGLIST_NEXT);
        }
    }
}