// Minimal JSON value, parser and writer for the engine core (no JUCE dependency).
// Objects keep insertion order, so a file round-trips with its keys in place.
#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kindred {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Member = std::pair<std::string, Json>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : type_(Type::Bool), b_(b) {}
    Json(double d) : type_(Type::Number), n_(d) {}
    Json(int i) : type_(Type::Number), n_(i) {}
    Json(const char* s) : type_(Type::String), s_(s) {}
    Json(std::string s) : type_(Type::String), s_(std::move(s)) {}
    static Json array() { Json j; j.type_ = Type::Array; return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    // Lenient readers: a missing or mistyped value gives the fallback.
    bool boolean(bool fallback = false) const;      // JS truthiness
    double number(double fallback = 0) const;
    int integer(int fallback = 0) const;
    const std::string& string() const { return s_; }
    std::string stringOr(const std::string& fallback) const { return isString() ? s_ : fallback; }

    const std::vector<Json>& items() const { return a_; }
    std::vector<Json>& items() { return a_; }
    const std::vector<Member>& members() const { return o_; }
    std::vector<Member>& members() { return o_; }
    size_t size() const { return isArray() ? a_.size() : isObject() ? o_.size() : 0; }

    bool has(const std::string& key) const { return find(key) != nullptr; }
    const Json* find(const std::string& key) const;
    const Json& operator[](const std::string& key) const;     // null if missing
    const Json& operator[](size_t i) const;
    Json& set(const std::string& key, Json v);                 // insert or replace, keeps order
    void erase(const std::string& key);
    void push(Json v) { type_ = Type::Array; a_.push_back(std::move(v)); }

    bool operator==(const Json& o) const;
    bool operator!=(const Json& o) const { return !(*this == o); }

    // indent < 0: compact. Otherwise pretty-printed with that many spaces.
    std::string dump(int indent = -1) const;
    // Throws std::runtime_error with a position on malformed input.
    static Json parse(const std::string& text);
    static std::string numberToString(double d);

private:
    void write(std::string& out, int indent, int depth) const;
    Type type_ = Type::Null;
    bool b_ = false;
    double n_ = 0;
    std::string s_;
    std::vector<Json> a_;
    std::vector<Member> o_;
};

}  // namespace kindred
