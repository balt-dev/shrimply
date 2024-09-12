#include <filesystem>
#include <iostream>
#include <fstream>

#include "lexer.h"
#include "parsing.h"
#include "runtime.h"

int main( int argc, char * argv[]) {
    if (argc <= 1) {
        std::cerr << "Usage: <filename> [args...]" << std::endl;
        return 0;
    }

    std::filesystem::path filename;
    try {
        filename = std::filesystem::path(argv[1]);
    } catch (std::exception& _) {
        std::cerr << "filesystem error: couldn't parse filename" << std::endl;
        return 1;
    }

    parsing::Root syntaxTree;
    try {
        syntaxTree = runtime::parseFile(filename);
    } catch (std::exception & err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    runtime::Stackframe rootFrame {
        nullptr,
        nullptr,
        0,
        {}, {},
        "<root>", {},
        false
    };

    std::unordered_map<std::filesystem::path, std::shared_ptr<runtime::Module>> seen {};
    try {
        auto module = initModule(filename, syntaxTree, rootFrame, seen);
        module->moduleName = "<root>";

        if (module->functions.find("main") == module->functions.end()) {
            std::cerr << "no main function found" << std::endl;
            return 0;
        }

        auto mainAbs = module->functions["main"];
        // there's no way for main to not be a SyntaxFunction, this is fine
        auto main = std::dynamic_pointer_cast<runtime::SyntaxFunction>(mainAbs);
        if (main->argumentNames.size() != 1) {
            throw exceptions::RuntimeError(rootFrame, "main function must have exactly one argument");
        }

        auto args = std::make_shared<std::vector<value::Value>>();
        for (int i = 1; i < argc; i++) {
            std::string arg { argv[i] };
            args->emplace_back(arg);
        }
        std::vector arglist { value::Value(args) };

        module->functions["main"]->call(rootFrame, arglist);
    } catch (const exceptions::RuntimeError & err) {
        std::cerr << err.what() << std::endl;
        return -1;
    }

    return 0;
}
