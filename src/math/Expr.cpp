#include "Expr.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "MathFunctions.h"

namespace terramath::math {

// ---------------------------------------------------------------------------
// Function & constant tables (mirror MathFunctionsRegistry / MathConstantsRegistry)
// ---------------------------------------------------------------------------
namespace {

const std::unordered_map<std::string, int>& arityTable() {
    static const std::unordered_map<std::string, int> t = {
        // 1-arg
        {"sin", 1}, {"cos", 1}, {"tan", 1}, {"csc", 1}, {"sec", 1}, {"cot", 1},
        {"asin", 1}, {"acos", 1}, {"atan", 1}, {"acsc", 1}, {"asec", 1}, {"acot", 1},
        {"sinh", 1}, {"cosh", 1}, {"tanh", 1}, {"asinh", 1}, {"acosh", 1}, {"atanh", 1},
        {"csch", 1}, {"sech", 1}, {"coth", 1}, {"acsch", 1}, {"asech", 1}, {"acoth", 1},
        {"sqrt", 1}, {"cbrt", 1}, {"ln", 1}, {"lg", 1}, {"exp", 1},
        {"floor", 1}, {"ceil", 1}, {"round", 1}, {"abs", 1}, {"sign", 1},
        {"gamma", 1}, {"erf", 1}, {"sigmoid", 1},
        // 2-arg
        {"atan2", 2}, {"pow", 2}, {"mod", 2}, {"max", 2}, {"min", 2},
        {"gcd", 2}, {"lcm", 2}, {"modi", 2}, {"root", 2}, {"beta", 2},
        {"randnormal", 2}, {"randrange", 2},
        // 3-arg
        {"clamp", 3},
        // 0-arg
        {"rand", 0},
        // noise
        {"perlin", 3}, {"simplex", 2}, {"normal", 3}, {"blended", 3}, {"octaved", 4},
    };
    return t;
}

// exact-name constant table (Greek + ASCII aliases), as in Java.
const std::unordered_map<std::string, double>& constTable() {
    static const std::unordered_map<std::string, double> t = {
        {"π", M_PI}, {"pi", M_PI},
        {"e", M_E},
        {"φ", 1.618033988749894848204}, {"phi", 1.618033988749894848204},
        {"ζ3", 1.2020569031595942}, {"zeta3", 1.2020569031595942},
        {"K", 0.91596559417721901}, {"catalan", 0.91596559417721901},
        {"α", 2.5029078750958928}, {"alpha", 2.5029078750958928},
        {"feigenbaum", 2.5029078750958928},
        {"δ", 4.6692016091029906}, {"delta", 4.6692016091029906},
        {"feigenbaumdelta", 4.6692016091029906},
        {"Ω", 0.6889}, {"omega", 0.6889},
    };
    return t;
}

std::string toLower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c += 32;
    return s;
}

std::unique_ptr<Node> makeBinary(Node::Kind k, std::unique_ptr<Node> a,
                                 std::unique_ptr<Node> b) {
    auto n = std::make_unique<Node>();
    n->kind = k;
    n->kids.push_back(std::move(a));
    n->kids.push_back(std::move(b));
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------
// AST evaluation
// ---------------------------------------------------------------------------
namespace {
double applyFunction(const std::string& fn, const std::vector<double>& a,
                     const EvalContext& c) {
    using namespace terramath::math;
    if (fn == "sin") return std::sin(a[0]);
    if (fn == "cos") return std::cos(a[0]);
    if (fn == "tan") return std::tan(a[0]);
    if (fn == "csc") return csc(a[0]);
    if (fn == "sec") return sec(a[0]);
    if (fn == "cot") return cot(a[0]);
    if (fn == "asin") return std::asin(a[0]);
    if (fn == "acos") return std::acos(a[0]);
    if (fn == "atan") return std::atan(a[0]);
    if (fn == "acsc") return acsc(a[0]);
    if (fn == "asec") return asec(a[0]);
    if (fn == "acot") return acot(a[0]);
    if (fn == "sinh") return std::sinh(a[0]);
    if (fn == "cosh") return std::cosh(a[0]);
    if (fn == "tanh") return std::tanh(a[0]);
    if (fn == "asinh") return asinh_(a[0]);
    if (fn == "acosh") return acosh_(a[0]);
    if (fn == "atanh") return atanh_(a[0]);
    if (fn == "csch") return csch(a[0]);
    if (fn == "sech") return sech(a[0]);
    if (fn == "coth") return coth(a[0]);
    if (fn == "acsch") return acsch(a[0]);
    if (fn == "asech") return asech(a[0]);
    if (fn == "acoth") return acoth(a[0]);
    if (fn == "sqrt") return std::sqrt(a[0]);
    if (fn == "cbrt") return std::cbrt(a[0]);
    if (fn == "ln") return std::log(a[0]);
    if (fn == "lg") return std::log10(a[0]);
    if (fn == "exp") return std::exp(a[0]);
    if (fn == "floor") return std::floor(a[0]);
    if (fn == "ceil") return std::ceil(a[0]);
    if (fn == "round") return std::round(a[0]);
    if (fn == "abs") return std::fabs(a[0]);
    if (fn == "sign") return sign(a[0]);
    if (fn == "gamma") return gamma(a[0]);
    if (fn == "erf") return erf(a[0]);
    if (fn == "sigmoid") return sigmoid(a[0]);
    if (fn == "atan2") return std::atan2(a[0], a[1]);
    if (fn == "pow") return std::pow(a[0], a[1]);
    if (fn == "mod") return mod(a[0], a[1]);
    if (fn == "max") return std::fmax(a[0], a[1]);
    if (fn == "min") return std::fmin(a[0], a[1]);
    if (fn == "gcd") return gcd(a[0], a[1]);
    if (fn == "lcm") return lcm(a[0], a[1]);
    if (fn == "modi") return modInverse(a[0], a[1]);
    if (fn == "root") return root(a[0], a[1]);
    if (fn == "beta") return beta(a[0], a[1]);
    if (fn == "randnormal") return randnormal(a[0], a[1]);
    if (fn == "randrange") return randrange(a[0], a[1]);
    if (fn == "clamp") return clampv(a[0], a[1], a[2]);
    if (fn == "rand") return rand01();
    if (c.noise) {
        if (fn == "perlin") return c.noise->perlin(a[0], a[1], a[2]);
        if (fn == "simplex") return c.noise->simplex(a[0], a[1]);
        if (fn == "normal") return c.noise->normal(a[0], a[1], a[2]);
        if (fn == "blended") return c.noise->blended(a[0], a[1], a[2]);
        if (fn == "octaved")
            return c.noise->octaved(a[0], a[1], static_cast<int>(a[2]), a[3]);
    } else if (fn == "perlin" || fn == "simplex" || fn == "normal" ||
               fn == "blended" || fn == "octaved") {
        return 0.0;  // noise requested but unavailable in this context
    }
    throw FormulaError("Unsupported function: " + fn);
}
}  // namespace

double Node::eval(const EvalContext& c) const {
    switch (kind) {
        case Kind::Num:
        case Kind::Const:
            return num;
        case Kind::Var:
            return var == 'x' ? c.x : (var == 'y' ? c.y : c.z);
        case Kind::Neg:
            return -kids[0]->eval(c);
        case Kind::Add:
            return kids[0]->eval(c) + kids[1]->eval(c);
        case Kind::Sub:
            return kids[0]->eval(c) - kids[1]->eval(c);
        case Kind::Mul:
            return kids[0]->eval(c) * kids[1]->eval(c);
        case Kind::Div:
            return kids[0]->eval(c) / kids[1]->eval(c);
        case Kind::Mod:
            return std::fmod(kids[0]->eval(c), kids[1]->eval(c));
        case Kind::Pow:
            return std::pow(kids[0]->eval(c), kids[1]->eval(c));
        case Kind::Fact:
            // Postfix `!` generalised via gamma: n! = gamma(n + 1).
            return gamma(kids[0]->eval(c) + 1.0);
        case Kind::Call: {
            std::vector<double> args;
            args.reserve(kids.size());
            for (const auto& k : kids) args.push_back(k->eval(c));
            return applyFunction(fn, args, c);
        }
    }
    return 0.0;
}

// ---------------------------------------------------------------------------
// Parser (recursive descent)
// ---------------------------------------------------------------------------
namespace {

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    std::unique_ptr<Node> parse() {
        auto n = parseExpr();
        skipWs();
        if (pos_ != s_.size())
            throw FormulaError("Unexpected character '" + std::string(1, s_[pos_]) +
                               "' at position " + std::to_string(pos_));
        return n;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    void skipWs() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }
    char peek() {
        skipWs();
        return pos_ < s_.size() ? s_[pos_] : '\0';
    }
    bool isIdentStart(unsigned char ch) {
        return std::isalpha(ch) || ch == '_' || ch >= 0x80;  // UTF-8 (Greek) bytes
    }
    bool isIdentPart(unsigned char ch) {
        return std::isalnum(ch) || ch == '_' || ch >= 0x80;
    }

    static std::unique_ptr<Node> make(Node::Kind k) {
        auto n = std::make_unique<Node>();
        n->kind = k;
        return n;
    }
    static std::unique_ptr<Node> binary(Node::Kind k, std::unique_ptr<Node> a,
                                        std::unique_ptr<Node> b) {
        auto n = make(k);
        n->kids.push_back(std::move(a));
        n->kids.push_back(std::move(b));
        return n;
    }

    std::unique_ptr<Node> parseExpr() {
        auto n = parseTerm();
        for (;;) {
            char c = peek();
            if (c == '+') { ++pos_; n = binary(Node::Kind::Add, std::move(n), parseTerm()); }
            else if (c == '-') { ++pos_; n = binary(Node::Kind::Sub, std::move(n), parseTerm()); }
            else break;
        }
        return n;
    }

    std::unique_ptr<Node> parseTerm() {
        auto n = parseUnary();
        for (;;) {
            char c = peek();
            if (c == '*') { ++pos_; n = binary(Node::Kind::Mul, std::move(n), parseUnary()); }
            else if (c == '/') { ++pos_; n = binary(Node::Kind::Div, std::move(n), parseUnary()); }
            else if (c == '%') { ++pos_; n = binary(Node::Kind::Mod, std::move(n), parseUnary()); }
            else break;
        }
        return n;
    }

    std::unique_ptr<Node> parseUnary() {
        char c = peek();
        if (c == '-') { ++pos_; auto n = make(Node::Kind::Neg); n->kids.push_back(parseUnary()); return n; }
        if (c == '+') { ++pos_; return parseUnary(); }
        return parsePower();
    }

    std::unique_ptr<Node> parsePower() {
        auto base = parsePostfix();
        if (peek() == '^') {
            ++pos_;
            // Right-associative; right operand may carry its own unary sign.
            return binary(Node::Kind::Pow, std::move(base), parseUnary());
        }
        return base;
    }

    std::unique_ptr<Node> parsePostfix() {
        auto n = parsePrimary();
        while (peek() == '!') {
            ++pos_;
            auto f = make(Node::Kind::Fact);
            f->kids.push_back(std::move(n));
            n = std::move(f);
        }
        return n;
    }

    std::unique_ptr<Node> parsePrimary() {
        char c = peek();
        if (c == '(') {
            ++pos_;
            auto n = parseExpr();
            if (peek() != ')') throw FormulaError("Expected ')'");
            ++pos_;
            return n;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return parseNumber();
        if (isIdentStart(static_cast<unsigned char>(c))) return parseIdentifier();
        throw FormulaError(c == '\0' ? "Unexpected end of formula"
                                     : "Unexpected character '" + std::string(1, c) + "'");
    }

    std::unique_ptr<Node> parseNumber() {
        skipWs();
        size_t start = pos_;
        while (pos_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.'))
            ++pos_;
        // optional scientific exponent: 1e3, 2.5E-4
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            size_t save = pos_;
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
            } else {
                pos_ = save;  // bare 'e' -> Euler constant, not an exponent
            }
        }
        auto n = make(Node::Kind::Num);
        n->num = std::stod(s_.substr(start, pos_ - start));
        return n;
    }

    std::unique_ptr<Node> parseIdentifier() {
        skipWs();
        size_t start = pos_;
        while (pos_ < s_.size() && isIdentPart(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        std::string name = s_.substr(start, pos_ - start);

        // Function call?
        if (peek() == '(') {
            std::string fn = toLower(name);
            auto it = arityTable().find(fn);
            if (it == arityTable().end()) throw FormulaError("Unknown function: " + name);
            ++pos_;  // consume '('
            auto call = make(Node::Kind::Call);
            call->fn = fn;
            if (peek() != ')') {
                call->kids.push_back(parseExpr());
                while (peek() == ',') {
                    ++pos_;
                    call->kids.push_back(parseExpr());
                }
            }
            if (peek() != ')') throw FormulaError("Expected ')' after arguments to " + name);
            ++pos_;
            if (static_cast<int>(call->kids.size()) != it->second)
                throw FormulaError("Function " + name + " expects " +
                                   std::to_string(it->second) + " argument(s)");
            return call;
        }

        // Variable?
        if (name == "x" || name == "y" || name == "z") {
            auto n = make(Node::Kind::Var);
            n->var = name[0];
            return n;
        }

        // Constant? (exact, then lowercase fallback for ASCII aliases)
        auto cit = constTable().find(name);
        if (cit == constTable().end()) cit = constTable().find(toLower(name));
        if (cit != constTable().end()) {
            auto n = make(Node::Kind::Const);
            n->num = cit->second;
            return n;
        }

        throw FormulaError("Unknown variable or constant: " + name);
    }
};

}  // namespace

std::unique_ptr<Node> parseExpression(const std::string& src) {
    Parser p(src);
    return p.parse();
}

std::unique_ptr<Node> parseFormula(const std::string& formula, bool& isEquation) {
    auto eq = formula.find('=');
    if (eq != std::string::npos) {
        isEquation = true;
        std::string lhs = formula.substr(0, eq);
        std::string rhs = formula.substr(eq + 1);
        // (rhs) - (lhs), exactly as FormulaFormatter.convertToJavaExpression.
        return makeBinary(Node::Kind::Sub, parseExpression(rhs), parseExpression(lhs));
    }
    isEquation = false;
    return parseExpression(formula);
}

}  // namespace terramath::math
