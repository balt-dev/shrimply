#pragma once

#include <unordered_map>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "lexer.h"

namespace parsing {
    class UnaryOp;
    class BinaryOp;
}

namespace value {
    struct Null {};

    class Value final {
        friend parsing::BinaryOp;
        friend parsing::UnaryOp;
    public:
        enum struct ValueType {
            Null,
            Integer,
            Number,
            Boolean,
            String,
            List,
            Map,
            Extern
        };
        static unsigned long long counter;

        // Note: These were originally private, but I stopped caring.
        // Nobody else is working on this anyways.
        union {
            long long integer;
            double number;
            bool boolean;
            std::string string;
            std::shared_ptr<std::vector<Value>> list;
            std::shared_ptr<std::unordered_map<std::string, Value>> map;
            void* external;
        };
        ValueType tag;

        unsigned long long id;

        void initFrom(const Value& source) {
            tag = source.tag;
            id = source.id;
            switch (source.tag) {
                case ValueType::Null: break;
                case ValueType::Integer: integer = source.integer; break;
                case ValueType::Number: number = source.number; break;
                case ValueType::Boolean: boolean = source.boolean; break;
                case ValueType::String: new (&string) std::string(source.string); break;
                case ValueType::List: new (&list) std::shared_ptr(source.list); break;
                case ValueType::Map: new (&map) std::shared_ptr(source.map); break;
                case ValueType::Extern: external = source.external; break;
            }
        }

        Value(): boolean{false}, id(counter++), tag(ValueType::Null) {}

        ~Value() {
            if (tag == ValueType::String) string.~basic_string();
            if (tag == ValueType::List) list.~shared_ptr();
            if (tag == ValueType::Map) map.~shared_ptr();
        }

        Value(const Value& source): tag(source.tag), id(source.id) {
            initFrom(source);
        }

        Value& operator=(const Value& source) {
            this->~Value();
            initFrom(source);
            return *this;
        }

        bool operator==(const Value& other) const {
            if (tag != other.tag) return false;
            switch (tag) {
                case ValueType::Null: return true;
                case ValueType::Integer: return integer == other.integer;
                case ValueType::Number: return number == other.number;
                case ValueType::Boolean: return boolean == other.boolean;
                case ValueType::String: return string == other.string;
                case ValueType::List: return list == other.list;
                case ValueType::Map: return map == other.map;
                case ValueType::Extern: return external == other.external;
            }
            return false;
        }

        explicit Value(const long long val): tag(ValueType::Integer), integer{val}, id(counter++) {}
        explicit Value(const double val): tag(ValueType::Number), number{val}, id(counter++) {}
        explicit Value(const bool val): tag(ValueType::Boolean), boolean{val}, id(counter++) {}
        explicit Value(std::string val): tag(ValueType::String), string{std::move(val)}, id(counter++) {}
        explicit Value(const std::shared_ptr<std::vector<Value>>& val): tag(ValueType::List), list{val}, id(counter++) {}
        explicit Value(const std::shared_ptr<std::unordered_map<std::string, Value>>& val): tag(ValueType::Map), map{val}, id(counter++) {}

        // Note: This can't actually be a constructor! It would clash with the string one.
        static Value fromPointer(void* ptr) {
            Value value;
            value.tag = ValueType::Extern;
            value.external = ptr;
            return value;
        }
        ValueType getTag() const {
            return tag;
        }

        bool asInteger(long long & out) const {
            if (tag == ValueType::Number) out = number;
            if (tag == ValueType::Boolean) out = boolean;
            if (tag == ValueType::Integer) out = integer;
            return tag == ValueType::Integer || tag == ValueType::Boolean || tag == ValueType::Number;
        }

        bool asNumber(double & out) const {
            if (tag == ValueType::Number) out = number;
            if (tag == ValueType::Boolean) out = boolean;
            if (tag == ValueType::Integer) out = integer;
            return tag == ValueType::Integer || tag == ValueType::Boolean  || tag == ValueType::Number;
        }

        bool asBoolean() const {
            switch (tag) {
                case ValueType::Null: return false;
                case ValueType::Boolean: return boolean;
                case ValueType::Integer: return integer > 0;
                case ValueType::Number: return number > 0;
                case ValueType::String: return !string.empty();
                case ValueType::List: return !list->empty();
                case ValueType::Map: return !map->empty();
                case ValueType::Extern: return false;
                default: return false;
            }
        }

        std::string raw_string(std::unordered_set<unsigned long long> seenIds = {}) const;
        std::string to_string() const {
            return tag == ValueType::String ? string : raw_string();
        };

        std::string asString() const {
            return to_string();
        }

        bool asList(std::shared_ptr<std::vector<Value>> & out) const {
            if (tag == ValueType::List) out = list;
            return tag == ValueType::List;
        }

        bool asMap(std::shared_ptr<std::unordered_map<std::string, Value>> & out) const {
            if (tag == ValueType::Map) out = map;
            return tag == ValueType::Map;
        }
    };
}
