#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <algorithm>
#include <cmath>

using namespace Hyprlang;

static std::string removeBeginEndSpacesTabs(std::string str) {
    if (str.empty())
        return str;

    int countBefore = 0;
    while (str[countBefore] == ' ' || str[countBefore] == '\t') {
        countBefore++;
    }

    int countAfter = 0;
    while ((int)str.length() - countAfter - 1 >= 0 && (str[str.length() - countAfter - 1] == ' ' || str[str.length() - 1 - countAfter] == '\t')) {
        countAfter++;
    }

    str = str.substr(countBefore, str.length() - countBefore - countAfter);

    return str;
}

CConfig::CConfig(const char* path) {
    impl = new CConfigImpl;
    try {
        impl->path = std::filesystem::canonical(path);
    } catch (std::exception& e) { throw "Couldn't open file. File does not exist"; }

    if (!std::filesystem::exists(impl->path))
        throw "File does not exist";
}

CConfig::~CConfig() {
    delete impl;
}

void CConfig::addConfigValue(const char* name, const CConfigValue value) {
    if (m_bCommenced)
        throw "Cannot addConfigValue after commence()";

    impl->defaultValues[std::string{name}] = value;
}

void CConfig::commence() {
    m_bCommenced = true;
    for (auto& [k, v] : impl->defaultValues) {
        impl->values[k] = v;
    }
}

bool isNumber(const std::string& str, bool allowfloat) {

    std::string copy = str;
    if (*copy.begin() == '-')
        copy = copy.substr(1);

    if (copy.empty())
        return false;

    bool point = !allowfloat;
    for (auto& c : copy) {
        if (c == '.') {
            if (point)
                return false;
            point = true;
            continue;
        }

        if (!std::isdigit(c))
            return false;
    }

    return true;
}

int64_t configStringToInt(const std::string& VALUE) {
    if (VALUE.starts_with("0x")) {
        // Values with 0x are hex
        const auto VALUEWITHOUTHEX = VALUE.substr(2);
        return stol(VALUEWITHOUTHEX, nullptr, 16);
    } else if (VALUE.starts_with("rgba(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = removeBeginEndSpacesTabs(VALUE.substr(5, VALUE.length() - 6));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 3) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            uint8_t     r       = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t g           = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t b           = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t a           = std::round(std::stof(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(',')))) * 255.f);

            return a * 0x1000000L + r * 0x10000L + g * 0x100L + b;
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = std::stol(VALUEWITHOUTFUNC, nullptr, 16);

            // now we need to RGBA -> ARGB. The config holds ARGB only.
            return (RGBA >> 8) + 0x1000000 * (RGBA & 0xFF);
        }

        throw std::invalid_argument("rgba() expects length of 8 characters (4 bytes) or 4 comma separated values");

    } else if (VALUE.starts_with("rgb(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = removeBeginEndSpacesTabs(VALUE.substr(4, VALUE.length() - 5));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 2) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            uint8_t     r       = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t g           = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t b           = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));

            return 0xFF000000L + r * 0x10000L + g * 0x100L + b;
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            const auto RGB = std::stol(VALUEWITHOUTFUNC, nullptr, 16);

            return RGB + 0xFF000000;
        }

        throw std::invalid_argument("rgb() expects length of 6 characters (3 bytes) or 3 comma separated values");
    } else if (VALUE.starts_with("true") || VALUE.starts_with("on") || VALUE.starts_with("yes")) {
        return 1;
    } else if (VALUE.starts_with("false") || VALUE.starts_with("off") || VALUE.starts_with("no")) {
        return 0;
    }

    if (VALUE.empty() || !isNumber(VALUE, false))
        return 0;

    return std::stoll(VALUE);
}

CParseResult CConfig::configSetValueSafe(const std::string& command, const std::string& value) {
    CParseResult result;

    std::string  valueName;
    for (auto& c : impl->categories) {
        valueName += c + ':';
    }

    valueName += command;

    const auto VALUEIT = impl->values.find(valueName);
    if (VALUEIT == impl->values.end()) {
        result.setError("config option doesn't exist");
        return result;
    }

    switch (VALUEIT->second.m_eType) {
        case CConfigValue::eDataType::CONFIGDATATYPE_INT: {
            try {
                VALUEIT->second = {configStringToInt(value)};
            } catch (std::exception& e) {
                result.setError(std::format("failed parsing an int: {}", e.what()));
                return result;
            }
            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_FLOAT: {
            try {
                VALUEIT->second = {std::stof(value)};
            } catch (std::exception& e) {
                result.setError(std::format("failed parsing a float: {}", e.what()));
                return result;
            }
            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_VEC2: {
            try {
                const auto SPACEPOS = value.find(' ');
                if (SPACEPOS == std::string::npos)
                    throw std::runtime_error("no space");
                const auto LHS = value.substr(0, SPACEPOS);
                const auto RHS = value.substr(SPACEPOS + 1);

                if (LHS.contains(" ") || RHS.contains(" "))
                    throw std::runtime_error("too many args");

                VALUEIT->second = {SVector2D{std::stof(LHS), std::stof(RHS)}};
            } catch (std::exception& e) {
                result.setError(std::format("failed parsing a vec2: {}", e.what()));
                return result;
            }
            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_STR: {
            VALUEIT->second = {value.c_str()};
            break;
        }
        default: {
            result.setError("internal error: invalid value found (no type?)");
            return result;
        }
    }

    return result;
}

CParseResult CConfig::parseLine(std::string line) {
    CParseResult result;

    auto         commentPos  = line.find('#');
    size_t       lastHashPos = 0;

    while (commentPos != std::string::npos) {
        bool escaped = false;
        if (commentPos < line.length() - 1) {
            if (line[commentPos + 1] == '#') {
                lastHashPos = commentPos + 2;
                escaped     = true;
            }
        }

        if (!escaped) {
            line = line.substr(0, commentPos);
            break;
        } else {
            commentPos = line.find('#', lastHashPos);
        }
    }

    line = removeBeginEndSpacesTabs(line);

    auto equalsPos = line.find('=');

    if (equalsPos == std::string::npos && !line.ends_with("{") && line != "}" && !line.empty()) {
        // invalid line
        result.setError("Invalid config line");
        return result;
    }

    if (equalsPos != std::string::npos) {
        // set value
        CParseResult ret = configSetValueSafe(removeBeginEndSpacesTabs(line.substr(0, equalsPos)), removeBeginEndSpacesTabs(line.substr(equalsPos + 1)));
        if (ret.error) {
            return ret;
        }
    } else if (!line.empty()) {
        // has to be a set
        if (line.contains("}")) {
            // easiest. } or invalid.
            if (line != "}") {
                result.setError("Invalid config line");
                return result;
            }

            if (impl->categories.empty()) {
                result.setError("Stray category close");
                return result;
            }

            impl->categories.pop_back();
        } else {
            // open a category.
            if (!line.ends_with("{")) {
                result.setError("Invalid category open, garbage after {");
                return result;
            }

            line.pop_back();
            line = removeBeginEndSpacesTabs(line);
            impl->categories.push_back(line);
        }
    }

    return result;
}

CParseResult CConfig::parse() {
    if (!m_bCommenced)
        throw "Cannot parse: not commenced. You have to .commence() first.";

    impl->parseError = "";

    for (auto& [k, v] : impl->defaultValues) {
        impl->values.at(k) = v;
    }

    std::ifstream iffile(impl->path);
    if (!iffile.good())
        throw "Config file failed to open";

    std::string  line    = "";
    int          linenum = 1;

    CParseResult fileParseResult;

    while (std::getline(iffile, line)) {

        const auto RET = parseLine(line);

        if (RET.error && impl->parseError.empty()) {
            impl->parseError = RET.getError();
            fileParseResult.setError(std::format("Config error at line {}: {}", linenum, RET.errorStdString));
        }

        ++linenum;
    }

    iffile.close();

    return fileParseResult;
}

CConfigValue* CConfig::getConfigValuePtr(const char* name) {
    const auto IT = impl->values.find(std::string{name});
    return IT == impl->values.end() ? nullptr : &IT->second;
}
