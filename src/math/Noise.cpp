#include "Noise.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace terramath::math {

Noise::Noise(uint64_t seed) {
    // Seed two independent permutation tables (one for Perlin, one for Simplex)
    // by shuffling 0..255 with a deterministic engine.
    std::mt19937_64 rng(seed ? seed : 0x2545F4914F6CDD1DULL);

    std::array<int, 256> p{};
    std::iota(p.begin(), p.end(), 0);
    std::shuffle(p.begin(), p.end(), rng);
    for (int i = 0; i < 512; ++i) perm_[i] = p[i & 255];

    std::array<int, 256> q{};
    std::iota(q.begin(), q.end(), 0);
    std::shuffle(q.begin(), q.end(), rng);
    for (int i = 0; i < 512; ++i) permSimp_[i] = q[i & 255];
}

double Noise::fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
double Noise::lerp(double t, double a, double b) { return a + t * (b - a); }

double Noise::grad(int hash, double x, double y, double z) {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

double Noise::perlin(double x, double y, double z) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;
    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);
    double u = fade(x), v = fade(y), w = fade(z);

    int A = perm_[X] + Y, AA = perm_[A] + Z, AB = perm_[A + 1] + Z;
    int B = perm_[X + 1] + Y, BA = perm_[B] + Z, BB = perm_[B + 1] + Z;

    double res = lerp(
        w,
        lerp(v, lerp(u, grad(perm_[AA], x, y, z), grad(perm_[BA], x - 1, y, z)),
                lerp(u, grad(perm_[AB], x, y - 1, z), grad(perm_[BB], x - 1, y - 1, z))),
        lerp(v, lerp(u, grad(perm_[AA + 1], x, y, z - 1), grad(perm_[BA + 1], x - 1, y, z - 1)),
                lerp(u, grad(perm_[AB + 1], x, y - 1, z - 1),
                        grad(perm_[BB + 1], x - 1, y - 1, z - 1))));
    return res;  // already in ~[-1, 1]
}

double Noise::simplex(double xin, double yin) const {
    static const double F2 = 0.5 * (std::sqrt(3.0) - 1.0);
    static const double G2 = (3.0 - std::sqrt(3.0)) / 6.0;
    static const int grad3[12][2] = {{1, 1},   {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0},
                                     {1, 0},   {-1, 0}, {0, 1},  {0, -1},  {0, 1}, {0, -1}};

    double s = (xin + yin) * F2;
    int i = static_cast<int>(std::floor(xin + s));
    int j = static_cast<int>(std::floor(yin + s));
    double t = (i + j) * G2;
    double X0 = i - t, Y0 = j - t;
    double x0 = xin - X0, y0 = yin - Y0;

    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; } else { i1 = 0; j1 = 1; }

    double x1 = x0 - i1 + G2, y1 = y0 - j1 + G2;
    double x2 = x0 - 1.0 + 2.0 * G2, y2 = y0 - 1.0 + 2.0 * G2;

    int ii = i & 255, jj = j & 255;
    int gi0 = permSimp_[ii + permSimp_[jj]] % 12;
    int gi1 = permSimp_[ii + i1 + permSimp_[jj + j1]] % 12;
    int gi2 = permSimp_[ii + 1 + permSimp_[jj + 1]] % 12;

    auto corner = [&](double xc, double yc, int gi) -> double {
        double tt = 0.5 - xc * xc - yc * yc;
        if (tt < 0) return 0.0;
        tt *= tt;
        return tt * tt * (grad3[gi][0] * xc + grad3[gi][1] * yc);
    };

    double n0 = corner(x0, y0, gi0);
    double n1 = corner(x1, y1, gi1);
    double n2 = corner(x2, y2, gi2);
    return 70.0 * (n0 + n1 + n2);  // scaled to ~[-1, 1]
}

double Noise::octaved(double x, double z, int octaves, double persistence) const {
    if (octaves < 1) octaves = 1;
    double total = 0.0, frequency = 1.0, amplitude = 1.0, maxValue = 0.0;
    for (int i = 0; i < octaves; ++i) {
        total += perlin(x * frequency, 0.0, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    return maxValue > 0.0 ? total / maxValue : 0.0;
}

double Noise::normal(double x, double y, double z) const {
    // Approximation of Minecraft's NormalNoise: a small octave sum (the Java
    // version uses 2 PerlinNoise instances at firstOctave=4, amplitude 0.25).
    double a = perlin(x, y, z);
    double b = perlin(x * 2.0 + 31.41, y * 2.0 + 17.0, z * 2.0 - 53.7);
    return (a + 0.5 * b) / 1.5;
}

double Noise::blended(double x, double y, double z) const {
    // Approximation of Minecraft's BlendedNoise (xzScale 0.25, yScale 0.125).
    return perlin(x * 0.25, y * 0.125, z * 0.25);
}

}  // namespace terramath::math
