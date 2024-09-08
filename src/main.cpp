#include <iostream>
#include <fstream>

#include "lexer.h"
#include "parsing.h"

int main( int argc, char * argv[]) {
    if (argc <= 1) {
        std::cerr << "Usage: simply.exe <filename>" << std::endl;
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
        while (lexer.advanceToken(token)) {
            // Feed it to the parser
            parser.advance(token);
        }
        syntaxTree = parser.getSyntaxTree();
    } catch (const std::exception & err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    return 0;
}