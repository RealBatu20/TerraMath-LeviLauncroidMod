// TerraMath-LeviLauncroidMod
// -----------------------------------------------------------------------------
// Native C++ port of TerraMath's `MathExtensions` (Java).
// Source of truth: addavriance/TerraMath
//   common/src/main/java/me/adda/terramath/math/functions/MathExtensions.java
//
// These are pure numeric helpers with no Minecraft / Android dependencies, so
// they can be unit-tested on the host. Behaviour is matched to the Java
// implementation (same algorithms / coefficients) so that converted worlds
// look the same.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace terramath::math {

// Gamma / special functions (Lanczos approximation, mirrors Java).
double gamma(double x);
double beta(double x, double y);
double erf(double x);

// Misc helpers.
double mod(double a, double b);
double sigmoid(double x);
double clampv(double x, double lo, double hi);
double root(double number, double degree);

// Integer-theoretic helpers (operate on truncated integer values, as in Java).
double gcd(double a, double b);
double lcm(double a, double b);
double modInverse(double a, double m);

// Reciprocal / inverse trig & hyperbolic functions (not in <cmath>).
double csc(double x);
double sec(double x);
double cot(double x);
double acsc(double x);
double asec(double x);
double acot(double x);

double asinh_(double x);
double acosh_(double x);
double atanh_(double x);
double csch(double x);
double sech(double x);
double coth(double x);
double acsch(double x);
double asech(double x);
double acoth(double x);

double sign(double x);  // Math.signum

// Deterministic RNG used by rand()/randrange()/randnormal(). Seeded from the
// world seed so generation is reproducible (unlike Java's ThreadLocalRandom,
// which is non-deterministic — see CONVERSION_REPORT.md, "Random functions").
void   seedRng(uint64_t seed);
double rand01();
double randrange(double lo, double hi);
double randnormal(double mean, double stddev);

}  // namespace terramath::math
