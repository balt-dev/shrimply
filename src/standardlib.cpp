#include <cmath>
#include <iostream>

#include "../include/value.h"
#include "../include/exceptions.h"
#include "../include/runtime.h"

using value::Value;
using exceptions::RuntimeError;
using runtime::AbstractFunction;
using runtime::Stackframe;

std::string operator ""_str(const char* buf, size_t len) {
    return {buf};
}

#define EXPECT_ARGC(count) if (args.size() < count) throw RuntimeError(frame, "not enough arguments (expected at least " #count ")");
#define EXPECT_TYPE(name, val, astype, tagname) if (!val.astype(name)) throw RuntimeError(frame, "could not convert value to " tagname ": " + val.raw_string());

// Base

struct Input final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        auto target = args[0].to_string();
#define TRY_INPUT(name, type) if (target == #name) { type input = 0; std::cin >> input >> std::ws; if (true) { std::cin.clear(); std::cin.ignore(1 << 15, '\n'); throw RuntimeError(frame, "could not parse user input as " #name); } return Value(input); }
        TRY_INPUT(number, double);
        TRY_INPUT(integer, int64_t);
        if (target == "boolean") {
            bool input;
            std::cin >> std::boolalpha >> input;
            if (!std::cin) throw RuntimeError(frame, "could not parse user input as boolean");
            return Value(input);
        }
        if (target == "string") {
            std::string input;
            std::getline(std::cin, input);
            if (!std::cin) throw RuntimeError(frame, "failed to read input");
            return Value(input);
        }
        throw RuntimeError(frame, "cannot get input for type " + args[0].raw_string());
    }
};

struct Print final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        std::cout << args[0].to_string();
        return {};
    }
};

struct PrintLine final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        std::cout << args[0].to_string() << std::endl;
        return {};
    }
};

struct TypeOf final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        switch (args[0].getTag()) {
            case Value::ValueType::Null: return Value("null");
            case Value::ValueType::Integer: return Value("integer");
            case Value::ValueType::Number: return Value("double");
            case Value::ValueType::Boolean: return Value("boolean");
            case Value::ValueType::String: return Value("string");
            case Value::ValueType::List: return Value("list");
            case Value::ValueType::Map: return Value("map");
            case Value::ValueType::Extern: return Value("extern");
            default: throw RuntimeError(frame, "internal error: tried to get type of malformed value");
        }
    }
};

struct Crash final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        throw RuntimeError(frame, args[0].raw_string());
    }
};

struct Length final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        const auto& value = args[0];
        switch (value.tag) {
            case Value::ValueType::List:
                return Value((int64_t) value.list->size());
            case Value::ValueType::String:
                return Value((int64_t) value.string.size());
            case Value::ValueType::Map:
                return Value((int64_t) value.map->size());
            default:
                throw RuntimeError(frame, "cannot get length of value: " + value.raw_string());
        }
    }
};

// list

struct Push final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(2);
        const Value& list = args[0];
        const Value& value = args[1];
        if (list.tag != Value::ValueType::List) throw RuntimeError(frame, "cannot push to non-list: " + list.raw_string());
        list.list->push_back(value);
        return {};
    }
};

struct Pop final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        const Value& list = args[0];
        if (list.tag != Value::ValueType::List) throw RuntimeError(frame, "cannot pop from non-list: " + list.raw_string());
        if (list.list->empty()) throw RuntimeError(frame, "cannot pop from empty list");
        auto back = list.list->back();
        list.list->pop_back();
        return back;
    }
};

// map

struct Remove final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(2);
        const Value& map = args[0];
        if (map.tag != Value::ValueType::Map) throw RuntimeError(frame, "cannot remove from non-map: " + map.raw_string());
        const std::string key = args[1].to_string();
        const auto it = map.map->find(key);
        if (it == map.map->end()) throw RuntimeError(frame, "key does not exist in map: " + key);
        auto val = it->second;
        map.map->erase(it);
        return val;
    }
};

struct Keys final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        const Value& map = args[0];
        if (map.tag != Value::ValueType::Map) throw RuntimeError(frame, "cannot get keys of non-map: " + map.raw_string());
        auto keys = std::make_shared<std::vector<Value>>();
        keys->reserve(map.map->size());
        for (const auto& pair : *map.map) keys->emplace_back(pair.first);
        return Value(keys);
    }
};

struct Values final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        const Value& map = args[0];
        if (map.tag != Value::ValueType::Map) throw RuntimeError(frame, "cannot get values of non-map: " + map.raw_string());
        auto values = std::make_shared<std::vector<Value>>();
        values->reserve(map.map->size());
        for (const auto& pair : *map.map) values->push_back(pair.second);
        return Value(values);
    }
};

struct Contains final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(2);
        const Value& map = args[0];
        if (map.tag != Value::ValueType::Map) throw RuntimeError(frame, "cannot find value in non-map: " + map.raw_string());
        const std::string key = args[1].to_string();
        const auto it = map.map->find(key);
        return Value(it != map.map->end());
    }
};

// string

struct Substring final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(3);
        const std::string haystack = args[0].asString();
        int64_t start; EXPECT_TYPE(start, args[1], asInteger, "integer");
        int64_t end; EXPECT_TYPE(end, args[2], asInteger, "integer");
        if (start > end) throw RuntimeError(frame, "substring start cannot be greater than end");
        if (0 > start || start > haystack.size()) throw RuntimeError(frame, "substring start out of bounds");
        if (0 > end || end > haystack.size()) throw RuntimeError(frame, "substring end out of bounds");
        return Value(haystack.substr(start, end));
    }
};

struct Find final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(2);
        const std::string haystack = args[0].asString();
        const std::string needle = args[1].asString();
        int64_t index = 0;
        if (args.size() > 2) EXPECT_TYPE(index, args[2], asInteger, "integer");
        if (needle.size() + index > haystack.size()) return Value((int64_t) -1);
        if (needle.size() == haystack.size()) return Value((int64_t) (haystack == needle));
        auto found = haystack.find(needle, index);
        if (found == std::string::npos) return Value((int64_t) -1);
        return Value((int64_t) found);
    }
};

struct Upper final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        std::string val = args[0].asString();
        for (auto & c: val) c = toupper((unsigned char) c);
        return Value(val);
    }
};

struct Lower final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        std::string val = args[0].asString();
        for (auto & c: val) c = tolower((unsigned char) c);
        return Value(val);
    }
};

struct Byte final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        std::string val = args[0].asString();
        if (val.empty()) throw RuntimeError(frame, "string cannot be empty");
        int64_t index = 0;
        if (args.size() > 1) EXPECT_TYPE(index, args[1], asInteger, "integer");
        if (index < 0 || index >= val.size()) throw RuntimeError(frame, "index is out of bounds for string");
        return Value((int64_t) (unsigned char) val[index]);
    }
};

struct Char final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> & args) override {
        EXPECT_ARGC(1);
        int64_t chrInt;
        EXPECT_TYPE(chrInt, args[0], asInteger, "integer");
        if (chrInt < 1 || chrInt > 255) throw RuntimeError(frame, "number is out of range for valid string character: " + std::to_string(chrInt));
        return Value(std::string(1, (unsigned char) chrInt));
    }
};
// math

struct Pow final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(2);
        double base; EXPECT_TYPE(base, args[0], asNumber, "number");
        double exponent; EXPECT_TYPE(exponent, args[1], asNumber, "number");
        return Value ( pow(base, exponent) );
    }
};

struct Log final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(2);
        double base; EXPECT_TYPE(base, args[0], asNumber, "number");
        double value; EXPECT_TYPE(value, args[1], asNumber, "number");
        return Value ( log(base) / log(value) );
    }
};

struct Sin final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( sin(value) );
    }
};

struct Cos final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( cos(value) );
    }
};

struct Tan final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( tan(value) );
    }
};

struct Arcsin final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( asin(value) );
    }
};

struct Arccos final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( acos(value) );
    }
};

struct Arctan final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( atan(value) );
    }
};

struct Signum final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( value == 0.0 ? 0.0 : value / fabs(value) );
    }
};

struct Abs final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( fabs(value) );
    }
};

struct Floor final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        double value; EXPECT_TYPE(value, args[0], asNumber, "number");
        return Value ( floor(value) );
    }
};

struct AsInteger final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        int64_t value; EXPECT_TYPE(value, args[0], asInteger, "integer");
        return Value ( value );
    }
};

struct Rand final: AbstractFunction {
    Rand () {
        srand(time(nullptr) * getpid());
    }

    Value call(Stackframe &frame, std::vector<Value> &args) override {
        if (!args.empty()) {
            if (args[0].tag == Value::ValueType::Null)
                srand(time(nullptr) * getpid());
            else {
                int64_t seed;
                EXPECT_TYPE(seed, args[0], asInteger, "integer");
                srand(seed);
            }
        }
        // Construct a double on [0, 1)
        // I didn't want to figure out the C++ random library, so I did it like C.
        uint64_t res = 0;
        // Fill mantissa
        res |= rand() & 0x7f;
        res <<= 15;
        res |= rand() & 0x7fff;
        res <<= 15;
        res |= rand() & 0x7fff;
        res <<= 15;
        res |= rand() & 0x7fff;
        // Add exponent
        res |= 0x3FFull << 52;

        // Reinterpret
        union {
            uint64_t i;
            double f;
        };

        i = res;

        // Subtract 1 from [1, 2) and return
        return Value ( f - 1 );
    }
};

struct Parse final: AbstractFunction {
    Value call(Stackframe &frame, std::vector<Value> &args) override {
        EXPECT_ARGC(1);
        std::stringstream stream;
        stream << args[0].to_string();
        double value;
        stream >> value;
        if (stream.fail())
            throw RuntimeError(frame, "failed to parse value as number: " + args[0].raw_string());
        return Value ( value );
    }
};

std::shared_ptr<runtime::Module> initStdlib() {
    std::shared_ptr<runtime::Module> std = std::make_shared<runtime::Module>(true);
    std->functions["print"] = std::make_shared<Print>();
    std->functions["println"] = std::make_shared<PrintLine>();
    std->functions["input"] = std::make_shared<Input>();
    std->functions["typeof"] = std::make_shared<TypeOf>();
    std->functions["crash"] = std::make_shared<Crash>();
    std->functions["length"] = std::make_shared<Length>();
    auto list = std::make_shared<runtime::Module>(true);
    std->imported["list"] = list;
    list->functions["push"] = std::make_shared<Push>();
    list->functions["pop"] = std::make_shared<Pop>();
    auto map = std::make_shared<runtime::Module>(true);
    std->imported["map"] = map;
    map->functions["remove"] = std::make_shared<Remove>();
    map->functions["keys"] = std::make_shared<Keys>();
    map->functions["values"] = std::make_shared<Values>();
    map->functions["contains"] = std::make_shared<Contains>();
    auto string = std::make_shared<runtime::Module>(true);
    std->imported["string"] = string;
    string->functions["find"] = std::make_shared<Find>();
    string->functions["substring"] = std::make_shared<Substring>();
    string->functions["upper"] = std::make_shared<Upper>();
    string->functions["lower"] = std::make_shared<Lower>();
    string->functions["byte"] = std::make_shared<Byte>();
    string->functions["char"] = std::make_shared<Char>();
    auto math = std::make_shared<runtime::Module>(true);
    std->imported["math"] = math;
    math->globals["pi"] = Value(M_PI);
    math->globals["e"] = Value(M_E);
    math->functions["pow"] = std::make_shared<Pow>();
    math->functions["log"] = std::make_shared<Log>();
    math->functions["sin"] = std::make_shared<Sin>();
    math->functions["cos"] = std::make_shared<Cos>();
    math->functions["tan"] = std::make_shared<Tan>();
    math->functions["asin"] = std::make_shared<Arcsin>();
    math->functions["acos"] = std::make_shared<Arccos>();
    math->functions["atan"] = std::make_shared<Arctan>();
    math->functions["signum"] = std::make_shared<Signum>();
    math->functions["abs"] = std::make_shared<Abs>();
    math->functions["floor"] = std::make_shared<Floor>();
    math->functions["as_int"] = std::make_shared<AsInteger>();
    math->functions["rand"] = std::make_shared<Rand>();
    math->functions["parse"] = std::make_shared<Parse>();
    return std;
}

std::shared_ptr<runtime::Module> stdlib = initStdlib();

runtime::Module::Module(bool isStdLib) {
    if (isStdLib) return;
    imported["std"] = stdlib;
}