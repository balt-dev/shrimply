#include <iostream>
#include <fstream>
#include <vector>

#include "lexer.h"

#define TESTING

std::vector<lexer::Token> tokenize(std::string file) {
    std::vector<lexer::Token> tokens;
    lexer::Lexer lexer ( file );
    lexer::Token token;
    // Trust me, a goto was better here.
    while (lexer.advanceToken(token))
        tokens.push_back(token);
    return tokens;
}

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

    // Tokenize
    std::vector<lexer::Token> tokens;
    try {
        tokens = tokenize(fileContents);
    } catch (exceptions::SyntaxError& err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

#ifdef TESTING
    std::cerr << "Tokens: ";
    for (auto token : tokens) {
        std::cerr << token.to_string() << " ";
    }
    std::cerr << std::endl;
#endif

    // Now that we have the tokens, convert them to an AST

    return 0;
}