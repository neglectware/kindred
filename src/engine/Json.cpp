#include "Json.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace kindred {

static const Json kNull;

bool Json::boolean(bool fallback) const {
    switch (type_) {
        case Type::Bool: return b_;
        case Type::Number: return n_ != 0 && !std::isnan(n_);
        case Type::String: return !s_.empty();
        case Type::Array: case Type::Object: return true;
        case Type::Null: return fallback;
    }
    return fallback;
}
double Json::number(double fallback) const {
    if (type_ == Type::Number) return n_;
    if (type_ == Type::Bool) return b_ ? 1 : 0;
    if (type_ == Type::String) { char* e = nullptr; double v = std::strtod(s_.c_str(), &e); return e && *e == 0 && !s_.empty() ? v : fallback; }
    return fallback;
}
int Json::integer(int fallback) const {
    double v = number(std::nan(""));
    if (std::isnan(v) || std::isinf(v)) return fallback;
    return (int) v;      // truncates, like JS `| 0` for values in range
}
const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (auto& m : o_) if (m.first == key) return &m.second;
    return nullptr;
}
const Json& Json::operator[](const std::string& key) const { auto* p = find(key); return p ? *p : kNull; }
const Json& Json::operator[](size_t i) const { return type_ == Type::Array && i < a_.size() ? a_[i] : kNull; }
Json& Json::set(const std::string& key, Json v) {
    type_ = Type::Object;
    for (auto& m : o_) if (m.first == key) { m.second = std::move(v); return m.second; }
    o_.emplace_back(key, std::move(v));
    return o_.back().second;
}
void Json::erase(const std::string& key) {
    for (size_t i = 0; i < o_.size(); i++) if (o_[i].first == key) { o_.erase(o_.begin() + (long) i); return; }
}
bool Json::operator==(const Json& o) const {
    if (type_ != o.type_) return false;
    switch (type_) {
        case Type::Null: return true;
        case Type::Bool: return b_ == o.b_;
        case Type::Number: return n_ == o.n_;
        case Type::String: return s_ == o.s_;
        case Type::Array: return a_ == o.a_;
        case Type::Object: return o_ == o.o_;
    }
    return false;
}

std::string Json::numberToString(double d) {
    if (std::isnan(d) || std::isinf(d)) return "null";
    if (d == std::floor(d) && std::fabs(d) < 1e15) { char b[32]; std::snprintf(b, sizeof b, "%.0f", d); return b; }
    char b[40];
    for (int p = 1; p <= 17; p++) {          // shortest text that reads back as the same double
        std::snprintf(b, sizeof b, "%.*g", p, d);
        if (std::strtod(b, nullptr) == d) break;
    }
    return b;
}
static void writeString(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += (char) c;
        }
    }
    out += '"';
}
void Json::write(std::string& out, int indent, int depth) const {
    auto nl = [&](int d) { if (indent >= 0) { out += '\n'; out.append((size_t) (indent * d), ' '); } };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += b_ ? "true" : "false"; break;
        case Type::Number: out += numberToString(n_); break;
        case Type::String: writeString(out, s_); break;
        case Type::Array:
            if (a_.empty()) { out += "[]"; break; }
            out += '[';
            for (size_t i = 0; i < a_.size(); i++) { if (i) out += ','; nl(depth + 1); a_[i].write(out, indent, depth + 1); }
            nl(depth); out += ']';
            break;
        case Type::Object:
            if (o_.empty()) { out += "{}"; break; }
            out += '{';
            for (size_t i = 0; i < o_.size(); i++) {
                if (i) out += ',';
                nl(depth + 1); writeString(out, o_[i].first); out += indent >= 0 ? ": " : ":";
                o_[i].second.write(out, indent, depth + 1);
            }
            nl(depth); out += '}';
            break;
    }
}
std::string Json::dump(int indent) const { std::string s; write(s, indent, 0); return s; }

namespace {
struct Parser {
    const std::string& t; size_t i = 0;
    [[noreturn]] void fail(const char* what) { throw std::runtime_error(std::string("JSON: ") + what + " at " + std::to_string(i)); }
    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++; }
    static void utf8(std::string& s, unsigned cp) {
        if (cp < 0x80) s += (char) cp;
        else if (cp < 0x800) { s += (char) (0xC0 | (cp >> 6)); s += (char) (0x80 | (cp & 63)); }
        else if (cp < 0x10000) { s += (char) (0xE0 | (cp >> 12)); s += (char) (0x80 | ((cp >> 6) & 63)); s += (char) (0x80 | (cp & 63)); }
        else { s += (char) (0xF0 | (cp >> 18)); s += (char) (0x80 | ((cp >> 12) & 63)); s += (char) (0x80 | ((cp >> 6) & 63)); s += (char) (0x80 | (cp & 63)); }
    }
    unsigned hex4() {
        if (i + 4 > t.size()) fail("bad escape");
        unsigned v = 0;
        for (int k = 0; k < 4; k++) { char c = t[i++]; v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned) (c - '0'); else if (c >= 'a' && c <= 'f') v |= (unsigned) (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned) (c - 'A' + 10); else fail("bad escape"); }
        return v;
    }
    std::string str() {
        std::string s; i++;
        while (true) {
            if (i >= t.size()) fail("unterminated string");
            char c = t[i++];
            if (c == '"') return s;
            if (c != '\\') { s += c; continue; }
            if (i >= t.size()) fail("bad escape");
            char e = t[i++];
            switch (e) {
                case 'n': s += '\n'; break; case 'r': s += '\r'; break; case 't': s += '\t'; break;
                case 'b': s += '\b'; break; case 'f': s += '\f'; break;
                case 'u': { unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < t.size() && t[i] == '\\' && t[i + 1] == 'u') { i += 2; unsigned lo = hex4(); cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); }
                    utf8(s, cp); break; }
                default: s += e;
            }
        }
    }
    Json value(int depth) {
        if (depth > 200) fail("too deep");
        ws();
        if (i >= t.size()) fail("unexpected end");
        char c = t[i];
        if (c == '{') {
            Json o = Json::object(); i++; ws();
            if (i < t.size() && t[i] == '}') { i++; return o; }
            while (true) {
                ws(); if (i >= t.size() || t[i] != '"') fail("expected key");
                std::string k = str(); ws();
                if (i >= t.size() || t[i] != ':') fail("expected ':'");
                i++; o.members().emplace_back(std::move(k), value(depth + 1)); ws();
                if (i < t.size() && t[i] == ',') { i++; continue; }
                if (i < t.size() && t[i] == '}') { i++; return o; }
                fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            Json a = Json::array(); i++; ws();
            if (i < t.size() && t[i] == ']') { i++; return a; }
            while (true) {
                a.items().push_back(value(depth + 1)); ws();
                if (i < t.size() && t[i] == ',') { i++; continue; }
                if (i < t.size() && t[i] == ']') { i++; return a; }
                fail("expected ',' or ']'");
            }
        }
        if (c == '"') return Json(str());
        if (t.compare(i, 4, "true") == 0) { i += 4; return Json(true); }
        if (t.compare(i, 5, "false") == 0) { i += 5; return Json(false); }
        if (t.compare(i, 4, "null") == 0) { i += 4; return Json(); }
        const char* b = t.c_str() + i; char* e = nullptr;
        double v = std::strtod(b, &e);
        if (e == b) fail("unexpected character");
        i += (size_t) (e - b);
        return Json(v);
    }
};
}  // namespace

Json Json::parse(const std::string& text) {
    Parser p{text};
    Json v = p.value(0);
    p.ws();
    if (p.i != text.size()) p.fail("trailing characters");
    return v;
}

}  // namespace kindred
