#pragma once
#include <unordered_map>
#include <vector>
#include <filesystem>

#include "parsing.h"
#include "value.h"

namespace runtime {

    class AbstractFunction {
    public:
        virtual ~AbstractFunction() = default;
        virtual value::Value call(Stackframe &frame, std::vector<value::Value> & args) {
            throw exceptions::RuntimeError(frame, "internal error: tried to call an abstract function");
        }
    };

    class SyntaxFunction final: public AbstractFunction {
    public:
        std::vector<std::string> argumentNames;
        std::string name;
        exceptions::FilePosition pos;
        std::vector<std::shared_ptr<parsing::Statement>> body;

        value::Value call(Stackframe & frame, std::vector<value::Value> & args) override;
    };

    struct Module {
        std::string moduleName;
        std::unordered_map<std::string, std::shared_ptr<Module>> imported;
        std::unordered_map<std::string, value::Value> globals {};
        std::unordered_map<std::string, std::shared_ptr<AbstractFunction>> functions;

        std::shared_ptr<AbstractFunction> getFunction(Stackframe &frame, parsing::Path &path);

        explicit Module(bool isStdLib = false);
    };

    struct Stackframe {
        Stackframe * parent;
        std::shared_ptr<Module> root;
        size_t depth;
        std::unordered_map<std::string, value::Value> variables {};
        std::vector<std::shared_ptr<parsing::Statement>> body {};

        std::string functionName;
        exceptions::FilePosition sourcePos;
        bool boundary = false;

        value::Value * getVariable(const parsing::Path &path);

        void assignVariable(const parsing::Path & path, const value::Value & value);

        Stackframe branch(exceptions::FilePosition pos);
    };

    std::shared_ptr<Module> initModule(
        const std::filesystem::path &filepath,
        parsing::Root &root,
        Stackframe &frame,
        std::unordered_map<std::filesystem::path, std::shared_ptr<Module>> &handled,
        std::unordered_set<std::filesystem::path> cycles = {}
    );

    parsing::Root parseFile(std::filesystem::path &path);
}
