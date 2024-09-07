#pragma once

#include "lexer.h"

namespace value {
    class AbstractValue {
    public:
        virtual ~AbstractValue() = default;
        virtual std::string to_string();

    };

    class Null final: public AbstractValue {
    public:
        std::string to_string() override { return "null"; }
    };

    class Boolean final : public AbstractValue {
    public:
        bool value;

        std::string to_string() override { return std::to_string(value); }
    };

    class Number final : public AbstractValue {
    public:
        double value;

        std::string to_string() override { return std::to_string(value); }
    };

    class String final : public AbstractValue {
    public:
        std::string value;

        std::string to_string() override { return value; }
    };
}
