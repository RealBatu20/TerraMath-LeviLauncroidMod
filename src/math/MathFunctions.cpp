#include "MathFunctions.h"

#include <cmath>
#include <random>

namespace terramath::math {

namespace {
// Lanczos coefficients g=7, n=9 — identical to MathExtensions.LANCZOS_COEFFICIENTS.
constexpr double kLanczos[8] = {
    676.5203681218851,    -1259.1392167224028,   771.32342877765313,
    -176.61502916214059,  12.507343278686905,    -0.13857109526572012,
    9.9843695780195716e-6, 1.5056327351493116e-7};

// Thread-local-free deterministic RNG. The mod is single-threaded for
// generation purposes in the native hook, so a single engine is sufficient.
std::mt19937_64 g_rng{0x9E3779B97F4A7C15ULL};
}  // namespace

double gamma(double x) {
    // Reflection for x < 0.5, Lanczos otherwise. Matches calculateGamma().
    if (x < 0.5) {
        return M_PI / (std::sin(M_PI * x) * gamma(1.0 - x));
    }
    x -= 1.0;
    double t = 0.99999999999980993;
    for (int i = 0; i < 8; ++i) {
        t += kLanczos[i] / (x + i + 1.0);
    }
    const double g = 7.5;  // x + 7.5 in Java (g = 7, +0.5)
    return std::sqrt(2.0 * M_PI) * std::pow(x + g, x + 0.5) * std::exp(-(x + g)) * t;
}

namespace {
double logGamma(double x) {
    if (x <= 0.0) return 0.0;
    double tmp = (x - 0.5) * std::log(x + 4.5) - (x + 4.5);
    double ser = 1.0 + 76.18009173 / (x + 0.0) - 86.50532033 / (x + 1.0) +
                 24.01409822 / (x + 2.0) - 1.231739516 / (x + 3.0) +
                 0.00120858003 / (x + 4.0) - 0.00000536382 / (x + 5.0);
    return tmp + std::log(ser * std::sqrt(2.0 * M_PI));
}
}  // namespace

double beta(double x, double y) {
    return std::exp(logGamma(x) + logGamma(y) - logGamma(x + y));
}

double erf(double x) {
    // Numerical Recipes rational approximation — identical to Java erf().
    double t = 1.0 / (1.0 + 0.5 * std::fabs(x));
    static const double c[10] = {-1.26551223, 1.00002368,  0.37409196,  0.09678418,
                                 -0.18628806, 0.27886807,  -1.13520398, 1.48851587,
                                 -0.82215223, 0.17087277};
    double tau =
        t * std::exp(-x * x + c[0] +
                     t * (c[1] + t * (c[2] + t * (c[3] + t * (c[4] + t * (c[5] +
                          t * (c[6] + t * (c[7] + t * (c[8] + t * c[9])))))))));
    return x >= 0.0 ? 1.0 - tau : tau - 1.0;
}

double mod(double a, double b) { return std::fmod(a, b); }
double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }
double clampv(double x, double lo, double hi) { return std::fmin(std::fmax(x, lo), hi); }
double root(double number, double degree) { return std::pow(number, 1.0 / degree); }

double gcd(double a, double b) {
    long av = static_cast<long>(a), bv = static_cast<long>(b);
    while (bv != 0) {
        long tmp = bv;
        bv = av % bv;
        av = tmp;
    }
    return static_cast<double>(std::labs(av));
}

double lcm(double a, double b) {
    long av = static_cast<long>(a), bv = static_cast<long>(b);
    if (av == 0 || bv == 0) return 0.0;
    return static_cast<double>(std::labs(av * bv)) / gcd(a, b);
}

namespace {
void extGcd(long a, long b, long& g, long& x, long& y) {
    if (b == 0) {
        g = a;
        x = 1;
        y = 0;
        return;
    }
    long g1, x1, y1;
    extGcd(b, a % b, g1, x1, y1);
    g = g1;
    x = y1;
    y = x1 - (a / b) * y1;
}
}  // namespace

double modInverse(double a, double m) {
    long g, x, y;
    extGcd(static_cast<long>(a), static_cast<long>(m), g, x, y);
    long mv = static_cast<long>(m);
    if (g != 1) return 0.0;
    long r = x % mv;
    return static_cast<double>(r < 0 ? r + mv : r);
}

double csc(double x) { return 1.0 / std::sin(x); }
double sec(double x) { return 1.0 / std::cos(x); }
double cot(double x) { return 1.0 / std::tan(x); }
double acsc(double x) { return std::asin(1.0 / x); }
double asec(double x) { return std::acos(1.0 / x); }
double acot(double x) {
    double t = std::atan(1.0 / x);
    return (x >= 0.0) ? t : t + M_PI;
}

double asinh_(double x) { return std::log(x + std::sqrt(x * x + 1.0)); }
double acosh_(double x) { return std::log(x + std::sqrt(x * x - 1.0)); }
double atanh_(double x) { return std::log((1.0 + x) / (1.0 - x)) / 2.0; }
double csch(double x) { return 1.0 / std::sinh(x); }
double sech(double x) { return 1.0 / std::cosh(x); }
double coth(double x) { return 1.0 / std::tanh(x); }
double acsch(double x) { return asinh_(1.0 / x); }
double asech(double x) { return acosh_(1.0 / x); }
double acoth(double x) { return atanh_(1.0 / x); }

double sign(double x) { return (x > 0.0) - (x < 0.0); }

void seedRng(uint64_t seed) { g_rng.seed(seed ? seed : 0x9E3779B97F4A7C15ULL); }

double rand01() {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(g_rng);
}

double randrange(double lo, double hi) {
    double v = lo + rand01() * (hi - lo);
    return std::round(v * 1000.0) / 1000.0;  // mirror Java rounding to 3 dp
}

double randnormal(double mean, double stddev) {
    std::normal_distribution<double> d(mean, stddev);
    return d(g_rng);
}

}  // namespace terramath::math
