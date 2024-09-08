#include "parsing.h"

#include <iostream>

#include "lexer.h"

using namespace parsing;
using lexer::TokenType;

#define SWAP_AND_BREAK(state) stateStack.back() = ParserState::state; break;
#define EXPECT_TYPE(expectedType) if (type != TokenType::expectedType) THROW_UNEXPECTED;
#define MATCH_TYPE(expectedType) case TokenType::expectedType:
#define THROW_UNEXPECTED throw exceptions::SyntaxError::unexpectedToken(token, stateStack.back());
#define EXPECT_SWAP_BREAK(expected, state) EXPECT_TYPE(expected); SWAP_AND_BREAK(state);
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
    std::vector<std::shared_ptr<Statement>> statements;
};

Root Parser::getSyntaxTree() {
    if (stateStack.size() != 1)
        throw exceptions::SyntaxError::unexpectedEOF(lastToken.getPosition());
    stateStack.pop_back();
    Root value = std::move(*syntaxTree);
    syntaxTree.reset();
    return value;
}

void Parser::advance(lexer::Token token) {
    lastToken = token;
    auto const type = token.getType();
    if (type == TokenType::COMMENT) return; // Ignore comments

    auto head = treeCursor.back();

    switch (stateStack.back()) {
        // Note: I've used blocks around the cases to make them distinct variable scopes.
        case ParserState::ROOT: {
            TRY_DOWNCAST(root, Root, head);
            switch (type.inner()) {
                case TokenType::KW_LET: {
                    stateStack.push_back(ParserState::DECLARATION_IDENT);
                    const std::shared_ptr<Item> decl = std::make_shared<Declaration>();
                    root->items.push_back(decl);
                    treeCursor.push_back(decl);
                    break;
                }
                case TokenType::KW_FUNCTION: {
                    stateStack.push_back(ParserState::FUNCTION_IDENT);
                    const std::shared_ptr<Item> fn = std::make_shared<Function>();
                    root->items.push_back(fn);
                    treeCursor.push_back(fn);
                    break;
                }
                default: THROW_UNEXPECTED;
            } break;
        }
        case ParserState::FUNCTION_IDENT: {
            EXPECT_SWAP_BREAK(IDENTIFIER, FUNCTION_OPEN_PAREN);
        }
        case ParserState::FUNCTION_OPEN_PAREN: {
            EXPECT_TYPE(PUNC_L_PAREN);
            TRY_DOWNCAST(fn, Function, head);
            auto arglist = std::make_shared<ArgList>();
            treeCursor.push_back(arglist);
            SWAP_AND_BREAK(ARGLIST_NEXT);
        }
        case ParserState::ARGLIST_NEXT: {
            TRY_DOWNCAST(argList, ArgList, head);
            switch (type.inner()) {
                case TokenType::IDENTIFIER: {
                    argList->arguments.push_back(token.to_string());
                    SWAP_AND_BREAK(ARGLIST_COMMA);
                }
                case TokenType::PUNC_R_PAREN: {
                    treeCursor.pop_back();
                    TRY_DOWNCAST(fn, Function, treeCursor.back());
                    fn->arguments = argList->arguments;
                    stateStack.back() = ParserState::FUNCTION_BLOCK;
                    stateStack.push_back(ParserState::BLOCK_START);
                    treeCursor.push_back(std::make_shared<Block>());
                    break;
                }
                default: THROW_UNEXPECTED
            }
            break;
        }
        case ParserState::ARGLIST_COMMA: {
            EXPECT_SWAP_BREAK(PUNC_COMMA, ARGLIST_NEXT);
        }
        case ParserState::BLOCK_START: {
            EXPECT_SWAP_BREAK(PUNC_L_BRACE, BLOCK);
        }
        case ParserState::BLOCK: {
            switch (type.inner()) {
                case TokenType::PUNC_R_BRACE: {
                    stateStack.pop_back();
                    break;
                }
                default: {
                    stateStack.push_back(ParserState::STATEMENT);
                    break;
                }
            }
            return advance(token);

        }
        case ParserState::FUNCTION_BLOCK: {
            // At this point, we have the block on the stack, then the function
            TRY_DOWNCAST(fnBody, Block, head);
            treeCursor.pop_back(); // Function is now the head
            TRY_DOWNCAST(fn, Function, treeCursor.back());
            fn->body = fnBody->statements;
            treeCursor.pop_back(); // Root is now the head
            stateStack.pop_back(); // This will bring us back to root
            break;
        }
        default:
            throw std::runtime_error("todo: " + std::to_string((int) stateStack.back()));
    }
}