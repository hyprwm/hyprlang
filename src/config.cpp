#include "config.hpp"
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <format>
#include <algorithm>
#include <cmath>
#include <expected>
#include <sstream>
#include <cstring>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/ConstVarList.hpp>

using namespace Hyprlang;
using namespace Hyprutils::String;

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
// NOLINTNEXTLINE
extern "C" char** environ;
#endif

// defines
inline constexpr const char* ANONYMOUS_KEY           = "__hyprlang_internal_anonymous_key";
inline constexpr const char* MULTILINE_SPACE_CHARSET = " \t";
//

static size_t seekABIStructSize(const void* begin, size_t startOffset, size_t maxSize) {
    for (size_t off = startOffset; off < maxSize; off += 4) {
        if (*(int*)((unsigned char*)begin + off) == HYPRLANG_END_MAGIC)
            return off;
    }

    return 0;
}

static std::expected<std::string, eGetNextLineFailure> getNextLine(std::istream& str, int& rawLineNum, int& lineNum) {
    std::string line     = "";
    std::string nextLine = "";

    if (!std::getline(str, line))
        return std::unexpected(GETNEXTLINEFAILURE_EOF);

    lineNum = ++rawLineNum;

    while (line.length() > 0 && line.at(line.length() - 1) == '\\') {
        const auto lastNonSpace = line.length() < 2 ? -1 : line.find_last_not_of(MULTILINE_SPACE_CHARSET, line.length() - 2);
        line                    = line.substr(0, lastNonSpace + 1);

        if (!std::getline(str, nextLine))
            return std::unexpected(GETNEXTLINEFAILURE_BACKSLASH);

        ++rawLineNum;
        line += nextLine;
    }

    return line;
}

CConfig::CConfig(const char* path, const Hyprlang::SConfigOptions& options_) : impl(new CConfigImpl) {
    SConfigOptions options;
    std::memcpy(&options, &options_, seekABIStructSize(&options_, 16, sizeof(SConfigOptions)));

    if (options.pathIsStream)
        impl->rawConfigString = path;
    else
        impl->path = path;

    if (!options.pathIsStream && !std::filesystem::exists(impl->path)) {
        if (!options.allowMissingConfig)
            throw "File does not exist";
    }

    impl->envVariables.clear();
    for (char** env = environ; *env; ++env) {
        const std::string ENVVAR   = *env ? *env : "";
        const auto        VARIABLE = ENVVAR.substr(0, ENVVAR.find_first_of('='));
        const auto        VALUE    = ENVVAR.substr(ENVVAR.find_first_of('=') + 1);
        impl->envVariables.push_back({VARIABLE, VALUE});
    }

    std::ranges::sort(impl->envVariables, [&](const auto& a, const auto& b) { return a.name.length() > b.name.length(); });

    impl->configOptions = options;
}

CConfig::~CConfig() {
    delete impl;
}

void CConfig::addConfigValue(const char* name, const CConfigValue& value) {
    if (m_bCommenced)
        throw "Cannot addConfigValue after commence()";

    if ((eDataType)value.m_eType != CONFIGDATATYPE_CUSTOM && (eDataType)value.m_eType != CONFIGDATATYPE_STR)
        impl->defaultValues.emplace(name, SConfigDefaultValue{.data = value.getValue(), .type = (eDataType)value.m_eType});
    else if ((eDataType)value.m_eType == CONFIGDATATYPE_STR)
        impl->defaultValues.emplace(name, SConfigDefaultValue{.data = std::string{std::any_cast<const char*>(value.getValue())}, .type = (eDataType)value.m_eType});
    else
        impl->defaultValues.emplace(name,
                                    SConfigDefaultValue{.data    = reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->defaultVal,
                                                        .type    = (eDataType)value.m_eType,
                                                        .handler = reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->handler,
                                                        .dtor    = reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->dtor});
}

void CConfig::addSpecialConfigValue(const char* cat, const char* name, const CConfigValue& value) {
    const auto IT = std::ranges::find_if(impl->specialCategoryDescriptors, [&](const auto& other) { return other->name == cat; });

    if (IT == impl->specialCategoryDescriptors.end())
        throw "No such category";

    if ((eDataType)value.m_eType != CONFIGDATATYPE_CUSTOM && (eDataType)value.m_eType != CONFIGDATATYPE_STR)
        IT->get()->defaultValues.emplace(name, SConfigDefaultValue{.data = value.getValue(), .type = (eDataType)value.m_eType});
    else if ((eDataType)value.m_eType == CONFIGDATATYPE_STR)
        IT->get()->defaultValues.emplace(name, SConfigDefaultValue{.data = std::string{std::any_cast<const char*>(value.getValue())}, .type = (eDataType)value.m_eType});
    else
        IT->get()->defaultValues.emplace(name,
                                         SConfigDefaultValue{.data    = reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->defaultVal,
                                                             .type    = (eDataType)value.m_eType,
                                                             .handler = reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->handler,
                                                             .dtor    = reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->dtor});

    const auto CAT = std::ranges::find_if(impl->specialCategories, [cat](const auto& other) { return other->name == cat && other->isStatic; });

    if (CAT != impl->specialCategories.end())
        CAT->get()->values[name].defaultFrom(IT->get()->defaultValues[name]);
}

void CConfig::removeSpecialConfigValue(const char* cat, const char* name) {
    const auto IT = std::ranges::find_if(impl->specialCategoryDescriptors, [&](const auto& other) { return other->name == cat; });

    if (IT == impl->specialCategoryDescriptors.end())
        throw "No such category";

    std::erase_if(IT->get()->defaultValues, [name](const auto& other) { return other.first == name; });
}

void CConfig::addSpecialCategory(const char* name, SSpecialCategoryOptions options_) {
    SSpecialCategoryOptions options;
    std::memcpy(&options, &options_, seekABIStructSize(&options_, 8, sizeof(SSpecialCategoryOptions)));

    const auto PDESC          = impl->specialCategoryDescriptors.emplace_back(std::make_unique<SSpecialCategoryDescriptor>()).get();
    PDESC->name               = name;
    PDESC->key                = options.key ? options.key : "";
    PDESC->dontErrorOnMissing = options.ignoreMissing;

    if (!options.key && !options.anonymousKeyBased) {
        const auto PCAT  = impl->specialCategories.emplace_back(std::make_unique<SSpecialCategory>()).get();
        PCAT->descriptor = PDESC;
        PCAT->name       = name;
        PCAT->key        = "";
        PCAT->isStatic   = true;
    }

    if (options.anonymousKeyBased) {
        PDESC->key       = ANONYMOUS_KEY;
        PDESC->anonymous = true;
    }

    // sort longest to shortest
    std::ranges::sort(impl->specialCategories, [](const auto& a, const auto& b) -> int { return a->name.length() > b->name.length(); });
    std::ranges::sort(impl->specialCategoryDescriptors, [](const auto& a, const auto& b) -> int { return a->name.length() > b->name.length(); });
}

void CConfig::removeSpecialCategory(const char* name) {
    std::erase_if(impl->specialCategories, [name](const auto& other) { return other->name == name; });
    std::erase_if(impl->specialCategoryDescriptors, [name](const auto& other) { return other->name == name; });
}

void CConfig::applyDefaultsToCat(SSpecialCategory& cat) {
    for (auto& [k, v] : cat.descriptor->defaultValues) {
        cat.values[k].defaultFrom(v);
    }
}

void CConfig::commence() {
    m_bCommenced = true;
    for (auto& [k, v] : impl->defaultValues) {
        impl->values[k].defaultFrom(v);
    }
}

static std::expected<int64_t, std::string> configStringToInt(const std::string& VALUE) {
    auto parseHex = [](const std::string& value) -> std::expected<int64_t, std::string> {
        try {
            size_t position;
            auto   result = stoll(value, &position, 16);
            if (position == value.size())
                return result;
        } catch (const std::exception&) {}
        return std::unexpected("invalid hex " + value);
    };
    if (VALUE.starts_with("0x")) {
        // Values with 0x are hex
        return parseHex(VALUE);
    } else if (VALUE.starts_with("rgba(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(5, VALUE.length() - 6));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 3) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t a           = 0;
            try {
                a = std::round(std::stof(trim(rolling.substr(0, rolling.find(',')))) * 255.f);
            } catch (std::exception& e) { return std::unexpected("failed parsing " + VALUEWITHOUTFUNC); }

            if (!r.has_value() || !g.has_value() || !b.has_value())
                return std::unexpected("failed parsing " + VALUEWITHOUTFUNC);

            return (a * (Hyprlang::INT)0x1000000) + (r.value() * (Hyprlang::INT)0x10000) + (g.value() * (Hyprlang::INT)0x100) + b.value();
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = parseHex(VALUEWITHOUTFUNC);

            if (!RGBA.has_value())
                return RGBA;

            // now we need to RGBA -> ARGB. The config holds ARGB only.
            return (RGBA.value() >> 8) + (0x1000000 * (RGBA.value() & 0xFF));
        }

        return std::unexpected("rgba() expects length of 8 characters (4 bytes) or 4 comma separated values");

    } else if (VALUE.starts_with("rgb(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(4, VALUE.length() - 5));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 2) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));

            if (!r.has_value() || !g.has_value() || !b.has_value())
                return std::unexpected("failed parsing " + VALUEWITHOUTFUNC);

            return (Hyprlang::INT)0xFF000000 + (r.value() * (Hyprlang::INT)0x10000) + (g.value() * (Hyprlang::INT)0x100) + b.value();
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            const auto RGB = parseHex(VALUEWITHOUTFUNC);

            if (!RGB.has_value())
                return RGB;

            return RGB.value() + 0xFF000000;
        }

        return std::unexpected("rgb() expects length of 6 characters (3 bytes) or 3 comma separated values");
    } else if (VALUE.starts_with("true") || VALUE.starts_with("on") || VALUE.starts_with("yes")) {
        return 1;
    } else if (VALUE.starts_with("false") || VALUE.starts_with("off") || VALUE.starts_with("no")) {
        return 0;
    }

    if (VALUE.empty() || !isNumber(VALUE, false))
        return std::unexpected("cannot parse \"" + VALUE + "\" as an int.");

    try {
        const auto RES = std::stoll(VALUE);
        return RES;
    } catch (std::exception& e) { return std::unexpected(std::string{"stoll threw: "} + e.what()); }

    return 0;
}

// found, result
std::pair<bool, CParseResult> CConfig::configSetValueSafe(const std::string& command, const std::string& value) {
    CParseResult result;

    std::string  valueName;
    for (auto& c : impl->categories) {
        valueName += c + ':';
    }

    valueName += command;

    if (valueName.contains('[') && valueName.contains(']')) {
        const auto L = valueName.find_first_of('[');
        const auto R = valueName.find_last_of(']');

        if (L < R) {
            const auto CATKEY       = valueName.substr(L + 1, R - L - 1);
            impl->currentSpecialKey = CATKEY;

            valueName = valueName.substr(0, L) + valueName.substr(R + 1);

            for (auto& sc : impl->specialCategoryDescriptors) {
                if (sc->key.empty() || !valueName.starts_with(sc->name + ":"))
                    continue;

                bool keyExists = false;
                for (const auto& specialCat : impl->specialCategories) {
                    if (specialCat->key != sc->key || specialCat->name != sc->name)
                        continue;

                    if (CATKEY != std::string_view{std::any_cast<const char*>(specialCat->values[sc->key].getValue())})
                        continue;

                    // existing special
                    keyExists = true;
                }

                if (keyExists)
                    break;

                // if it doesn't exist, make it
                const auto PCAT  = impl->specialCategories.emplace_back(std::make_unique<SSpecialCategory>()).get();
                PCAT->descriptor = sc.get();
                PCAT->name       = sc->name;
                PCAT->key        = sc->key;
                addSpecialConfigValue(sc->name.c_str(), sc->key.c_str(), CConfigValue(CATKEY.c_str()));

                applyDefaultsToCat(*PCAT);

                PCAT->values[sc->key].setFrom(CATKEY);
            }
        }
    }

    auto VALUEIT = impl->values.find(valueName);
    if (VALUEIT == impl->values.end()) {
        // it might be in a special category
        bool found = false;

        if (impl->currentSpecialCategory && valueName.starts_with(impl->currentSpecialCategory->name)) {
            VALUEIT = impl->currentSpecialCategory->values.find(valueName.substr(impl->currentSpecialCategory->name.length() + 1));

            if (VALUEIT != impl->currentSpecialCategory->values.end())
                found = true;
        }

        if (!found) {
            for (auto& sc : impl->specialCategories) {
                if (!valueName.starts_with(sc->name))
                    continue;

                if (!sc->isStatic && std::string{std::any_cast<const char*>(sc->values[sc->key].getValue())} != impl->currentSpecialKey)
                    continue;

                VALUEIT                      = sc->values.find(valueName.substr(sc->name.length() + 1));
                impl->currentSpecialCategory = sc.get();

                if (VALUEIT != sc->values.end())
                    found = true;
                else if (sc->descriptor->dontErrorOnMissing)
                    return {true, result}; // will return a success, cuz we want to ignore missing

                break;
            }
        }

        if (!found) {
            // could be a dynamic category that doesnt exist yet
            for (auto& sc : impl->specialCategoryDescriptors) {
                if (sc->key.empty() || !valueName.starts_with(sc->name + ":"))
                    continue;

                // found value root to be a special category, get the trunk
                const auto VALUETRUNK = valueName.substr(sc->name.length() + 1);

                // check if trunk is a value within the special category
                if (!sc->defaultValues.contains(VALUETRUNK) && VALUETRUNK != sc->key)
                    break;

                // bingo
                const auto PCAT  = impl->specialCategories.emplace_back(std::make_unique<SSpecialCategory>()).get();
                PCAT->descriptor = sc.get();
                PCAT->name       = sc->name;
                PCAT->key        = sc->key;
                addSpecialConfigValue(sc->name.c_str(), sc->key.c_str(), CConfigValue("0"));

                applyDefaultsToCat(*PCAT);

                VALUEIT                      = PCAT->values.find(valueName.substr(sc->name.length() + 1));
                impl->currentSpecialCategory = PCAT;

                if (VALUEIT != PCAT->values.end())
                    found = true;

                if (sc->anonymous) {
                    // find suitable key
                    size_t biggest = 0;
                    for (auto& catt : impl->specialCategories) {
                        biggest = std::max(catt->anonymousID, biggest);
                    }

                    biggest++;

                    PCAT->values[ANONYMOUS_KEY].setFrom(std::to_string(biggest));
                    impl->currentSpecialKey = std::to_string(biggest);
                    PCAT->anonymousID       = biggest;
                } else {
                    if (VALUEIT == PCAT->values.end() || VALUEIT->first != sc->key) {
                        result.setError(std::format("special category's first value must be the key. Key for <{}> is <{}>", PCAT->name, PCAT->key));
                        return {true, result};
                    }
                    impl->currentSpecialKey = value;
                }

                break;
            }
        }

        if (!found) {
            result.setError(std::format("config option <{}> does not exist.", valueName));
            return {false, result};
        }
    }

    switch (VALUEIT->second.m_eType) {
        case CConfigValue::eDataType::CONFIGDATATYPE_INT: {

            const auto INT = configStringToInt(value);
            if (!INT.has_value()) {
                result.setError(INT.error());
                return {true, result};
            }

            VALUEIT->second.setFrom(INT.value());

            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_FLOAT: {
            try {
                VALUEIT->second.setFrom(std::stof(value));
            } catch (std::exception& e) {
                result.setError(std::format("failed parsing a float: {}", e.what()));
                return {true, result};
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

                VALUEIT->second.setFrom(SVector2D{.x = std::stof(LHS), .y = std::stof(RHS)});
            } catch (std::exception& e) {
                result.setError(std::format("failed parsing a vec2: {}", e.what()));
                return {true, result};
            }
            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_STR: {
            VALUEIT->second.setFrom(value);
            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_CUSTOM: {
            auto RESULT = reinterpret_cast<CConfigCustomValueType*>(VALUEIT->second.m_pData)
                              ->handler(value.c_str(), &reinterpret_cast<CConfigCustomValueType*>(VALUEIT->second.m_pData)->data);
            reinterpret_cast<CConfigCustomValueType*>(VALUEIT->second.m_pData)->lastVal = value;

            if (RESULT.error) {
                result.setError(RESULT.getError());
                return {true, result};
            }
            break;
        }
        default: {
            result.setError("internal error: invalid value found (no type?)");
            return {true, result};
        }
    }

    VALUEIT->second.m_bSetByUser = true;

    return {true, result};
}

CParseResult CConfig::parseVariable(const std::string& lhs, const std::string& rhs, bool dynamic) {
    auto IT = std::ranges::find_if(impl->variables, [&](const auto& v) { return v.name == lhs.substr(1); });

    if (IT != impl->variables.end())
        IT->value = rhs;
    else {
        impl->variables.push_back({lhs.substr(1), rhs});
        std::ranges::sort(impl->variables, [](const auto& lhs, const auto& rhs) { return lhs.name.length() > rhs.name.length(); });
        IT = std::ranges::find_if(impl->variables, [&](const auto& v) { return v.name == lhs.substr(1); });
    }

    if (dynamic) {
        for (auto& l : IT->linesContainingVar) {
            impl->categories             = l.categories;
            impl->currentSpecialCategory = l.specialCategory;
            parseLine(l.line, true);
        }

        impl->categories = {};
    }

    CParseResult result;
    return result;
}

SVariable* CConfigImpl::getVariable(const std::string& name) {
    for (auto& v : envVariables) {
        if (v.name == name)
            return &v;
    }

    for (auto& v : variables) {
        if (v.name == name)
            return &v;
    }

    return nullptr;
}

std::optional<std::string> CConfigImpl::parseComment(const std::string& comment) {
    const auto COMMENT = trim(comment);

    if (!COMMENT.starts_with("hyprlang"))
        return std::nullopt;

    CConstVarList args(COMMENT, 0, 's', true);

    bool          negated         = false;
    std::string   ifBlockVariable = "";

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "noerror") {
            if (negated)
                currentFlags.noError = false;
            else
                currentFlags.noError = args[2] == "true" || args[2] == "yes" || args[2] == "enable" || args[2] == "enabled" || args[2] == "set" || args[2].empty();
            break;
        }

        if (args[i] == "endif") {
            if (currentFlags.ifDatas.empty())
                return "stray endif";
            currentFlags.ifDatas.pop_back();
            break;
        }

        if (args[i] == "if") {
            ifBlockVariable = args[++i];
            break;
        }
    }

    if (!ifBlockVariable.empty()) {
        if (ifBlockVariable.starts_with("!")) {
            negated         = true;
            ifBlockVariable = ifBlockVariable.substr(1);
        }

        CConfigImpl::SIfBlockData newIfData;

        if (const auto VAR = getVariable(ifBlockVariable); VAR)
            newIfData.failed = negated ? VAR->truthy() : !VAR->truthy();
        else
            newIfData.failed = !negated;

        currentFlags.ifDatas.emplace_back(newIfData);
    }

    return std::nullopt;
}

std::expected<float, std::string> CConfigImpl::parseExpression(const std::string& s) {
    // for now, we only support very basic expressions.
    // + - * / and only one per $()
    // TODO: something better

    if (s.empty())
        return std::unexpected("Expression is empty");

    CConstVarList args(s, 0, 's', true);

    if (args[1] != "+" && args[1] != "-" && args[1] != "*" && args[1] != "/")
        return std::unexpected("Invalid expression type: supported +, -, *, /");

    auto  LHS_VAR = std::ranges::find_if(variables, [&](const auto& v) { return v.name == args[0]; });
    auto  RHS_VAR = std::ranges::find_if(variables, [&](const auto& v) { return v.name == args[2]; });

    float left  = 0;
    float right = 0;

    if (LHS_VAR != variables.end()) {
        try {
            left = std::stof(LHS_VAR->value);
        } catch (...) { return std::unexpected("Failed to parse expression: value 1 holds a variable that does not look like a number"); }
    } else {
        try {
            left = std::stof(std::string{args[0]});
        } catch (...) { return std::unexpected("Failed to parse expression: value 1 does not look like a number or the variable doesn't exist"); }
    }

    if (RHS_VAR != variables.end()) {
        try {
            right = std::stof(RHS_VAR->value);
        } catch (...) { return std::unexpected("Failed to parse expression: value 1 holds a variable that does not look like a number"); }
    } else {
        try {
            right = std::stof(std::string{args[2]});
        } catch (...) { return std::unexpected("Failed to parse expression: value 1 does not look like a number or the variable doesn't exist"); }
    }

    switch (args[1][0]) {
        case '+': return left + right;
        case '-': return left - right;
        case '*': return left * right;
        case '/': return left / right;
        default: break;
    }

    return std::unexpected("Unknown error while parsing expression");
}

CParseResult CConfig::parseLine(std::string line, bool dynamic) {
    CParseResult result;

    line = trim(line);

    auto commentPos = line.find('#');

    if (commentPos == 0) {
        const auto COMMENT_RESULT = impl->parseComment(line.substr(1));
        if (COMMENT_RESULT.has_value())
            result.setError(*COMMENT_RESULT);
        return result;
    }

    if (!impl->currentFlags.ifDatas.empty() && impl->currentFlags.ifDatas.back().failed)
        return result;

    size_t lastHashPos = 0;

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
            line       = line.substr(0, commentPos + 1) + line.substr(commentPos + 2);
            commentPos = line.find('#', lastHashPos);
        }
    }

    line = trim(line);

    if (line.empty())
        return result;

    auto equalsPos = line.find('=');

    if (equalsPos == std::string::npos && !line.ends_with("{") && line != "}") {
        // invalid line
        result.setError("Invalid config line");
        return result;
    }

    if (equalsPos != std::string::npos) {
        // set value or call handler
        CParseResult ret;
        auto         LHS = trim(line.substr(0, equalsPos));
        auto         RHS = trim(line.substr(equalsPos + 1));

        if (LHS.empty()) {
            result.setError("Empty lhs.");
            return result;
        }

        const bool ISVARIABLE = *LHS.begin() == '$';

        // limit unwrapping iterations to 100. if exceeds, raise error
        for (size_t i = 0; i < 100; ++i) {
            bool anyMatch = false;

            // parse variables
            for (auto& var : impl->variables) {
                // don't parse LHS variables if this is a variable...
                const auto LHSIT = ISVARIABLE ? std::string::npos : LHS.find("$" + var.name);
                const auto RHSIT = RHS.find("$" + var.name);

                if (LHSIT != std::string::npos)
                    replaceInString(LHS, "$" + var.name, var.value);
                if (RHSIT != std::string::npos)
                    replaceInString(RHS, "$" + var.name, var.value);

                if (RHSIT == std::string::npos && LHSIT == std::string::npos)
                    continue;
                else if (!dynamic)
                    var.linesContainingVar.push_back({line, impl->categories, impl->currentSpecialCategory});

                anyMatch = true;
            }

            // parse expressions {{somevar + 2}}
            // We only support single expressions for now
            while (RHS.contains("{{")) {
                auto firstUnescaped = RHS.find("{{");
                // Keep searching until non-escaped expression start is found
                while (firstUnescaped > 0) {
                    // Special check to avoid undefined behaviour with std::basic_string::find_last_not_of
                    auto amountSkipped = 0;
                    for (int i = firstUnescaped - 1; i >= 0; i--) {
                        if (RHS.at(i) != '\\')
                            break;
                        amountSkipped++;
                    }
                    // No escape chars, or even escape chars. means they escaped themselves.
                    if (amountSkipped % 2 == 0)
                        break;
                    // Continue searching for next valid expression start.
                    firstUnescaped = RHS.find("{{", firstUnescaped + 1);
                    // Break if the next match is never found
                    if (firstUnescaped == std::string::npos)
                        break;
                }
                // Real match was never found.
                if (firstUnescaped == std::string::npos)
                    break;
                const auto BEGIN_EXPR = firstUnescaped;
                // "}}" doesnt need escaping. Would be invalid expression anyways.
                const auto END_EXPR = RHS.find("}}", BEGIN_EXPR + 2);
                if (END_EXPR != std::string::npos) {
                    // try to parse the expression
                    const auto RESULT = impl->parseExpression(RHS.substr(BEGIN_EXPR + 2, END_EXPR - BEGIN_EXPR - 2));
                    if (!RESULT.has_value()) {
                        result.setError(RESULT.error());
                        return result;
                    }

                    RHS = RHS.substr(0, BEGIN_EXPR) + std::format("{}", RESULT.value()) + RHS.substr(END_EXPR + 2);
                } else
                    break;
            }

            if (!anyMatch)
                break;

            if (i == 99) {
                result.setError("Expanding variables exceeded max iteration limit");
                return result;
            }
        }

        if (ISVARIABLE)
            return parseVariable(LHS, RHS, dynamic);

        // Removing escape chars. -- in the future, maybe map all the chars that can be escaped.
        // Right now only expression parsing has escapeable chars
        const char                ESCAPE_CHAR = '\\';
        const std::array<char, 2> ESCAPE_SET{'{', '}'};
        for (size_t i = 0; RHS.length() != 0 && i < RHS.length() - 1; i++) {
            if (RHS.at(i) != ESCAPE_CHAR)
                continue;
            //if escaping an escape, remove and skip the next char
            if (RHS.at(i + 1) == ESCAPE_CHAR) {
                RHS.erase(i, 1);
                continue;
            }
            //checks if any of the chars were escapable.
            for (const auto& ESCAPABLE_CHAR : ESCAPE_SET) {
                if (RHS.at(i + 1) != ESCAPABLE_CHAR)
                    continue;
                RHS.erase(i--, 1);
                break;
            }
        }

        bool found = false;

        if (!impl->configOptions.verifyOnly) {
            auto [f, rv] = configSetValueSafe(LHS, RHS);
            found        = f;
            ret          = std::move(rv);
        }

        if (!found) {
            for (auto& h : impl->handlers) {
                // we want to handle potentially nested keywords and ensure
                // we only call the handler if they are scoped correctly,
                // unless the keyword is not scoped itself

                const bool UNSCOPED    = !h.name.contains(":");
                const auto HANDLERNAME = !h.name.empty() && h.name.at(0) == ':' ? h.name.substr(1) : h.name;

                if (!h.options.allowFlags && !UNSCOPED) {
                    size_t colon = 0;
                    size_t idx   = 0;
                    size_t depth = 0;

                    while ((colon = HANDLERNAME.find(':', idx)) != std::string::npos && impl->categories.size() > depth) {
                        auto actual = HANDLERNAME.substr(idx, colon - idx);

                        if (actual != impl->categories[depth])
                            break;

                        idx = colon + 1;
                        ++depth;
                    }

                    if (depth != impl->categories.size() || HANDLERNAME.substr(idx) != LHS)
                        continue;
                }

                if (UNSCOPED && HANDLERNAME != LHS && !h.options.allowFlags)
                    continue;

                if (h.options.allowFlags && (!LHS.starts_with(HANDLERNAME) || LHS.contains(':') /* avoid cases where a category is called the same as a handler */))
                    continue;

                ret   = h.func(LHS.c_str(), RHS.c_str());
                found = true;
            }
        }

        if (ret.error)
            return ret;
    } else {
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

            if (impl->categories.empty()) {
                impl->currentSpecialKey      = "";
                impl->currentSpecialCategory = nullptr;
            }
        } else {
            // open a category.
            if (!line.ends_with("{")) {
                result.setError("Invalid category open, garbage after {");
                return result;
            }

            line.pop_back();
            line = trim(line);
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
        impl->values.at(k).defaultFrom(v);
    }
    for (auto& sc : impl->specialCategories) {
        applyDefaultsToCat(*sc);
    }

    CParseResult fileParseResult;

    if (impl->rawConfigString.empty()) {
        bool fileExists = std::filesystem::exists(impl->path);

        // implies options.allowMissingConfig
        if (impl->configOptions.allowMissingConfig && !fileExists)
            return CParseResult{};
        else if (!fileExists) {
            CParseResult res;
            res.setError("Config file is missing");
            return res;
        }

        std::string canonical = std::filesystem::canonical(impl->path);

        fileParseResult = parseFile(canonical.c_str());
    } else {
        fileParseResult = parseRawStream(impl->rawConfigString);
    }

    return fileParseResult;
}

CParseResult CConfig::parseRawStream(const std::string& stream) {
    CParseResult      result;

    int               rawLineNum = 0;
    int               lineNum    = 0;

    std::stringstream str(stream);

    while (true) {
        const auto line = getNextLine(str, rawLineNum, lineNum);

        if (!line) {
            switch (line.error()) {
                case GETNEXTLINEFAILURE_EOF: break;
                case GETNEXTLINEFAILURE_BACKSLASH:
                    if (!impl->parseError.empty())
                        impl->parseError += "\n";
                    impl->parseError += std::format("Config error: Last line ends with backslash");
                    result.setError(impl->parseError);
                    break;
            }
            break;
        }

        const auto RET = parseLine(line.value());

        if (RET.error && (impl->parseError.empty() || impl->configOptions.throwAllErrors)) {
            if (!impl->parseError.empty())
                impl->parseError += "\n";
            impl->parseError += std::format("Config error at line {}: {}", lineNum, RET.errorStdString);
            result.setError(impl->parseError);
        }
    }

    if (!impl->categories.empty()) {
        if (impl->parseError.empty() || impl->configOptions.throwAllErrors) {
            if (!impl->parseError.empty())
                impl->parseError += "\n";
            impl->parseError += std::format("Config error: Unclosed category at EOF");
            result.setError(impl->parseError);
        }

        impl->categories.clear();
    }

    return result;
}

CParseResult CConfig::parseFile(const char* file) {
    CParseResult  result;

    std::ifstream iffile(file);
    if (!iffile.good()) {
        result.setError("File failed to open");
        return result;
    }

    int rawLineNum = 0;
    int lineNum    = 0;

    while (true) {
        const auto line = getNextLine(iffile, rawLineNum, lineNum);

        if (!line) {
            switch (line.error()) {
                case GETNEXTLINEFAILURE_EOF: break;
                case GETNEXTLINEFAILURE_BACKSLASH:
                    if (!impl->parseError.empty())
                        impl->parseError += "\n";
                    impl->parseError += std::format("Config error in file {}: Last line ends with backslash", file);
                    result.setError(impl->parseError);
                    break;
            }
            break;
        }

        const auto RET = parseLine(line.value());

        if (!impl->currentFlags.noError && RET.error && (impl->parseError.empty() || impl->configOptions.throwAllErrors)) {
            if (!impl->parseError.empty())
                impl->parseError += "\n";
            impl->parseError += std::format("Config error in file {} at line {}: {}", file, lineNum, RET.errorStdString);
            result.setError(impl->parseError);
        }
    }

    iffile.close();

    if (!impl->categories.empty()) {
        if (impl->parseError.empty() || impl->configOptions.throwAllErrors) {
            if (!impl->parseError.empty())
                impl->parseError += "\n";
            impl->parseError += std::format("Config error in file {}: Unclosed category at EOF", file);
            result.setError(impl->parseError);
        }

        impl->categories.clear();
    }

    return result;
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
    std::erase_if(impl->specialCategories, [](const auto& e) { return !e->isStatic; });
}

CConfigValue* CConfig::getConfigValuePtr(const char* name) {
    const auto IT = impl->values.find(std::string{name});
    return IT == impl->values.end() ? nullptr : &IT->second;
}

CConfigValue* CConfig::getSpecialConfigValuePtr(const char* category, const char* name, const char* key) {
    const std::string CAT  = category;
    const std::string NAME = name;
    const std::string KEY  = key ? key : "";

    for (auto& sc : impl->specialCategories) {
        if (sc->name != CAT || (!sc->isStatic && std::string{std::any_cast<const char*>(sc->values[sc->key].getValue())} != KEY))
            continue;

        const auto IT = sc->values.find(NAME);
        if (IT == sc->values.end())
            return nullptr;

        return &IT->second;
    }

    return nullptr;
}

void CConfig::registerHandler(PCONFIGHANDLERFUNC func, const char* name, SHandlerOptions options_) {
    SHandlerOptions options;
    std::memcpy(&options, &options_, seekABIStructSize(&options_, 0, sizeof(SHandlerOptions)));
    impl->handlers.push_back(SHandler{.name = name, .options = options, .func = func});
}

void CConfig::unregisterHandler(const char* name) {
    std::erase_if(impl->handlers, [name](const auto& other) { return other.name == name; });
}

bool CConfig::specialCategoryExistsForKey(const char* category, const char* key) {
    for (auto& sc : impl->specialCategories) {
        if (sc->isStatic)
            continue;

        if (sc->name != category || std::string{std::any_cast<const char*>(sc->values[sc->key].getValue())} != key)
            continue;

        return true;
    }

    return false;
}

/* if len != 0, out needs to be freed */
void CConfig::retrieveKeysForCat(const char* category, const char*** out, size_t* len) {
    size_t count = 0;
    for (auto& sc : impl->specialCategories) {
        if (sc->isStatic)
            continue;

        if (sc->name != category)
            continue;

        count++;
    }

    if (count == 0) {
        *len = 0;
        return;
    }

    *out            = (const char**)calloc(1, count * sizeof(const char*));
    size_t counter2 = 0;
    for (auto& sc : impl->specialCategories) {
        if (sc->isStatic)
            continue;

        if (sc->name != category)
            continue;

        // EVIL, but the pointers will be almost instantly discarded by the caller
        (*out)[counter2++] = (const char*)sc->values[sc->key].m_pData;
    }

    *len = count;
}
