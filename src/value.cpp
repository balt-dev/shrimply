#include "../include/value.h"

#include <unordered_set>
using namespace value;

unsigned long long Value::counter = 0;

std::string Value::raw_string(std::unordered_set<unsigned long long> seenIds) const {
    if (seenIds.find(id) != seenIds.end()) return "...";
    switch (tag) {
        case ValueType::Null: return "null";
        case ValueType::String: return "\"" + string + "\"";
        case ValueType::Boolean: return boolean ? "true" : "false";
        case ValueType::Integer: return std::to_string(integer);
        case ValueType::Number: return std::to_string(number);
        case ValueType::List: {
            seenIds.insert(id);
            std::stringstream stream;
            stream << "[";
            for (size_t i = 0; i < list->size(); i++) {
                if (i != 0) stream << ", ";
                auto value = list->at(i);
                stream << value.raw_string(seenIds);
            }
            stream << "]";
            return stream.str();
        }
        case ValueType::Map: {
            seenIds.insert(id);
            std::stringstream stream;
            stream << "(";
            size_t i = 0;
            for (const auto& pair : *map) {
                if (i != 0) stream << ", ";
                stream << "\"" << pair.first << "\"" << ": ";
                stream << pair.second.raw_string(seenIds);
                i++;
            }
            stream << ")";
            return stream.str();
        }
        case ValueType::Extern: {
            std::stringstream stream;
            stream << "<extern " << external << ">";
            return stream.str();
        }
        default:
            throw std::runtime_error("internal runtime error: malformed value");
    }
}