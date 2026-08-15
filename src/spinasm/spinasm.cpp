#include <fv1/spinasm.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace fv1::spinasm {
namespace {

constexpr std::uint32_t PROG_INSTRUCTIONS = 128;
constexpr std::uint32_t DELAY_SAMPLES = 32767;
constexpr std::uint32_t M2 = 0x03;
constexpr std::uint32_t M5 = 0x1F;
constexpr std::uint32_t M6 = 0x3F;
constexpr std::uint32_t M9 = 0x1FF;
constexpr std::uint32_t M11 = 0x7FF;
constexpr std::uint32_t M15 = 0x7FFF;
constexpr std::uint32_t M16 = 0xFFFF;
constexpr std::uint32_t M24 = 0xFFFFFF;

using Value = std::variant<std::int64_t, double>;
using SymbolTable = std::map<std::string, Value>;

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

bool is_int(const Value& value) noexcept { return std::holds_alternative<std::int64_t>(value); }
std::int64_t as_int(const Value& value) {
    if (const auto* v = std::get_if<std::int64_t>(&value)) return *v;
    return static_cast<std::int64_t>(std::get<double>(value));
}
double as_double(const Value& value) {
    if (const auto* v = std::get_if<double>(&value)) return *v;
    return static_cast<double>(std::get<std::int64_t>(value));
}

bool numerically_integral(const Value& value) {
    if (is_int(value)) return true;
    const double v = as_double(value);
    return std::isfinite(v) && std::trunc(v) == v;
}

std::int64_t floor_div(std::int64_t lhs, std::int64_t rhs) {
    if (rhs == 0) throw std::runtime_error("division by zero");
    std::int64_t q = lhs / rhs;
    const std::int64_t r = lhs % rhs;
    if (r != 0 && ((r > 0) != (rhs > 0))) --q;
    return q;
}

std::int64_t round_even(double value) {
    if (!std::isfinite(value)) throw std::runtime_error("non-finite numeric value");
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) return static_cast<std::int64_t>(lower);
    if (fraction > 0.5) return static_cast<std::int64_t>(lower + 1.0);
    const auto lower_i = static_cast<std::int64_t>(lower);
    return (lower_i & 1LL) == 0 ? lower_i : lower_i + 1LL;
}

bool checked_mul(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if (a == 0 || b == 0) { out = 0; return true; }
    if (a == -1) {
        if (b == std::numeric_limits<std::int64_t>::min()) return false;
        out = -b; return true;
    }
    if (b == -1) {
        if (a == std::numeric_limits<std::int64_t>::min()) return false;
        out = -a; return true;
    }
    const auto max = std::numeric_limits<std::int64_t>::max();
    const auto min = std::numeric_limits<std::int64_t>::min();
    if (a > 0) {
        if (b > 0) { if (a > max / b) return false; }
        else       { if (b < min / a) return false; }
    } else {
        if (b > 0) { if (a < min / b) return false; }
        else       { if (a < max / b) return false; }
    }
    out = a * b;
    return true;
}

Value pow_value(const Value& lhs, const Value& rhs) {
    if (is_int(lhs) && is_int(rhs) && as_int(rhs) >= 0 && as_int(rhs) <= 62) {
        std::int64_t base = as_int(lhs);
        std::int64_t exponent = as_int(rhs);
        std::int64_t result = 1;
        bool overflow = false;
        while (exponent > 0) {
            if ((exponent & 1LL) != 0) {
                std::int64_t product = 0;
                if (!checked_mul(result, base, product)) { overflow = true; break; }
                result = product;
            }
            exponent >>= 1LL;
            if (exponent > 0) {
                std::int64_t product = 0;
                if (!checked_mul(base, base, product)) { overflow = true; break; }
                base = product;
            }
        }
        if (!overflow) return result;
    }
    return std::pow(as_double(lhs), as_double(rhs));
}

std::string normalize_expression(std::string expression) {
    expression = trim(std::move(expression));
    std::string replaced;
    replaced.reserve(expression.size() + 8);
    for (std::size_t i = 0; i < expression.size(); ++i) {
        if (expression[i] == '$') {
            replaced += "0x";
        } else if (expression[i] == '%' && i + 1 < expression.size() && (expression[i + 1] == '0' || expression[i + 1] == '1')) {
            replaced += "0b";
        } else {
            replaced += expression[i];
        }
    }
    static const std::regex mid_re(R"(\b([A-Za-z_][A-Za-z0-9_]*)\^)");
    static const std::regex end_re(R"(\b([A-Za-z_][A-Za-z0-9_]*)#)");
    replaced = std::regex_replace(replaced, mid_re, "$1__MID");
    replaced = std::regex_replace(replaced, end_re, "$1__END");
    return replaced;
}

enum class TokenKind {
    End, Number, Identifier,
    Plus, Minus, Tilde, Star, Slash, FloorDiv, Power,
    Pipe, Amp, Caret, LShift, RShift, LParen, RParen
};

struct Token {
    TokenKind kind{TokenKind::End};
    std::string text;
    Value number{std::int64_t{0}};
};

class Lexer {
public:
    explicit Lexer(std::string text) : text_(std::move(text)) {}

    Token next() {
        skip_space();
        if (pos_ >= text_.size()) return {TokenKind::End, {}, std::int64_t{0}};
        const char c = text_[pos_];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return number();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return identifier();
        if (c == '<' && peek(1) == '<') { pos_ += 2; return {TokenKind::LShift, "<<", std::int64_t{0}}; }
        if (c == '>' && peek(1) == '>') { pos_ += 2; return {TokenKind::RShift, ">>", std::int64_t{0}}; }
        if (c == '*' && peek(1) == '*') { pos_ += 2; return {TokenKind::Power, "**", std::int64_t{0}}; }
        if (c == '/' && peek(1) == '/') { pos_ += 2; return {TokenKind::FloorDiv, "//", std::int64_t{0}}; }
        ++pos_;
        switch (c) {
            case '+': return {TokenKind::Plus, "+", std::int64_t{0}};
            case '-': return {TokenKind::Minus, "-", std::int64_t{0}};
            case '~': return {TokenKind::Tilde, "~", std::int64_t{0}};
            case '*': return {TokenKind::Star, "*", std::int64_t{0}};
            case '/': return {TokenKind::Slash, "/", std::int64_t{0}};
            case '|': return {TokenKind::Pipe, "|", std::int64_t{0}};
            case '&': return {TokenKind::Amp, "&", std::int64_t{0}};
            case '^': return {TokenKind::Caret, "^", std::int64_t{0}};
            case '(': return {TokenKind::LParen, "(", std::int64_t{0}};
            case ')': return {TokenKind::RParen, ")", std::int64_t{0}};
            default: throw std::runtime_error(std::string("unexpected character '") + c + "'");
        }
    }

private:
    char peek(std::size_t offset) const noexcept {
        return pos_ + offset < text_.size() ? text_[pos_ + offset] : '\0';
    }

    void skip_space() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) ++pos_;
    }

    Token identifier() {
        const std::size_t start = pos_++;
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_]);
            if (std::isalnum(c) == 0 && c != '_') break;
            ++pos_;
        }
        return {TokenKind::Identifier, text_.substr(start, pos_ - start), std::int64_t{0}};
    }

    Token number() {
        const std::size_t start = pos_;
        if (text_[pos_] == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            pos_ += 2;
            while (pos_ < text_.size() && (std::isxdigit(static_cast<unsigned char>(text_[pos_])) != 0 || text_[pos_] == '_')) ++pos_;
            std::string raw = text_.substr(start + 2, pos_ - start - 2);
            raw.erase(std::remove(raw.begin(), raw.end(), '_'), raw.end());
            if (raw.empty()) throw std::runtime_error("invalid hexadecimal literal");
            return {TokenKind::Number, text_.substr(start, pos_ - start), static_cast<std::int64_t>(std::stoll(raw, nullptr, 16))};
        }
        if (text_[pos_] == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
            pos_ += 2;
            while (pos_ < text_.size() && (text_[pos_] == '0' || text_[pos_] == '1' || text_[pos_] == '_')) ++pos_;
            std::string raw = text_.substr(start + 2, pos_ - start - 2);
            raw.erase(std::remove(raw.begin(), raw.end(), '_'), raw.end());
            if (raw.empty()) throw std::runtime_error("invalid binary literal");
            return {TokenKind::Number, text_.substr(start, pos_ - start), static_cast<std::int64_t>(std::stoll(raw, nullptr, 2))};
        }

        bool saw_dot = false;
        bool saw_exp = false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (std::isdigit(static_cast<unsigned char>(c)) != 0 || c == '_') { ++pos_; continue; }
            if (c == '.' && !saw_dot && !saw_exp) { saw_dot = true; ++pos_; continue; }
            if ((c == 'e' || c == 'E') && !saw_exp) {
                saw_exp = true;
                ++pos_;
                if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
                continue;
            }
            break;
        }
        std::string raw = text_.substr(start, pos_ - start);
        raw.erase(std::remove(raw.begin(), raw.end(), '_'), raw.end());
        if (raw == ".") throw std::runtime_error("invalid numeric literal");
        if (saw_dot || saw_exp) return {TokenKind::Number, raw, std::stod(raw)};
        return {TokenKind::Number, raw, static_cast<std::int64_t>(std::stoll(raw, nullptr, 10))};
    }

    std::string text_;
    std::size_t pos_{};
};

class ExpressionParser {
public:
    ExpressionParser(std::string expression, const SymbolTable& symbols)
        : lexer_(normalize_expression(std::move(expression))), symbols_(symbols), current_(lexer_.next()) {}

    Value parse() {
        Value value = parse_or();
        if (current_.kind != TokenKind::End) throw std::runtime_error("unexpected token " + current_.text);
        return value;
    }

private:
    void advance() { current_ = lexer_.next(); }
    bool accept(TokenKind kind) {
        if (current_.kind != kind) return false;
        advance();
        return true;
    }
    void expect(TokenKind kind, const char* what) {
        if (!accept(kind)) throw std::runtime_error(std::string("expected ") + what);
    }

    Value parse_or() {
        Value value = parse_xor();
        while (accept(TokenKind::Pipe)) value = as_int(value) | as_int(parse_xor());
        return value;
    }
    Value parse_xor() {
        Value value = parse_and();
        while (accept(TokenKind::Caret)) value = as_int(value) ^ as_int(parse_and());
        return value;
    }
    Value parse_and() {
        Value value = parse_shift();
        while (accept(TokenKind::Amp)) value = as_int(value) & as_int(parse_shift());
        return value;
    }
    Value parse_shift() {
        Value value = parse_add();
        while (current_.kind == TokenKind::LShift || current_.kind == TokenKind::RShift) {
            const TokenKind op = current_.kind;
            advance();
            const std::int64_t amount = as_int(parse_add());
            if (amount < 0 || amount >= 63) throw std::runtime_error("invalid shift count");
            value = op == TokenKind::LShift ? (as_int(value) << amount) : (as_int(value) >> amount);
        }
        return value;
    }
    Value parse_add() {
        Value value = parse_mul();
        while (current_.kind == TokenKind::Plus || current_.kind == TokenKind::Minus) {
            const TokenKind op = current_.kind;
            advance();
            Value rhs = parse_mul();
            if (is_int(value) && is_int(rhs)) {
                value = op == TokenKind::Plus ? as_int(value) + as_int(rhs) : as_int(value) - as_int(rhs);
            } else {
                value = op == TokenKind::Plus ? as_double(value) + as_double(rhs) : as_double(value) - as_double(rhs);
            }
        }
        return value;
    }
    Value parse_mul() {
        Value value = parse_unary();
        while (current_.kind == TokenKind::Star || current_.kind == TokenKind::Slash || current_.kind == TokenKind::FloorDiv) {
            const TokenKind op = current_.kind;
            advance();
            Value rhs = parse_unary();
            if (op == TokenKind::Star) {
                value = (is_int(value) && is_int(rhs)) ? Value{as_int(value) * as_int(rhs)} : Value{as_double(value) * as_double(rhs)};
            } else if (op == TokenKind::Slash) {
                if (as_double(rhs) == 0.0) throw std::runtime_error("division by zero");
                value = as_double(value) / as_double(rhs);
            } else {
                if (is_int(value) && is_int(rhs)) value = floor_div(as_int(value), as_int(rhs));
                else {
                    if (as_double(rhs) == 0.0) throw std::runtime_error("division by zero");
                    value = std::floor(as_double(value) / as_double(rhs));
                }
            }
        }
        return value;
    }
    Value parse_unary() {
        if (accept(TokenKind::Plus)) return parse_unary();
        if (accept(TokenKind::Minus)) {
            Value v = parse_unary();
            return is_int(v) ? Value{-as_int(v)} : Value{-as_double(v)};
        }
        if (accept(TokenKind::Tilde)) return ~as_int(parse_unary());
        return parse_power();
    }
    Value parse_power() {
        Value value = parse_primary();
        if (accept(TokenKind::Power)) value = pow_value(value, parse_unary());
        return value;
    }
    Value parse_primary() {
        if (current_.kind == TokenKind::Number) {
            Value value = current_.number;
            advance();
            return value;
        }
        if (current_.kind == TokenKind::Identifier) {
            const std::string name = current_.text;
            advance();
            if (upper(name) == "INT" && accept(TokenKind::LParen)) {
                Value value = parse_or();
                expect(TokenKind::RParen, "')'");
                return round_even(as_double(value));
            }
            const auto it = symbols_.find(upper(name));
            if (it == symbols_.end()) throw std::runtime_error("undefined symbol: " + name);
            return it->second;
        }
        if (accept(TokenKind::LParen)) {
            Value value = parse_or();
            expect(TokenKind::RParen, "')'");
            return value;
        }
        throw std::runtime_error("expected expression");
    }

    Lexer lexer_;
    const SymbolTable& symbols_;
    Token current_;
};

Value eval_expr(const std::string& expression, const SymbolTable& symbols, std::uint32_t line) {
    try {
        return ExpressionParser(expression, symbols).parse();
    } catch (const CompileError&) {
        throw;
    } catch (const std::exception& e) {
        throw CompileError(line, std::string("Line ") + std::to_string(line) + ": invalid expression '" + expression + "': " + e.what());
    }
}

std::uint32_t fixed(const Value& value, std::int64_t ref, double minimum, double maximum,
                    std::uint32_t mask, const char* name, std::uint32_t line) {
    if (is_int(value)) {
        const std::int64_t integer = as_int(value);
        if (integer < 0) return static_cast<std::uint32_t>(integer) & mask;
        if (static_cast<std::uint64_t>(integer) > mask) {
            throw CompileError(line, std::string("Line ") + std::to_string(line) + ": " + name + " integer operand " + std::to_string(integer) + " exceeds 0x" + [&] {
                std::ostringstream os; os << std::hex << std::uppercase << mask; return os.str();
            }());
        }
        return static_cast<std::uint32_t>(integer) & mask;
    }
    const double f = as_double(value);
    if (!std::isfinite(f) || f < minimum || f > maximum) {
        std::ostringstream os;
        os << "Line " << line << ": " << name << " real operand " << f << " outside " << minimum << ".." << maximum;
        throw CompileError(line, os.str());
    }
    return static_cast<std::uint32_t>(round_even(f * static_cast<double>(ref))) & mask;
}

std::uint32_t s1_9(const Value& v, std::uint32_t line) { return fixed(v, 512, -2.0, 1.998046875, M11, "S1.9", line); }
std::uint32_t s1_14(const Value& v, std::uint32_t line) { return fixed(v, 16384, -2.0, 1.99993896484375, M16, "S1.14", line); }
std::uint32_t s_10(const Value& v, std::uint32_t line) { return fixed(v, 1024, -1.0, 0.9990234375, M11, "S.10", line); }
std::uint32_t s_15(const Value& v, std::uint32_t line) { return fixed(v, 32768, -1.0, 0.999969482421875, M16, "S.15", line); }
std::uint32_t s_23(const Value& v, std::uint32_t line) { return fixed(v, 8388608, -1.0, 0.9999998807907104, M24, "S.23", line); }

struct ParsedInstruction {
    std::string mnemonic;
    std::vector<std::string> args;
    std::uint32_t line{};
    std::uint32_t address{};
};

struct ParseResult {
    std::vector<ParsedInstruction> instructions;
    SymbolTable symbols;
    std::map<std::string, std::uint32_t> labels;
    std::uint32_t highest_delay{};
};

SymbolTable default_symbols() {
    SymbolTable symbols{
        {"SIN0_RATE", std::int64_t{0x00}}, {"SIN0_RANGE", std::int64_t{0x01}},
        {"SIN1_RATE", std::int64_t{0x02}}, {"SIN1_RANGE", std::int64_t{0x03}},
        {"RMP0_RATE", std::int64_t{0x04}}, {"RMP0_RANGE", std::int64_t{0x05}},
        {"RMP1_RATE", std::int64_t{0x06}}, {"RMP1_RANGE", std::int64_t{0x07}},
        {"POT0", std::int64_t{0x10}}, {"POT1", std::int64_t{0x11}}, {"POT2", std::int64_t{0x12}},
        {"ADCL", std::int64_t{0x14}}, {"ADCR", std::int64_t{0x15}}, {"DACL", std::int64_t{0x16}}, {"DACR", std::int64_t{0x17}},
        {"ADDR_PTR", std::int64_t{0x18}},
        {"SIN0", std::int64_t{0}}, {"SIN1", std::int64_t{1}}, {"RMP0", std::int64_t{2}}, {"RMP1", std::int64_t{3}},
        {"RDA", std::int64_t{0}}, {"SOF", std::int64_t{2}}, {"RDAL", std::int64_t{3}},
        {"SIN", std::int64_t{0}}, {"COS", std::int64_t{1}}, {"REG", std::int64_t{2}}, {"COMPC", std::int64_t{4}},
        {"COMPA", std::int64_t{8}}, {"RPTR2", std::int64_t{0x10}}, {"NA", std::int64_t{0x20}},
        {"RUN", std::int64_t{0x10}}, {"ZRC", std::int64_t{0x08}}, {"ZRO", std::int64_t{0x04}}, {"GEZ", std::int64_t{0x02}}, {"NEG", std::int64_t{0x01}},
    };
    for (std::int64_t i = 0; i < 32; ++i) symbols["REG" + std::to_string(i)] = 0x20 + i;
    return symbols;
}

std::vector<std::string> split_args(const std::string& text) {
    std::vector<std::string> args;
    if (trim(text).empty()) return args;
    std::size_t start = 0;
    int depth = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size()) {
            if (text[i] == '(') ++depth;
            else if (text[i] == ')') --depth;
        }
        if (i == text.size() || (text[i] == ',' && depth == 0)) {
            args.push_back(trim(text.substr(start, i - start)));
            start = i + 1;
        }
    }
    return args;
}

std::vector<std::string> split_ws3(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t pos = 0;
    while (parts.size() < 2) {
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) ++pos;
        if (pos >= line.size()) return parts;
        const std::size_t start = pos;
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) == 0) ++pos;
        parts.push_back(line.substr(start, pos - start));
    }
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) ++pos;
    if (pos < line.size()) parts.push_back(line.substr(pos));
    return parts;
}

ParseResult parse_source(std::string_view source) {
    ParseResult out;
    out.symbols = default_symbols();
    std::uint32_t delay_cursor = 0;
    std::istringstream stream{std::string(source)};
    std::string raw;
    std::uint32_t line_no = 0;
    static const std::regex label_re(R"(^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$)");

    while (std::getline(stream, raw)) {
        ++line_no;
        const auto semicolon = raw.find(';');
        std::string line = trim(raw.substr(0, semicolon));
        if (line.empty()) continue;

        while (true) {
            std::smatch match;
            if (!std::regex_match(line, match, label_re)) break;
            const std::string label = upper(match[1].str());
            if (out.labels.contains(label)) throw CompileError(line_no, "Line " + std::to_string(line_no) + ": duplicate target " + label);
            out.labels[label] = static_cast<std::uint32_t>(out.instructions.size());
            line = trim(match[2].str());
            if (line.empty()) break;
        }
        if (line.empty()) continue;

        const auto parts = split_ws3(line);
        const std::string p0 = parts.empty() ? std::string{} : upper(parts[0]);
        const std::string p1 = parts.size() > 1 ? upper(parts[1]) : std::string{};

        if (p0 == "MEM" || p1 == "MEM") {
            std::string name;
            std::string expression;
            if (p0 == "MEM") {
                if (parts.size() < 3) throw CompileError(line_no, "Line " + std::to_string(line_no) + ": MEM requires name and length");
                name = parts[1]; expression = parts[2];
            } else {
                if (parts.size() < 3) throw CompileError(line_no, "Line " + std::to_string(line_no) + ": MEM requires length");
                name = parts[0]; expression = parts[2];
            }
            const Value length_value = eval_expr(expression, out.symbols, line_no);
            if (!numerically_integral(length_value) || as_double(length_value) < 0.0) {
                throw CompileError(line_no, "Line " + std::to_string(line_no) + ": invalid MEM length");
            }
            const auto length = static_cast<std::uint32_t>(as_double(length_value));
            if (static_cast<std::uint64_t>(delay_cursor) + length > DELAY_SAMPLES) {
                throw CompileError(line_no, "Line " + std::to_string(line_no) + ": delay RAM exhausted");
            }
            const std::string key = upper(name);
            out.symbols[key] = static_cast<std::int64_t>(delay_cursor);
            out.symbols[key + "__MID"] = static_cast<std::int64_t>(delay_cursor + length / 2u);
            out.symbols[key + "__END"] = static_cast<std::int64_t>(delay_cursor + length);
            out.highest_delay = std::max(out.highest_delay, delay_cursor + length);
            delay_cursor += length + 1u;
            continue;
        }

        if (p0 == "EQU" || p1 == "EQU") {
            std::string name;
            std::string expression;
            if (p0 == "EQU") {
                if (parts.size() < 3) throw CompileError(line_no, "Line " + std::to_string(line_no) + ": EQU requires name and expression");
                name = parts[1]; expression = parts[2];
            } else {
                if (parts.size() < 3) throw CompileError(line_no, "Line " + std::to_string(line_no) + ": EQU requires expression");
                name = parts[0]; expression = parts[2];
            }
            out.symbols[upper(name)] = eval_expr(expression, out.symbols, line_no);
            continue;
        }

        const auto split = line.find_first_of(" \t");
        const std::string mnemonic = upper(split == std::string::npos ? line : line.substr(0, split));
        const std::string arg_text = split == std::string::npos ? std::string{} : trim(line.substr(split + 1));
        out.instructions.push_back({mnemonic, split_args(arg_text), line_no, static_cast<std::uint32_t>(out.instructions.size())});
    }

    if (out.instructions.size() > PROG_INSTRUCTIONS) {
        throw CompileError(0, "Program has " + std::to_string(out.instructions.size()) + " instructions; FV-1 limit is 128");
    }
    return out;
}

std::uint32_t encode_instruction(const ParsedInstruction& ins, const SymbolTable& symbols,
                                 const std::map<std::string, std::uint32_t>& labels) {
    const auto& a = ins.args;
    const auto need = [&](std::size_t count) {
        if (a.size() != count) {
            throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": " + ins.mnemonic + " expects " +
                               std::to_string(count) + " operands, got " + std::to_string(a.size()));
        }
    };
    const auto ex = [&](std::size_t i) { return eval_expr(a.at(i), symbols, ins.line); };
    const auto reg = [&](std::size_t i) -> std::uint32_t {
        const Value value = ex(i);
        if (!numerically_integral(value) || as_double(value) < 0.0 || as_double(value) > 63.0) {
            throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid register " + a[i]);
        }
        return static_cast<std::uint32_t>(as_double(value));
    };
    const auto addr15 = [&](std::size_t i) -> std::uint32_t {
        const Value value = ex(i);
        if (is_int(value)) {
            const auto integer = as_int(value);
            if (integer < -0x8000LL || integer > 0x7fffLL) {
                throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": address out of range: " + std::to_string(integer));
            }
            return static_cast<std::uint32_t>(integer) & M15;
        }
        return s_15(value, ins.line) & M15;
    };

    const std::string& m = ins.mnemonic;
    if (m == "RDA")  { need(2); return (s1_9(ex(1), ins.line) << 21) | (addr15(0) << 5) | 0x00u; }
    if (m == "RMPA") { need(1); return (s1_9(ex(0), ins.line) << 21) | 0x01u; }
    if (m == "WRA")  { need(2); return (s1_9(ex(1), ins.line) << 21) | (addr15(0) << 5) | 0x02u; }
    if (m == "WRAP") { need(2); return (s1_9(ex(1), ins.line) << 21) | (addr15(0) << 5) | 0x03u; }
    if (m == "RDAX") { need(2); return (s1_14(ex(1), ins.line) << 16) | (reg(0) << 5) | 0x04u; }
    if (m == "RDFX") { need(2); return (s1_14(ex(1), ins.line) << 16) | (reg(0) << 5) | 0x05u; }
    if (m == "LDAX") { need(1); return (reg(0) << 5) | 0x05u; }
    if (m == "WRAX") { need(2); return (s1_14(ex(1), ins.line) << 16) | (reg(0) << 5) | 0x06u; }
    if (m == "WRHX") { need(2); return (s1_14(ex(1), ins.line) << 16) | (reg(0) << 5) | 0x07u; }
    if (m == "WRLX") { need(2); return (s1_14(ex(1), ins.line) << 16) | (reg(0) << 5) | 0x08u; }
    if (m == "MAXX") { need(2); return (s1_14(ex(1), ins.line) << 16) | (reg(0) << 5) | 0x09u; }
    if (m == "ABSA") { need(0); return 0x09u; }
    if (m == "MULX") { need(1); return (reg(0) << 5) | 0x0Au; }
    if (m == "LOG")  { need(2); return (s1_14(ex(0), ins.line) << 16) | (s_10(ex(1), ins.line) << 5) | 0x0Bu; }
    if (m == "EXP")  { need(2); return (s1_14(ex(0), ins.line) << 16) | (s_10(ex(1), ins.line) << 5) | 0x0Cu; }
    if (m == "SOF")  { need(2); return (s1_14(ex(0), ins.line) << 16) | (s_10(ex(1), ins.line) << 5) | 0x0Du; }
    if (m == "AND")  { need(1); return (s_23(ex(0), ins.line) << 8) | 0x0Eu; }
    if (m == "CLR")  { need(0); return 0x0Eu; }
    if (m == "OR")   { need(1); return (s_23(ex(0), ins.line) << 8) | 0x0Fu; }
    if (m == "XOR")  { need(1); return (s_23(ex(0), ins.line) << 8) | 0x10u; }
    if (m == "NOT")  { need(0); return (M24 << 8) | 0x10u; }
    if (m == "SKP" || m == "JMP") {
        std::uint32_t cond = 0;
        std::string target_expr;
        if (m == "SKP") { need(2); cond = static_cast<std::uint32_t>(as_int(ex(0))) & M5; target_expr = a[1]; }
        else { need(1); target_expr = a[0]; }
        const std::string key = upper(trim(target_expr));
        std::int64_t offset = 0;
        static const std::regex label_name(R"(^[A-Z_][A-Z0-9_]*$)");
        const auto label_it = labels.find(key);
        if (std::regex_match(key, label_name) && label_it != labels.end()) {
            offset = static_cast<std::int64_t>(label_it->second) - static_cast<std::int64_t>(ins.address) - 1;
        } else {
            offset = as_int(eval_expr(target_expr, symbols, ins.line));
        }
        if (offset < 0 || offset > static_cast<std::int64_t>(M6)) {
            throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": skip offset " + std::to_string(offset) + " out of range");
        }
        return (cond << 27) | (static_cast<std::uint32_t>(offset) << 21) | 0x11u;
    }
    if (m == "NOP") { need(0); return 0x11u; }
    if (m == "WLDS") {
        need(3);
        const auto lfo = as_int(ex(0));
        const auto freq = as_int(ex(1));
        if (lfo < 0 || lfo > 1 || freq < 0 || freq > static_cast<std::int64_t>(M9)) {
            throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid WLDS operands");
        }
        const std::uint32_t amp = s_15(ex(2), ins.line) & M15;
        return (static_cast<std::uint32_t>(lfo) << 29) | (static_cast<std::uint32_t>(freq) << 20) | (amp << 5) | 0x12u;
    }
    if (m == "WLDR") {
        need(3);
        auto lfo = as_int(ex(0));
        if (lfo == 2 || lfo == 3) lfo -= 2;
        if (lfo < 0 || lfo > 1) throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid RMP LFO");
        const Value freq_value = ex(1);
        std::uint32_t freq = 0;
        if (is_int(freq_value)) {
            const auto rate = as_int(freq_value);
            if (rate < -0x8000LL || rate > 0x7fffLL) throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid ramp rate");
            freq = static_cast<std::uint32_t>(rate) & M16;
        } else {
            freq = s_15(freq_value, ins.line);
        }
        const auto amp_value = as_int(ex(2));
        std::uint32_t amp_code = 0;
        switch (amp_value) {
            case 4096: case 0: amp_code = 0; break;
            case 2048: case 1: amp_code = 1; break;
            case 1024: case 2: amp_code = 2; break;
            case 512:  case 3: amp_code = 3; break;
            default: throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid ramp range " + std::to_string(amp_value));
        }
        return ((static_cast<std::uint32_t>(lfo) | 0x2u) << 29) | (freq << 13) | (amp_code << 5) | 0x12u;
    }
    if (m == "JAM") {
        need(1);
        auto lfo = as_int(ex(0));
        if (lfo == 2 || lfo == 3) lfo -= 2;
        if (lfo < 0 || lfo > 1) throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid JAM LFO");
        return ((static_cast<std::uint32_t>(lfo) | 0x2u) << 6) | 0x13u;
    }
    if (m == "CHO") {
        if (a.size() != 2 && a.size() != 3 && a.size() != 4) {
            throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": CHO expects 2-4 operands");
        }
        const std::string cho_type = upper(trim(a[0]));
        const auto lfo = as_int(eval_expr(a[1], symbols, ins.line));
        if (lfo < 0 || lfo > 3) throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid CHO LFO");
        std::uint32_t flags = 0;
        std::uint32_t address = 0;
        std::uint32_t type_code = 0;
        if (cho_type == "RDAL") {
            flags = a.size() >= 3 && !a[2].empty() ? static_cast<std::uint32_t>(as_int(eval_expr(a[2], symbols, ins.line))) : 2u;
            type_code = 3;
        } else {
            if (a.size() != 4) throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": CHO " + cho_type + " expects LFO, flags, value");
            flags = a[2].empty() ? 0u : static_cast<std::uint32_t>(as_int(eval_expr(a[2], symbols, ins.line)));
            const Value value = eval_expr(a[3], symbols, ins.line);
            address = is_int(value) ? static_cast<std::uint32_t>(as_int(value)) & M16 : s_15(value, ins.line);
            if (cho_type == "RDA") type_code = 0;
            else if (cho_type == "SOF") type_code = 2;
            else throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": invalid CHO type " + cho_type);
        }
        return (type_code << 30) | ((flags & M6) << 24) | ((static_cast<std::uint32_t>(lfo) & M2) << 21) | ((address & M16) << 5) | 0x14u;
    }
    if (m == "RAW") { need(1); return static_cast<std::uint32_t>(as_int(ex(0))) & 0xffffffffu; }

    throw CompileError(ins.line, "Line " + std::to_string(ins.line) + ": unsupported mnemonic " + m);
}

} // namespace

CompileResult compile(std::string_view source) {
    const ParseResult parsed = parse_source(source);
    CompileResult result{};
    result.instruction_count = static_cast<std::uint32_t>(parsed.instructions.size());
    result.highest_delay_address = parsed.highest_delay;

    std::array<std::uint32_t, PROG_INSTRUCTIONS> words{};
    words.fill(0x00000011u); // NOP padding
    for (std::size_t i = 0; i < parsed.instructions.size(); ++i) {
        words[i] = encode_instruction(parsed.instructions[i], parsed.symbols, parsed.labels);
    }
    for (std::size_t i = 0; i < words.size(); ++i) {
        const std::uint32_t word = words[i];
        result.image[i * 4 + 0] = static_cast<std::uint8_t>(word >> 24);
        result.image[i * 4 + 1] = static_cast<std::uint8_t>(word >> 16);
        result.image[i * 4 + 2] = static_cast<std::uint8_t>(word >> 8);
        result.image[i * 4 + 3] = static_cast<std::uint8_t>(word);
    }
    return result;
}

} // namespace fv1::spinasm
