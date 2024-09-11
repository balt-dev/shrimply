#include <iostream>
#include <fstream>

#include "lexer.h"
#include "parsing.h"
#include "runtime.h"
#include "standardlib.h"

int main( int argc, char * argv[]) {
    if (argc <= 1) {
        std::cerr << "Usage: simply.exe <filename> [args...]" << std::endl;
        return 0;
    }

    std::string filename (argv[1]);
    std::string fileContents;

    {
        // Read the file to a string
        std::ifstream file ( filename );
        if (!file) {
            std::cerr << "Couldn't open file for reading" << std::endl;
            return 1;
        }

        // Read until EOF (or until a null byte but oh well)
        std::getline(file, fileContents, '\0');

        if (!file) {
            std::cerr << "Failed to read file" << std::endl;
            return 1;
        }
    }

    // Tokenize and parse the AST
    lexer::Lexer lexer ( fileContents );
    parsing::Parser parser;
    parsing::Root syntaxTree;
    try {
        lexer::Token token;
        while (true) {
            auto res = lexer.advanceToken(token);
            // Feed it to the parser
            parser.advance(token);
            if (!res) break;
        }
        syntaxTree = parser.getSyntaxTree();
    } catch (const std::exception & err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    Stackframe rootFrame {
        nullptr,
        nullptr,
        0,
        {}, {},
        "<root>", {},
        false
    };

    auto module = initModule(syntaxTree, rootFrame);
    rootFrame.root = &module;

    if (module.functions.find("main") == module.functions.end()) {
        std::cerr << "no main function found" << std::endl;
        return 0;
    }

    auto main = module.functions["main"];
    if (main->argumentNames.size() != 1) {
        throw exceptions::RuntimeError(rootFrame, "main function must have exactly one argument");
    }

    auto args = std::make_shared<std::vector<value::Value>>();
    for (int i = 1; i < argc; i++) {
        std::string arg { argv[i] };
        args->emplace_back(arg);
    }

    // TODO: Don't just inject them! Use a namespace! Figure that shit out! Namespaces are cool and good!
    stdlib::inject(module.functions);

    try {
        module.functions["main"]->call(rootFrame, { value::Value(args) });
    } catch (const exceptions::RuntimeError & err) {
        std::cerr << err.what() << std::endl;
        return -1;
    }

    return 0;
}
