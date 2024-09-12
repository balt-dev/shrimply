/// This is the stackframe depth limit. Adjust if you're running into errors.
#define DEPTH_LIMIT 1024

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <cmath>
#include <list>

#include "runtime.h"

#include <fstream>
#include <iostream>

#include "parsing.h"
#include "value.h"

using namespace runtime;
using exceptions::RuntimeError;
using lexer::TokenType;
using value::Value;

Value parsing::Ternary::result(Stackframe &frame) {
    frame.sourcePos = position;
    auto pred = predicate->result(frame);
    if (pred.asBoolean())
        return lhs->result(frame);
    return rhs->result(frame);
}

Value * parsing::Ternary::pointer(Stackframe &frame) {
    frame.sourcePos = position;
    auto pred = predicate->result(frame);
    if (pred.asBoolean())
        return lhs->pointer(frame);
    return rhs->pointer(frame);
}


Value parsing::BinaryOp::result(Stackframe & frame) {
    frame.sourcePos = position;
    switch (opr.inner()) {
        case TokenType::PUNC_INDEX: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            if ( left.getTag() == Value::ValueType::String ) {
                auto str = left.asString();
                long long right;
                auto r = rhs->result(frame);
                if (!r.asInteger(right)) throw RuntimeError(frame, "cannot index string using " + r.raw_string());
                if (right < 0 || right >= str.size()) throw RuntimeError(frame, "string index is out of bounds: " + std::to_string(right));
                return Value(std::string(1, str.c_str()[right]));
            }
            if ( std::shared_ptr<std::vector<Value>> list {}; left.asList(list) ) {
                long long right;
                auto r = rhs->result(frame);
                if (!r.asInteger(right)) throw RuntimeError(frame, "cannot index list using " + r.raw_string());
                if (right < 0 || right >= list->size()) throw RuntimeError(frame, "list index is out of bounds: " + std::to_string(right));
                return list->at(right);
            }
            if ( std::shared_ptr<std::unordered_map<std::string, Value>> map {}; lhs->result(frame).asMap(map) ) {
                auto index = rhs->result(frame);
                auto access = index.asString();
                auto iter = map->find(access);
                if (iter == map->end()) throw RuntimeError(frame, "index does not exist in map: " + index.raw_string());
                return iter->second;
            }
            throw RuntimeError(frame, "cannot index into value " + left.raw_string());
        }
        case TokenType::PUNC_EQ: {
            // Assignment
            auto ptr = lhs->pointer(frame);
            frame.sourcePos = rhs->position;
            *ptr = rhs->result(frame);
            return Value {};
        }
        case TokenType::PUNC_PLUS: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            auto right = rhs->result(frame);
            if (left.getTag() == Value::ValueType::String || right.getTag() == Value::ValueType::String)
                return Value(left.asString() + right.asString());
            if (left.getTag() == Value::ValueType::Integer && right.getTag() == Value::ValueType::Integer)
                return Value(left.integer + right.integer);
            if (double x, y; left.asNumber(x) && right.asNumber(y))
                return Value(x + y);
            throw RuntimeError(frame, "cannot add values " + left.raw_string() + " and " + right.raw_string());
        }
        case TokenType::PUNC_MINUS: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            auto right = rhs->result(frame);
            if (left.getTag() == Value::ValueType::Integer && right.getTag() == Value::ValueType::Integer)
                return Value(left.integer - right.integer);
            if (double x, y; left.asNumber(x) && right.asNumber(y))
                return Value(x - y);
            throw RuntimeError(frame, "cannot subtract values " + left.raw_string() + " and " + right.raw_string());
        }
        case TokenType::PUNC_MULT: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            auto right = rhs->result(frame);
            if (
                long long count;
                left.getTag() == Value::ValueType::String && right.asInteger(count)
            ) {
                std::ostringstream ss;
                for (auto i = 0; i < count; i++) {
                    ss << left.string;
                }
                return Value(ss.str());
            }
            if (left.getTag() == Value::ValueType::Integer && right.getTag() == Value::ValueType::Integer)
                return Value(left.integer * right.integer);
            if (double x, y; left.asNumber(x) && right.asNumber(y))
                return Value(x * y);
            throw RuntimeError(frame, "cannot multiply values " + left.raw_string() + " and " + right.raw_string());
        }
        case TokenType::PUNC_DIV:
        case TokenType::PUNC_MOD: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            auto right = rhs->result(frame);
            if (left.getTag() == Value::ValueType::Integer && right.getTag() == Value::ValueType::Integer) {
                if (right.integer == 0) throw RuntimeError(frame, "integer division by zero");
                return Value(
                    opr.inner() == TokenType::PUNC_DIV
                    ? left.integer / right.integer
                    : left.integer % right.integer
                );
            }
            if (double x, y; left.asNumber(x) && right.asNumber(y))
                return Value(
                    opr.inner() == TokenType::PUNC_DIV
                    ? x / y
                    : std::fmod(x, y)
                );
            throw RuntimeError(frame, "cannot divide values " + left.raw_string() + " and " + right.raw_string());
        }
        case TokenType::PUNC_DOUBLE_EQ: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            return Value(left == rhs->result(frame));
        }
        case TokenType::PUNC_NEQ: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            return Value(!(left == rhs->result(frame)));
        }
#define CMP(type, opr) \
        case TokenType::type: { \
            auto left = lhs->result(frame); \
            frame.sourcePos = rhs->position; \
            auto right = rhs->result(frame); \
            double x, y;  \
            if (!(left.asNumber(x) && right.asNumber(y))) return Value(left.to_string() opr right.to_string()); \
            return Value(x opr y); \
        }
        CMP(PUNC_LT, <);
        CMP(PUNC_GT, >);
        CMP(PUNC_LEQ, <=);
        CMP(PUNC_GEQ, >=);
#define BIT(type, opr, name) \
        case TokenType::type: { \
            auto left = lhs->result(frame); \
            frame.sourcePos = rhs->position; \
            auto right = rhs->result(frame); \
            long long x, y;  \
            if (!(left.asInteger(x) && right.asInteger(y))) throw RuntimeError(frame, \
                "cannot apply bitwise " name " to values " + \
                left.raw_string() + " and " + right.raw_string()\
            ); \
            return Value(x opr y); \
        }
        BIT(PUNC_AMPERSAND, &, "and");
        BIT(PUNC_BITOR, |, "and");
        BIT(PUNC_SHL, <<, "left shift");
        BIT(PUNC_SHR, >>, "right shift");
        case TokenType::PUNC_XOR: {
            auto left = lhs->result(frame);
            frame.sourcePos = rhs->position;
            auto right = rhs->result(frame);
            if (left.tag == Value::ValueType::Boolean && right.tag == Value::ValueType::Boolean)
                return Value(left.boolean != right.boolean);
            long long x, y;
            if (left.asInteger(x) && right.asInteger(y))
                return Value(left.integer ^ right.integer);
            throw RuntimeError(frame, "cannot apply binary xor to values " + left.raw_string() + " and " + right.raw_string() );
        }
#define LOG(type, opr, inverse) \
        case TokenType::type: { \
            auto left = lhs->result(frame).asBoolean(); \
            if (inverse left) return Value(left); \
            frame.sourcePos = rhs->position; \
            auto right = rhs->result(frame).asBoolean(); \
            return Value(left opr right); \
        }
        LOG(PUNC_AND, &&, !);
        LOG(PUNC_OR, ||, );
        default:
            throw RuntimeError(
                frame,
                "internal error: BinaryOp opr was not a valid operand: " + opr.to_string()
            );
    }
}
Value parsing::UnaryOp::result(Stackframe & frame) {
    frame.sourcePos = position;
    switch (opr.inner()) {
        case TokenType::PUNC_NOT:
            return Value(!value->result(frame).asBoolean());
        default:
            throw RuntimeError(
                frame,
                "internal error: UnaryOp opr was not a valid operand: " + opr.to_string()
            );
    }
}

Value parsing::Call::result(Stackframe & frame) {
    frame.sourcePos = position;

    auto fn = frame.root->getFunction(frame, functionPath);

    std::vector<Value> args {};
    args.reserve(arguments.size());
    for (const auto& arg : arguments) {
        frame.sourcePos = arg->position;
        args.push_back(arg->result(frame));
    }

    return fn->call(frame, args);
}

Value *parsing::BinaryOp::pointer(Stackframe &frame) {
    frame.sourcePos = position;

    if (opr != TokenType::PUNC_INDEX) goto error;
    {
        auto target = lhs->result(frame);
        if (
            std::shared_ptr<std::vector<Value>> list;
            target.asList(list)
        ) {
            frame.sourcePos = rhs->position;
            auto left = lhs->result(frame);
            auto index = rhs->result(frame);
            long long num;
            if (!index.asInteger(num))
                throw RuntimeError(frame, "cannot index list using " + index.raw_string());
            if (num < 0 || num >= list->size())
                throw RuntimeError(frame, "list index is out of bounds: " + std::to_string(num));
            return &list->at(num);
        }
        if (
            std::shared_ptr<std::unordered_map<std::string, Value>> map;
            target.asMap(map)
        ) {
            frame.sourcePos = rhs->position;
            auto index = rhs->result(frame);
            auto access = index.asString();
            if (map->find(access) == map->end()) {
                map->operator[](access) = Value();
            }
            return &map->at(access);
        }
    }

    error:
    throw RuntimeError(frame, "expression does not support assignment: " + lhs->to_string());
}

Value parsing::Path::result(Stackframe &frame) {
    return *pointer(frame);
}

Value* parsing::Path::pointer(Stackframe &frame) {
    return frame.getVariable(*this);
}

Value * Stackframe::getVariable(const parsing::Path &path) {
    if (path.members.empty())
        throw RuntimeError(*this, "internal error: tried to resolve variable with empty path");
    if (path.members.size() == 1) {
        // This is a lone identifier, which names variable in local scope
        const auto& name = path.members.back();
        auto current = this;
        while (current) {
            auto it = current->variables.find(name);
            if (it != current->variables.end()) return &it->second;
            if (current->boundary) break;
            current = current->parent;
        };
        // Check globals
        auto it = root->globals.find(name);
        if (it != root->globals.end()) return &it->second;
        throw RuntimeError(
            *this,
            "could not find variable \"" + name +  "\" in scope"
        );
    }

    // This is a scoped identifier, look for imported modules
    {
        std::shared_ptr<Module> current = root;
        auto iter = path.members.cbegin();
        while (iter != path.members.cend() - 1) {
            auto it = current->imported.find(*iter);
            if (it == current->imported.end()) goto error;
            current = it->second;
            ++iter;
        }
        auto it = current->globals.find(path.members.back());
        if (it != current->globals.end()) return &it->second;
    }
    error:
    throw RuntimeError(*this, "could not resolve variable path: " + path.to_string());
}

std::shared_ptr<AbstractFunction> Module::getFunction(Stackframe & frame, parsing::Path &path) {
    auto mod = this;
    for (auto it = path.members.begin(); it != (path.members.end() - 1); ++it) {
        auto found = mod->imported.find(*it);
        if (found == mod->imported.end())
            throw RuntimeError(frame, "could not resolve function path: " + path.to_string());
        mod = &*found->second;
    }
    auto const fn = mod->functions.find(path.members.back());
    if (fn == mod->functions.end())
        throw RuntimeError(frame, "could not resolve function path: " + path.to_string());
    return fn->second;
}


void Stackframe::assignVariable(const parsing::Path &path, const Value &value) {
    if (path.members.size() == 1)
        variables[path.members.back()] = value;
    else
        *getVariable(path) = value;
}

Stackframe Stackframe::branch(exceptions::FilePosition pos) {
    if (depth > DEPTH_LIMIT)
        throw exceptions::RuntimeError(*this, "reached call depth limit" );
    auto child = *this;
    child.boundary = false;
    child.depth += 1;
    child.parent = this;
    child.variables = {};
    child.sourcePos = pos;
    return child;
}


Value parsing::List::result(Stackframe &frame) {
    frame.sourcePos = position;
    auto vec = std::make_shared<std::vector<Value>>();
    vec->reserve(members.size());
    for (const auto& expr : members) {
        frame.sourcePos = expr->position;
        vec->push_back(expr->result(frame));
    }
    return Value(vec);
}

Value parsing::Map::result(Stackframe &frame) {
    frame.sourcePos = position;
    auto map = std::make_shared<std::unordered_map<std::string, Value>>();
    map->reserve(pairs.size());
    for (const auto & pair : pairs) {
        map->operator[](pair.first) = pair.second->result(frame);
    }
    return Value(map);
}

parsing::Root runtime::parseFile(std::filesystem::path & path) {
    std::string fileContents;
    {
        // Read the file to a string
        std::ifstream file ( path );
        if (!file) {
            throw std::runtime_error("filesystem error: couldn't open file for reading");
        }

        // Read until EOF (or until a null byte but oh well)
        std::getline(file, fileContents, '\0');

        if (!file) {
            throw std::runtime_error("filesystem error: failed to read file");
        }
    }

    // Tokenize and parse the AST
    lexer::Lexer lexer ( fileContents, path );
    parsing::Root syntaxTree;
    parsing::Parser parser ( path );
    lexer::Token token;
    while (true) {
        auto res = lexer.advanceToken(token);
        // Feed it to the parser
        parser.advance(token);
        if (!res) break;
    }
    return parser.getSyntaxTree();
}

std::list<std::filesystem::path> parsePaths() {
    auto paths = getenv("SHRIMPLY_MOD_PATHS");
    if (!paths) return {};
    std::string rawPaths { paths };
    std::list<std::filesystem::path> pathList {};
    size_t pos;
    while ((pos = rawPaths.find(';')) != std::string::npos) {
        try {
            pathList.emplace_back( rawPaths.substr(0, pos) );
        } catch (std::exception & err) {
            std::cerr << "failed to parse SHRIMPLY_MOD_PATHS environment variable: " << err.what() << std::endl;
            std::terminate();
        }
        rawPaths.erase(0, pos + 1);
    }
    return pathList;
}
static std::list<std::filesystem::path> searchPaths = parsePaths();

std::shared_ptr<Module> runtime::initModule(
    const std::filesystem::path & filepath,
    parsing::Root & root,
    Stackframe & frame,
    std::unordered_map<std::filesystem::path, std::shared_ptr<Module>> & handled,
    std::unordered_set<std::filesystem::path> cycles
) {
    cycles.insert(canonical(filepath));
    auto module = std::make_shared<Module>();
    frame.root = module;
    // First, we scan for imports
    for (const auto& item : root.items) {
        if (
            const auto use = std::dynamic_pointer_cast<parsing::Use>(item)
        ) {
            // Try to find a file to import
            auto folderName = use->module.members.front();
            auto moduleName = use->module.members.back();
            auto paths = searchPaths;
            paths.push_front(filepath.parent_path());
            std::filesystem::path importPath;
            for (const auto& path : paths) {
                try {
                    auto children = std::filesystem::directory_iterator(path);
                    for (const auto& child : children) {
                        const auto& childPath = child.path();
                        if (childPath.stem() == folderName) {
                            importPath = path;
                            goto foundFolder;
                        }
                    }
                } catch (std::exception & err) {
                    throw RuntimeError(frame, "failed to read path: " + std::string(err.what()));
                }
            }
            throw RuntimeError(frame, "could not resolve module path: " + use->module.to_string());

        foundFolder:
            // Search recursively for the right file
            for (auto iter = use->module.members.begin(); iter != use->module.members.end(); ++iter) {
                const auto& member = *iter;

                auto children = std::filesystem::directory_iterator(importPath);
                auto found = false;
                for (const auto& child : children) {
                    const auto& childPath = child.path();
                    if (childPath.stem() == member) {
                        importPath /= member;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    throw RuntimeError(
                        frame,
                        "could not resolve \"" + member + "\" in path \"" + importPath.generic_string() + "\": " +
                        use->module.to_string()
                    );
            }

            importPath.replace_extension(".spl");
            auto path = canonical(importPath);
            // Check for dependency cycle
            if (cycles.count(path))
                throw RuntimeError(frame, "dependency cycle detected for module " + use->module.to_string());

            auto it = handled.find(path);
            if (it != handled.end()) {
                module->imported[moduleName] = it->second;
                continue;
            }

            // Need to parse the file
            try {
                auto moduleRoot = parseFile(path);
                auto childFrame = frame.branch(use->position);
                auto parsedModule = initModule(path, moduleRoot, childFrame, handled, cycles);
                parsedModule->moduleName = moduleName;
                handled[path] = parsedModule;
                module->imported[moduleName] = parsedModule;
            } catch (RuntimeError & err) {
                throw RuntimeError(frame, "failed to load module at \"" + path.generic_string() + "\": " + err.message);
            } catch (std::exception & err) {
                throw RuntimeError(frame, "failed to load module at \"" + path.generic_string() + "\": " + err.what());
            }
        }
    }
    // Then, we scan for functions
    for (const auto& item : root.items) {
        if (
            const auto fn = std::dynamic_pointer_cast<parsing::Function>(item)
        ) {
            const auto synFn = std::make_shared<SyntaxFunction>();
            synFn->name = fn->name;
            synFn->pos = fn->position;
            synFn->argumentNames = fn->arguments;

            if ( const auto fnBlock = std::dynamic_pointer_cast<parsing::Block>(fn->body) )
                synFn->body = fnBlock->statements;
            else synFn->body = { fn->body };
            module->functions[fn->name] = synFn;
        }
    }
    // Now, we scan for globals
    for (const auto& item : root.items) {
        if (
            const auto decl = std::dynamic_pointer_cast<parsing::Declaration>(item)
        ) {
            module->globals[decl->name] = decl->value->result(frame);
        }
    }
    return module;
}

#define IF_DOWNCAST(type, name) if (auto name = std::dynamic_pointer_cast<type>(stmt))

struct LoopBreak { Stackframe & frame; };
struct LoopContinue { Stackframe & frame; };
struct FnReturn { Value value; };

using namespace parsing;

void handleFrame(Stackframe & frame);

void handleStatement(Stackframe & frame, const std::shared_ptr<Statement>& stmt) {
    frame.sourcePos = stmt->position;

    IF_DOWNCAST(Block, block) {
        Stackframe childFrame = frame.branch(block->position);
        childFrame.body = block->statements;
        handleFrame(childFrame);
    } else IF_DOWNCAST(ExpressionStatement, expr) {
        expr->expr->result(frame);
    } else IF_DOWNCAST(IfElse, ifelse) {
        auto predicate =
            ifelse->predicate->result(frame)
            .asBoolean();
        auto path = predicate ? ifelse->truePath : ifelse->falsePath;
        if (path) { // These may be null
            Stackframe childFrame = frame.branch(path->position);
            handleStatement(childFrame, path);
        }
    } else IF_DOWNCAST(TryRecover, tryrecv) {
        Stackframe childFrame = frame.branch(tryrecv->position);
        try {
            handleStatement(childFrame, tryrecv->happyPath);
        } catch (RuntimeError & err) {
            if (!tryrecv->binding.members.empty()) {
                childFrame.variables = {};
                childFrame.assignVariable(tryrecv->binding, Value(err.message));
                handleStatement(childFrame, tryrecv->sadPath);
            }
        }
    } else IF_DOWNCAST(Loop, loop) {
        while (true) {
            try {
                Stackframe childFrame = frame.branch(loop->position);
                childFrame.body = {loop->body};
                handleFrame(childFrame);
            }
            catch (LoopBreak & _) { break; }
            catch (LoopContinue & _) {}
        }
    } else IF_DOWNCAST(Declaration, decl) {
        auto res = decl->value->result(frame);
        frame.variables[decl->name] = res;
    }
    else IF_DOWNCAST(Break, brk)
        throw LoopBreak {frame};
    else IF_DOWNCAST(Continue, cont)
        throw LoopContinue {frame};
    else IF_DOWNCAST(Return, ret)
        throw FnReturn { ret->value->result(frame) };
    else
        throw RuntimeError(frame, "internal error: could not downcast " + stmt->to_string());
}

void handleFrame(Stackframe & frame) {
    for (const auto& stmt : frame.body) {
        frame.sourcePos = stmt->position;
        handleStatement(frame, stmt);
    }
}

Value SyntaxFunction::call(Stackframe &frame, std::vector<Value> & args) {
    std::unordered_map<std::string, Value> variables = {};
    auto iter = argumentNames.begin();
    for (const auto& arg : args) {
        if (iter == argumentNames.end()) break;
        variables[*iter] = arg;
        ++iter;
    }
    for (; iter != argumentNames.end(); ++iter) {
        variables[*iter] = Value();
    }
    variables["__ARGC"] = Value((long long) args.size());
    auto childFrame = frame.branch(pos);
    childFrame.variables = variables;
    childFrame.functionName = name;
    childFrame.body = body;
    childFrame.boundary = true;

    try {
        handleFrame(childFrame);
    } catch (LoopBreak brk) {
        throw RuntimeError(brk.frame, "unhandled break statement");
    } catch (LoopContinue cont) {
        throw RuntimeError(cont.frame, "unhandled continue statement");
    } catch (FnReturn ret) {
        return ret.value;
    }

    return {};
}
