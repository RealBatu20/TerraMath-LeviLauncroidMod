#include "Config.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace terramath {

namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

// Find `"key"` then ':' and return the index just after ':' (or npos).
size_t valuePos(const std::string& j, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t k = j.find(needle);
    if (k == std::string::npos) return std::string::npos;
    size_t colon = j.find(':', k + needle.size());
    if (colon == std::string::npos) return std::string::npos;
    size_t p = colon + 1;
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\n' || j[p] == '\r')) ++p;
    return p;
}

std::string readString(const std::string& j, const std::string& key, const std::string& def) {
    size_t p = valuePos(j, key);
    if (p == std::string::npos || p >= j.size() || j[p] != '"') return def;
    ++p;
    std::string out;
    while (p < j.size() && j[p] != '"') {
        if (j[p] == '\\' && p + 1 < j.size()) {
            char n = j[p + 1];
            switch (n) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                default: out += n; break;
            }
            p += 2;
        } else {
            out += j[p++];
        }
    }
    return out;
}

double readNumber(const std::string& j, const std::string& key, double def) {
    size_t p = valuePos(j, key);
    if (p == std::string::npos) return def;
    try {
        return std::stod(j.substr(p));
    } catch (...) {
        return def;
    }
}

bool readBool(const std::string& j, const std::string& key, bool def) {
    size_t p = valuePos(j, key);
    if (p == std::string::npos) return def;
    return j.compare(p, 4, "true") == 0;
}

}  // namespace

std::string Config::toJson() const {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);
    o << "{\n";
    o << "  \"baseFormula\": \"" << jsonEscape(baseFormula) << "\",\n";
    o << "  \"formula\": \"" << jsonEscape(terrain.formula) << "\",\n";
    o << "  \"useDefaultFormula\": " << (useDefaultFormula ? "true" : "false") << ",\n";
    o << "  \"coordinateScale\": " << terrain.coordinateScale << ",\n";
    o << "  \"baseHeight\": " << terrain.baseHeight << ",\n";
    o << "  \"heightVariation\": " << terrain.heightVariation << ",\n";
    o << "  \"smoothingFactor\": " << terrain.smoothingFactor << ",\n";
    o << "  \"useDensityMode\": " << (terrain.useDensityMode ? "true" : "false") << ",\n";
    o << "  \"noiseType\": \"" << noiseTypeName(terrain.noiseType) << "\",\n";
    o << "  \"noiseScaleX\": " << terrain.noiseScaleX << ",\n";
    o << "  \"noiseScaleY\": " << terrain.noiseScaleY << ",\n";
    o << "  \"noiseScaleZ\": " << terrain.noiseScaleZ << ",\n";
    o << "  \"noiseHeightScale\": " << terrain.noiseHeightScale << ",\n";
    o << "  \"terrainSignature\": \"" << jsonEscape(terrainSignature) << "\"\n";
    o << "}\n";
    return o.str();
}

Config Config::fromJson(const std::string& j) {
    Config c;
    c.baseFormula = readString(j, "baseFormula", "");
    c.terrain.formula = readString(j, "formula", "");
    c.useDefaultFormula = readBool(j, "useDefaultFormula", true);
    c.terrain.coordinateScale = readNumber(j, "coordinateScale", 5.0);
    c.terrain.baseHeight = readNumber(j, "baseHeight", 64.0);
    c.terrain.heightVariation = readNumber(j, "heightVariation", 32.5);
    c.terrain.smoothingFactor = readNumber(j, "smoothingFactor", 0.55);
    c.terrain.useDensityMode = readBool(j, "useDensityMode", false);
    c.terrain.noiseType = noiseTypeFromName(readString(j, "noiseType", "NONE"));
    c.terrain.noiseScaleX = readNumber(j, "noiseScaleX", 30.0);
    c.terrain.noiseScaleY = readNumber(j, "noiseScaleY", 30.0);
    c.terrain.noiseScaleZ = readNumber(j, "noiseScaleZ", 30.0);
    c.terrain.noiseHeightScale = readNumber(j, "noiseHeightScale", 4.0);
    c.terrainSignature = readString(j, "terrainSignature", "");

    // Mirror ModConfig.initializeDefaultValues: fall back to baseFormula.
    if (c.terrain.formula.empty() && c.useDefaultFormula) c.terrain.formula = c.baseFormula;
    return c;
}

Config Config::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Config{};
    std::ostringstream ss;
    ss << in.rdbuf();
    return fromJson(ss.str());
}

bool Config::save(const std::string& path) const {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << toJson();
        if (!out) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace terramath
