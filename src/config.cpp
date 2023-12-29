#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <algorithm>
#include <cmath>

using namespace Hyprlang;
extern "C" char**  environ;

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

    impl->envVariables.clear();
    for (char** env = environ; *env; ++env) {
        const std::string ENVVAR   = *env;
        const auto        VARIABLE = ENVVAR.substr(0, ENVVAR.find_first_of('='));
        const auto        VALUE    = ENVVAR.substr(ENVVAR.find_first_of('=') + 1);
        impl->envVariables.push_back({VARIABLE, VALUE});
    }

    std::sort(impl->envVariables.begin(), impl->envVariables.end(), [&](const auto& a, const auto& b) { return a.name.length() > b.name.length(); });
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

static bool isNumber(const std::string& str, bool allowfloat) {

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

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

static int64_t configStringToInt(const std::string& VALUE) {
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
        result.setError(std::format("config option <{}> does not exist.", valueName));
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

CParseResult CConfig::parseVariable(const std::string& lhs, const std::string& rhs, bool dynamic) {
    auto IT = std::find_if(impl->variables.begin(), impl->variables.end(), [&](const auto& v) { return v.name == lhs.substr(1); });

    if (IT != impl->variables.end())
        IT->value = rhs;
    else {
        impl->variables.push_back({lhs.substr(1), rhs});
        std::sort(impl->variables.begin(), impl->variables.end(), [](const auto& lhs, const auto& rhs) { return lhs.name.length() > rhs.name.length(); });
    }

    if (dynamic) {
        if (IT == impl->variables.end())
            IT = std::find_if(impl->variables.begin(), impl->variables.end(), [&](const auto& v) { return v.name == lhs.substr(1); });
        for (auto& l : IT->linesContainingVar) {
            parseLine(l, true);
        }
    }

    CParseResult result;
    return result;
}

CParseResult CConfig::parseLine(std::string line, bool dynamic) {
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
        // set value or call handler
        CParseResult ret;
        auto         LHS = removeBeginEndSpacesTabs(line.substr(0, equalsPos));
        auto         RHS = removeBeginEndSpacesTabs(line.substr(equalsPos + 1));

        if (LHS.empty()) {
            result.setError("Empty lhs.");
            return result;
        }

        if (*LHS.begin() == '$')
            return parseVariable(LHS, RHS, dynamic);

        // limit unwrapping iterations to 100. if exceeds, raise error
        for (size_t i = 0; i < 100; ++i) {
            bool anyMatch = false;
            for (auto& var : impl->variables) {
                const auto LHSIT = LHS.find("$" + var.name);
                const auto RHSIT = RHS.find("$" + var.name);

                if (LHSIT != std::string::npos)
                    replaceAll(LHS, "$" + var.name, var.value);
                if (RHSIT != std::string::npos)
                    replaceAll(RHS, "$" + var.name, var.value);

                if (RHSIT == std::string::npos && LHSIT == std::string::npos)
                    continue;
                else
                    var.linesContainingVar.push_back(line);

                anyMatch = true;
            }

            if (!anyMatch)
                break;

            if (i == 99) {
                result.setError("Expanding variables exceeded max iteration limit");
                return result;
            }
        }

        bool found = false;
        for (auto& h : impl->handlers) {
            if (!h.options.allowFlags && h.name != LHS)
                continue;

            if (h.options.allowFlags && !LHS.starts_with(h.name))
                continue;

            ret   = h.func(LHS.c_str(), RHS.c_str());
            found = true;
        }

        if (!found)
            ret = configSetValueSafe(LHS, RHS);

        if (ret.error)
            return ret;
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

    clearState();

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

    if (!impl->categories.empty()) {
        fileParseResult.setError("Unclosed category at EOF");
        impl->categories.clear();
        return fileParseResult;
    }

    return fileParseResult;
}

CParseResult CConfig::parseDynamic(const char* line) {
    return parseLine(line, true);
}

CParseResult CConfig::parseDynamic(const char* command, const char* value) {
    return parseLine(std::string{command} + "=" + std::string{value}, true);
}

void CConfig::clearState() {
    impl->categories.clear();
    impl->parseError = "";
    impl->variables  = impl->envVariables;
}

CConfigValue* CConfig::getConfigValuePtr(const char* name) {
    const auto IT = impl->values.find(std::string{name});
    return IT == impl->values.end() ? nullptr : &IT->second;
}

void CConfig::registerHandler(PCONFIGHANDLERFUNC func, const char* name, SHandlerOptions options) {
    impl->handlers.push_back(SHandler{name, options, func});
}