#include "parsing.h"

#include <cmath>
#include <filesystem>
#include <iostream>

#include "lexer.h"

using namespace parsing;
using lexer::TokenType;

#define SWAP_AND_BREAK(state) stateStack.back() = ParserState::state; break;
#define EXPECT_TYPE(expectedType) if (type != TokenType::expectedType) { \
    throw exceptions::SyntaxError::unexpectedToken(token, stateStack.back(), TokenType(TokenType::expectedType), filename); \
}
#define THROW_UNEXPECTED throw exceptions::SyntaxError::unexpectedToken(token, stateStack.back(), filename);
#define THROW_INVALID(why) throw exceptions::SyntaxError::invalidToken(token, why, filename);
#define EXPECT_SWAP_BREAK(expected, state) EXPECT_TYPE(expected); SWAP_AND_BREAK(state);
#define TRY_DOWNCAST_HEAD(name, type) std::shared_ptr<type> name; { \
    auto ptr = treeCursor.back(); \
    auto val = std::dynamic_pointer_cast<type>(ptr); \
    if (!val) \
        throwFailedDowncast(stateStack, treeCursor, #type, ptr->position, filename); \
    name = val; \
}

void throwFailedDowncast(const std::vector<ParserState> & stateStack, const std::vector<std::shared_ptr<Atom>> & treeCursor, const char* typeName, exceptions::FilePosition pos, std::filesystem::path filename) {
    std::ostringstream ss;
    ss << "internal error: failed to downcast to " << typeName << " (tree: ";
    for (const auto& atom : treeCursor) {
        ss << atom->to_string() << " | ";
    }
    ss << ", stack: ";
    for (auto state : stateStack) {
        ss << (int) state << " ";
    }
    ss << ")";
    throw exceptions::SyntaxError(ss.str(), pos, filename);
}

class ArgList final: public Atom {
public:
    std::vector<std::string> arguments;

    std::string to_string() const override {
        std::stringstream ss;
        for (const auto& arg : arguments)
            ss << arg << ", ";
        return ss.str();
    }
};

std::string unescapeString(const lexer::Token &token, const std::filesystem::path& filename) {
    auto escaped_buf = token.span();
    escaped_buf = escaped_buf.substr(1, escaped_buf.size() - 2); // Trim quotes from both sides

    if (escaped_buf.find('\\') == std::string::npos) {
        // Happy path, no escapes
        return escaped_buf;
    }

    auto size = escaped_buf.size();
    const char* escaped = escaped_buf.c_str();
    std::string unescaped;
    // Reserve the current length of the string
    unescaped.reserve(size);
    for (size_t i = 0; i < size; i++) {
        char chr = escaped[i];
        if (chr != '\\') {
            unescaped.push_back(chr);
            continue;
        }

        // Parse escape sequence
        i++; if (i >= size) THROW_INVALID("unexpected end of escape sequence");
        switch (escaped[i]) {
            case '0': THROW_INVALID("cannot have null byte in string");
            case 'n': unescaped.push_back('\n'); break;
            case 'r': unescaped.push_back('\r'); break;
            case 't': unescaped.push_back('\t'); break;
            case 'x': {
                // Need to parse a hex number
                i += 2; if (i + 1 >= size) THROW_INVALID("unexpected end of escape sequence");
                std::string hex (escaped + i - 1, 2);
                char byte;
                try {
                    byte = (unsigned char) std::stoi(hex, nullptr, 16);
                } catch (const std::exception& _) {
                    THROW_INVALID("failed to parse byte escape as a number");
                }
                if (byte == 0) THROW_INVALID("cannot have null byte in string");
                unescaped.push_back(byte);
                break;
            }
            default: unescaped.push_back(escaped[i]);
        }
    }

    return unescaped;
}


Root Parser::getSyntaxTree() {
    if (stateStack.size() != 1)
        throw exceptions::SyntaxError::unexpectedEOF(lastToken.getPosition(), filename);
    stateStack.pop_back();
    Root value = std::move(*syntaxTree);
    syntaxTree.reset();
    return value;
}

void Parser::advance(lexer::Token token) {
reinterpret: // Note: this is in place of TCO-powered recursion, since TCO isn't guaranteed.

    lastToken = token;
    auto const type = token.type.value;
    if (type == TokenType::COMMENT) return; // Ignore comments

    auto head = treeCursor.back();
    head->position = token.getPosition();

    switch (stateStack.back()) {
        // Note: I've used blocks around the cases to make them distinct variable scopes.
        case ParserState::ROOT: {
            TRY_DOWNCAST_HEAD(root, Root);
            switch (type) {
                case TokenType::KW_USE: {
                    std::shared_ptr<Item> ptr = std::make_shared<Use>();
                    ptr->position = token.getPosition();
                    root->items.push_back(ptr);
                    treeCursor.push_back(ptr);
                    stateStack.push_back(ParserState::STATEMENT_SEMICOLON);
                    stateStack.push_back(ParserState::USE_PATH);
                    std::shared_ptr<Path> path = std::make_shared<Path>();
                    path->position = token.getPosition();
                    treeCursor.push_back(path);
                    stateStack.push_back(ParserState::PATH_IDENT);
                    break;
                }
                case TokenType::PUNC_DECLARATION: {
                    stateStack.push_back(ParserState::GLOBAL_DECLARATION);
                    stateStack.push_back(ParserState::STATEMENT_SEMICOLON);
                    stateStack.push_back(ParserState::DECLARATION_IDENT);
                    std::shared_ptr<Item> ptr = std::make_shared<Declaration>();
                    ptr->position = token.getPosition();
                    treeCursor.push_back(ptr);
                    break;
                }
                case TokenType::KW_FUNCTION: {
                    stateStack.push_back(ParserState::FUNCTION_IDENT);
                    std::shared_ptr<Item> ptr = std::make_shared<Function>();
                    ptr->position = token.getPosition();
                    treeCursor.push_back(ptr);
                    root->items.push_back(ptr);
                    break;
                }
                case TokenType::END_OF_FILE: return;
                default: THROW_UNEXPECTED;
            } break;
        }
        case ParserState::PATH_IDENT: {
            EXPECT_TYPE(IDENTIFIER);
            TRY_DOWNCAST_HEAD(path, Path);
            path->position = token.getPosition();
            path->members.push_back(token.span());
            SWAP_AND_BREAK(PATH_SCOPE_OR_END);
        }
        case ParserState::PATH_SCOPE_OR_END: {
            switch (type) {
                case TokenType::PUNC_SCOPE: SWAP_AND_BREAK(PATH_IDENT);
                default: {
                    // At the end of the path, bubble it up and reinterpret
                    stateStack.pop_back();
                    goto reinterpret;
                }
            }
            break;
        }
        case ParserState::USE_PATH: {
            TRY_DOWNCAST_HEAD(path, Path);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(use, Use);
            use->module = *path;
            treeCursor.pop_back();
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::FUNCTION_IDENT: {
            EXPECT_TYPE(IDENTIFIER);
            TRY_DOWNCAST_HEAD(fn, Function);
            fn->name = token.span();
            SWAP_AND_BREAK(FUNCTION_OPEN_PAREN);
        }
        case ParserState::FUNCTION_OPEN_PAREN: {
            EXPECT_TYPE(PUNC_L_PAREN);
            TRY_DOWNCAST_HEAD(fn, Function);
            treeCursor.push_back(std::make_shared<ArgList>());
            treeCursor.back()->position = token.getPosition();
            SWAP_AND_BREAK(ARGLIST_NEXT);
        }
        case ParserState::ARGLIST_NEXT: {
            TRY_DOWNCAST_HEAD(argList, ArgList);
            switch (type) {
                case TokenType::IDENTIFIER: {
                    argList->arguments.push_back(token.span());
                    SWAP_AND_BREAK(ARGLIST_COMMA);
                }
                case TokenType::PUNC_R_PAREN: {
                    treeCursor.pop_back();
                    TRY_DOWNCAST_HEAD(fn, Function);
                    fn->arguments = argList->arguments;
                    stateStack.back() = ParserState::FUNCTION_STATEMENT;
                    stateStack.push_back(ParserState::STATEMENT);
                    break;
                }
                default: THROW_UNEXPECTED
            }
            break;
        }
        case ParserState::ARGLIST_COMMA: {
            if (token.type == TokenType::PUNC_R_PAREN) {
                stateStack.back() = ParserState::ARGLIST_NEXT;
                goto reinterpret;
            }
            EXPECT_SWAP_BREAK(PUNC_COMMA, ARGLIST_NEXT);
        }

        case ParserState::BLOCK_START: {
            EXPECT_SWAP_BREAK(PUNC_L_BRACE, BLOCK);
        }
        case ParserState::BLOCK: {
            TRY_DOWNCAST_HEAD(block, Block);
            switch (type) {
                case TokenType::PUNC_R_BRACE: {
                    stateStack.pop_back();
                    return;
                }
                default: {
                    stateStack.push_back(ParserState::BLOCK_STATEMENT);
                    stateStack.push_back(ParserState::STATEMENT);
                }
            }
            goto reinterpret;
        }
        case ParserState::FUNCTION_STATEMENT: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(func, Function);
            func->body = stmt;
            treeCursor.pop_back();
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::DECLARATION_IDENT: {
            EXPECT_TYPE(IDENTIFIER);
            TRY_DOWNCAST_HEAD(decl, Declaration);
            decl->name = token.span();
            SWAP_AND_BREAK(DECLARATION_ASSIGN_OR_END);
        }
        case ParserState::DECLARATION_ASSIGN_OR_END: {
            TRY_DOWNCAST_HEAD(decl, Declaration);
            switch (token.type.value) {
                case TokenType::PUNC_SEMICOLON: {
                    stateStack.pop_back();
                    stateStack.pop_back();
                    break;
                }
                default: {
                    stateStack.back() = ParserState::DECLARATION_END;
                    stateStack.push_back(ParserState::EXPRESSION);
                    break;
                }
            }
            goto reinterpret;
        }
        case ParserState::DECLARATION_END: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(decl, Declaration);
            decl->value = expr;
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::GLOBAL_DECLARATION: {
            TRY_DOWNCAST_HEAD(decl, Declaration);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(root, Root);
            root->items.push_back(decl);
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::STATEMENT: {
            switch (token.type.value) {
                case TokenType::PUNC_DECLARATION: {
                    stateStack.back() = ParserState::STATEMENT_SEMICOLON;
                    stateStack.push_back(ParserState::DECLARATION_IDENT);
                    std::shared_ptr<Statement> decl = std::make_shared<Declaration>();
                    decl->position = token.getPosition();
                    treeCursor.push_back(decl);
                    break;
                }
                // Since these can only appear within blocks, we can swap the state on the stack
                // instead of pushing a new one
                case TokenType::KW_BREAK: {
                    const std::shared_ptr<Statement> brk = std::make_shared<Break>();
                    treeCursor.push_back(brk);
                    brk->position = token.getPosition();
                    stateStack.back() = ParserState::STATEMENT_SEMICOLON;
                    break;
                }
                case TokenType::KW_CONTINUE: {
                    const std::shared_ptr<Statement> cont = std::make_shared<Continue>();
                    treeCursor.push_back(cont);
                    cont->position = token.getPosition();
                    stateStack.back() = ParserState::STATEMENT_SEMICOLON;
                    break;
                }
                case TokenType::KW_RETURN: {
                    std::shared_ptr<Statement> ret = std::make_shared<Return>();
                    treeCursor.push_back(ret);
                    ret->position = token.getPosition();
                    stateStack.back() = ParserState::RETURN_EXPRESSION_OR_END;
                    break;
                }
                case TokenType::KW_IF: {
                    std::shared_ptr<Statement> ifelse = std::make_shared<IfElse>();
                    treeCursor.push_back(ifelse);
                    ifelse->position = token.getPosition();
                    stateStack.back() = ParserState::IF_PREDICATE;
                    stateStack.push_back(ParserState::EXPRESSION);
                    break;
                }
                case TokenType::KW_TRY: {
                    std::shared_ptr<Statement> tryrecv = std::make_shared<TryRecover>();
                    treeCursor.push_back(tryrecv);
                    tryrecv->position = token.getPosition();
                    stateStack.back() = ParserState::TRY_STATEMENT;
                    stateStack.push_back(ParserState::STATEMENT);
                    break;
                }
                case TokenType::KW_LOOP: {
                    auto loopRaw = std::make_shared<Loop>();
                    std::shared_ptr<Statement> loop = loopRaw;
                    treeCursor.push_back(loop);
                    loop->position = token.getPosition();
                    stateStack.back() = ParserState::LOOP_STATEMENT;
                    stateStack.push_back(ParserState::STATEMENT);
                    break;
                }
                case TokenType::PUNC_L_BRACE: {
                    std::shared_ptr<Statement> blockStmt = std::make_shared<Block>();
                    treeCursor.push_back(blockStmt);
                    blockStmt->position = token.getPosition();
                    stateStack.back() = ParserState::BLOCK;
                    break;
                }
                default: {
                    // Reinterpret as expression
                    std::shared_ptr<Statement> exprStmt = std::make_shared<ExpressionStatement>();
                    treeCursor.push_back(exprStmt);
                    exprStmt->position = token.getPosition();
                    stateStack.push_back( ParserState::STATEMENT_EXPRESSION);
                    stateStack.push_back(ParserState::EXPRESSION);
                    goto reinterpret;
                }
            }
            break;
        }
        case ParserState::LOOP_STATEMENT: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(loop, Loop);
            loop->body = stmt;
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::BLOCK_STATEMENT: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(block, Block);
            block->statements.push_back(stmt);
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::STATEMENT_SEMICOLON: {
            EXPECT_TYPE(PUNC_SEMICOLON);
            stateStack.pop_back();
            break;
        }
        case ParserState::STATEMENT_EXPRESSION: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(stmt, ExpressionStatement);
            stmt->expr = expr;
            stateStack.pop_back();
            stateStack.back() = ParserState::STATEMENT_SEMICOLON;
            goto reinterpret;
        }
        case ParserState::EXPRESSION: {
            switch (token.type.value) {
                case TokenType::PUNC_TERNARY: { //
                    stateStack.back() = ParserState::TERNARY_PREDICATE;
                    stateStack.push_back(ParserState::EXPRESSION);
                    auto tern = std::make_shared<Ternary>();
                    tern->position = token.getPosition();
                    treeCursor.push_back(tern);
                    break;
                }
                // Binary operator(s)
                case TokenType::PUNC_PLUS: //
                case TokenType::PUNC_MINUS: //
                case TokenType::PUNC_MULT: //
                case TokenType::PUNC_DIV: //
                case TokenType::PUNC_MOD: //
                case TokenType::PUNC_INDEX: //
                case TokenType::PUNC_AND: //
                case TokenType::PUNC_OR: //
                case TokenType::PUNC_DOUBLE_EQ: //
                case TokenType::PUNC_NEQ: //
                case TokenType::PUNC_LEQ: //
                case TokenType::PUNC_GEQ: //
                case TokenType::PUNC_EQ: //
                case TokenType::PUNC_AMPERSAND: //
                case TokenType::PUNC_BITOR: //
                case TokenType::PUNC_XOR: //
                case TokenType::PUNC_SHL: //
                case TokenType::PUNC_SHR: //
                case TokenType::PUNC_LT: //
                case TokenType::PUNC_GT: { //
                    stateStack.back() = ParserState::BINARY_LHS;
                    stateStack.push_back(ParserState::EXPRESSION);
                    auto binop = std::make_shared<BinaryOp>(token.type);
                    binop->position = token.getPosition();
                    treeCursor.push_back(binop);
                    break;
                }
                // Call
                case TokenType::PUNC_CALL: { //
                    stateStack.back() = ParserState::CALL_PATH;
                    auto call = std::make_shared<Call>();
                    call->position = token.getPosition();
                    treeCursor.push_back(call);
                    auto path = std::make_shared<Path>();
                    path->position = token.getPosition();
                    treeCursor.push_back(path);
                    stateStack.push_back(ParserState::PATH_IDENT);
                    break;
                }
                // Unary operator(s)
                case TokenType::PUNC_NOT: {
                    stateStack.back() = ParserState::UNARY_VALUE;
                    stateStack.push_back(ParserState::EXPRESSION);
                    auto unary = std::make_shared<UnaryOp>(token.type);
                    unary->position = token.getPosition();
                    treeCursor.push_back(unary);
                    break;
                }
                // Identifiers
                case TokenType::IDENTIFIER: {
                    stateStack.back() = ParserState::PATH_IDENT;
                    std::shared_ptr<Atom> path = std::make_shared<Path>();
                    path->position = token.getPosition();
                    treeCursor.push_back(path);
                    goto reinterpret;
                }
                // Literals
                case TokenType::KW_NULL: {
                    auto lit = std::make_shared<Literal>();
                    lit->position = token.getPosition();
                    treeCursor.push_back(lit);
                    stateStack.pop_back();
                    break;
                }
                case TokenType::KW_TRUE: {
                    auto literal = std::make_shared<Literal>(value::Value(true));
                    literal->position = token.getPosition();
                    treeCursor.push_back(literal);
                    stateStack.pop_back();
                    break;
                }
                case TokenType::KW_FALSE: {
                    auto literal = std::make_shared<Literal>(value::Value(false));
                    literal->position = token.getPosition();
                    treeCursor.push_back(literal);
                    stateStack.pop_back();
                    break;
                }
                case TokenType::KW_NEG_INFINITY: {
                    auto literal = std::make_shared<Literal>(value::Value(-1.0 / 0.0));
                    literal->position = token.getPosition();
                    treeCursor.push_back(literal);
                    stateStack.pop_back();
                    break;
                }
                case TokenType::KW_INFINITY: {
                    auto literal = std::make_shared<Literal>(value::Value(1.0 / 0.0));
                    literal->position = token.getPosition();
                    treeCursor.push_back(literal);
                    stateStack.pop_back();
                    break;
                }
                case TokenType::KW_NAN: {
                    auto literal = std::make_shared<Literal>(value::Value(0.0 / 0.0));
                    literal->position = token.getPosition();
                    treeCursor.push_back(literal);
                    stateStack.pop_back();
                    break;
                }
                case TokenType::LIT_DEC_NUMBER: {
                    stateStack.pop_back();
                    try {
                        auto string = token.span();
                        if (string.find('.') == std::string::npos) {
                            int64_t number = std::stoll(string);
                            treeCursor.push_back(std::make_shared<Literal>(value::Value(number)));
                        } else {
                            double number = std::stod(string);
                            treeCursor.push_back(std::make_shared<Literal>(value::Value(number)));
                        }
                        treeCursor.back()->position = token.getPosition();
                        break;
                    } catch (std::out_of_range& _) {
                        THROW_INVALID("number is out of range");
                    } catch (std::invalid_argument& _) { \
                        THROW_INVALID("could not convert to number: " + token.span()); \
                    }
                }
#define BASE_LITERAL(base, type) \
                case TokenType::type: { \
                    stateStack.pop_back(); \
                    auto span = token.span().substr(2); \
                    try { \
                        auto number = (int64_t) std::stoull(span, nullptr, base); \
                        treeCursor.push_back(std::make_shared<Literal>(value::Value(number))); \
                        treeCursor.back()->position = token.getPosition(); \
                        break; \
                    } catch (std::out_of_range& _) { \
                        THROW_INVALID("number is out of range"); \
                    } catch (std::invalid_argument& _) { \
                        THROW_INVALID("could not convert to number: " + token.span()); \
                    } \
                }
                BASE_LITERAL(16, LIT_HEX_NUMBER);
                BASE_LITERAL(2, LIT_BIN_NUMBER);
                BASE_LITERAL(8, LIT_OCT_NUMBER);
                case TokenType::LIT_STRING: {
                    stateStack.pop_back();
                    auto unescaped = unescapeString(token, filename);
                    treeCursor.push_back(std::make_shared<Literal>(value::Value(unescaped)));
                    treeCursor.back()->position = token.getPosition();
                    break;
                }
                case TokenType::PUNC_L_BRACKET: {
                    stateStack.back() = ParserState::LIST_NEXT;
                    treeCursor.push_back(std::make_shared<List>());
                    treeCursor.back()->position = token.getPosition();
                    break;
                }
                case TokenType::PUNC_L_PAREN: {
                    stateStack.back() = ParserState::MAP_KEY;
                    treeCursor.push_back(std::make_shared<Map>());
                    treeCursor.back()->position = token.getPosition();
                    break;
                }
                default:
                    THROW_UNEXPECTED;
            }
            break;
        }
        case ParserState::RETURN_EXPRESSION_OR_END: {
            TRY_DOWNCAST_HEAD(ret, Return);
            stateStack.back() = ParserState::RETURN_END;
            if (token.type.value != TokenType::PUNC_SEMICOLON) {
                stateStack.push_back(ParserState::EXPRESSION);
            } else {
                treeCursor.push_back(std::make_shared<Literal>());
                treeCursor.back()->position = token.getPosition();
            }
            goto reinterpret;
        }
        case ParserState::RETURN_END: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(ret, Return);
            ret->value = expr;
            stateStack.back() = ParserState::STATEMENT_SEMICOLON;
            goto reinterpret;
        }
        case ParserState::UNARY_VALUE: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(unr, UnaryOp);
            unr->value = expr;
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::BINARY_LHS: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(bin, BinaryOp);
            bin->lhs = expr;
            stateStack.back() = ParserState::BINARY_RHS;
            stateStack.push_back(ParserState::EXPRESSION);
            goto reinterpret;
        }
        case ParserState::BINARY_RHS: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(bin, BinaryOp);
            bin->rhs = expr;
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::TERNARY_PREDICATE: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(tern, Ternary);
            tern->predicate = expr;
            stateStack.back() = ParserState::TERNARY_LHS;
            stateStack.push_back(ParserState::EXPRESSION);
            goto reinterpret;
        }
        case ParserState::TERNARY_LHS: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(tern, Ternary);
            tern->lhs = expr;
            stateStack.back() = ParserState::TERNARY_RHS;
            stateStack.push_back(ParserState::EXPRESSION);
            goto reinterpret;
        }
        case ParserState::TERNARY_RHS: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(tern, Ternary);
            tern->rhs = expr;
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::CALL_PATH: {
            TRY_DOWNCAST_HEAD(path, Path);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(call, Call);
            call->functionPath = *path;
            stateStack.back() = ParserState::CALL_L_PAREN;
            goto reinterpret;
        }
        case ParserState::CALL_L_PAREN:
            EXPECT_SWAP_BREAK(PUNC_L_PAREN, CALL_ARGS_NEXT);
        case ParserState::CALL_ARGS_NEXT: {
            if (token.type == TokenType::PUNC_R_PAREN) {
                stateStack.pop_back();
                break;
            }
            stateStack.back() = ParserState::CALL_ARG_EXPR;
            stateStack.push_back(ParserState::EXPRESSION);
            goto reinterpret;
        }
        case ParserState::CALL_ARG_EXPR: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(call, Call);
            call->arguments.push_back(expr);
            stateStack.back() = ParserState::CALL_ARGS_COMMA;
        }
        case ParserState::CALL_ARGS_COMMA: {
            if (token.type == TokenType::PUNC_R_PAREN) {
                stateStack.back() = ParserState::CALL_ARGS_NEXT;
                goto reinterpret;
            }
            EXPECT_SWAP_BREAK(PUNC_COMMA, CALL_ARGS_NEXT);
        }
        case ParserState::IF_PREDICATE: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(ifelse, IfElse);
            ifelse->predicate = expr;
            stateStack.back() = ParserState::IF_TRUE;
            stateStack.push_back(ParserState::STATEMENT);
            goto reinterpret;
        }
        case ParserState::IF_TRUE: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(ifelse, IfElse);
            ifelse->truePath = stmt;
            stateStack.back() = ParserState::IF_ELSE;
            goto reinterpret;
        }
        case ParserState::IF_ELSE: {
            TRY_DOWNCAST_HEAD(ifelse, IfElse);
            if (token.type == TokenType::KW_ELSE) {
                stateStack.back() = ParserState::IF_FALSE;
                stateStack.push_back(ParserState::STATEMENT);
                break;
            }
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::IF_FALSE: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(ifelse, IfElse);
            ifelse->falsePath = stmt;
            stateStack.pop_back();
            goto reinterpret;
        }
        case ParserState::LIST_NEXT: {
            TRY_DOWNCAST_HEAD(list, List);
            if (token.type == TokenType::PUNC_R_BRACKET) {
                stateStack.pop_back();
                break;
            }
            stateStack.back() = ParserState::LIST_EXPR;
            stateStack.push_back(ParserState::EXPRESSION);
            goto reinterpret;
        }
        case ParserState::LIST_EXPR: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(list, List);
            list->members.push_back(expr);
            stateStack.back() = ParserState::LIST_COMMA;
            goto reinterpret;
        }
        case ParserState::LIST_COMMA: {
            if (token.type == TokenType::PUNC_R_BRACKET) {
                stateStack.back() = ParserState::LIST_NEXT;
                goto reinterpret;
            }
            EXPECT_SWAP_BREAK(PUNC_COMMA, LIST_NEXT);
        }
        case ParserState::MAP_KEY: {
            TRY_DOWNCAST_HEAD(map, Map);
            if (token.type == TokenType::PUNC_R_PAREN) {
                stateStack.pop_back();
                break;
            }
            stateStack.back() = ParserState::MAP_KEY_STRING;
            goto reinterpret;
        }
        case ParserState::MAP_KEY_STRING: {
            TRY_DOWNCAST_HEAD(map, Map);
            EXPECT_TYPE(LIT_STRING);
            auto key = unescapeString(token, filename);
            map->nextKey = key;
            stateStack.back() = ParserState::MAP_EQ;
            break;
        }
        case ParserState::MAP_EQ: {
            EXPECT_TYPE(PUNC_EQ);
            stateStack.back() = ParserState::MAP_VALUE;
            stateStack.push_back(ParserState::EXPRESSION);
            break;
        }
        case ParserState::MAP_VALUE: {
            TRY_DOWNCAST_HEAD(expr, Expression);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(map, Map);
            map->pairs[map->nextKey] = expr;
            stateStack.back() = ParserState::MAP_COMMA;
            goto reinterpret;
        }
        case ParserState::MAP_COMMA: {
            if (token.type == TokenType::PUNC_R_PAREN) {
                stateStack.back() = ParserState::MAP_KEY;
                goto reinterpret;
            }
            EXPECT_SWAP_BREAK(PUNC_COMMA, MAP_KEY);
        }
        case ParserState::TRY_STATEMENT: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(tryrecv, TryRecover);
            tryrecv->happyPath = stmt;
            stateStack.back() = ParserState::TRY_MAYBE_RECV;
            goto reinterpret;
        }
        case ParserState::TRY_MAYBE_RECV: {
            if (token.type != TokenType::KW_RECOVER) {
                stateStack.pop_back();
                goto reinterpret;
            }
            stateStack.back() = ParserState::RECV_PATH;
            std::shared_ptr<Path> path = std::make_shared<Path>();
            path->position = token.getPosition();
            treeCursor.push_back(path);
            stateStack.push_back(ParserState::PATH_IDENT);
            break;
        }
        case ParserState::RECV_PATH: {
            TRY_DOWNCAST_HEAD(path, Path);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(tryrecv, TryRecover);
            tryrecv->binding = *path;
            stateStack.back() = ParserState::RECV_STATEMENT;
            stateStack.push_back(ParserState::STATEMENT);
            goto reinterpret;
        }
        case ParserState::RECV_STATEMENT: {
            TRY_DOWNCAST_HEAD(stmt, Statement);
            treeCursor.pop_back();
            TRY_DOWNCAST_HEAD(tryrecv, TryRecover);
            tryrecv->sadPath = stmt;
            stateStack.pop_back();
            goto reinterpret;
        }
    }
}