// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Expression AST + recursive-descent parser.
//
// TerraMath (Java) builds a Java source string from the user's formula and
// JIT-compiles it with Janino at runtime (FormulaParser/FormulaFormatter).
// Janino does not exist on Android-native, so we parse the user's formula
// directly into an AST and interpret it. The supported function names,
// constants, the `^` power operator, postfix `!` factorial and the implicit
// `lhs = rhs` -> `(rhs)-(lhs)` equation rewrite all mirror the Java behaviour
// (MathFunctionsRegistry / MathConstantsRegistry / PowerParser / FactorialParser).
// -----------------------------------------------------------------------------
#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Noise.h"

namespace terramath::math {

struct EvalContext {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    const Noise* noise = nullptr;
};

class FormulaError : public std::runtime_error {
public:
    explicit FormulaError(const std::string& what) : std::runtime_error(what) {}
};

struct Node {
    enum class Kind { Num, Var, Const, Neg, Add, Sub, Mul, Div, Mod, Pow, Fact, Call };
    Kind kind;
    double num = 0.0;         // Num / Const value
    char var = 0;             // Var: 'x' | 'y' | 'z'
    std::string fn;           // Call: lowercased function name
    std::vector<std::unique_ptr<Node>> kids;

    double eval(const EvalContext& c) const;
};

// Parses one expression side into an AST. Throws FormulaError on bad syntax,
// unknown identifiers, or wrong function arity.
std::unique_ptr<Node> parseExpression(const std::string& src);

// Parses a full formula. If it contains '=', returns AST for (rhs)-(lhs) and
// sets isEquation = true; otherwise returns the expression AST.
std::unique_ptr<Node> parseFormula(const std::string& formula, bool& isEquation);

}  // namespace terramath::math
