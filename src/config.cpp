#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <algorithm>
#include <cmath>
#include <expected>
#include <sstream>
#include <cstring>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/String.hpp>

using namespace Hyprlang;
using namespace Hyprutils::String;

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern "C" char** environ;
#endif

// defines
inline constexpr const char* ANONYMOUS_KEY = "__hyprlang_internal_anonymous_key";
//

static size_t seekABIStructSize(const void* begin, size_t startOffset, size_t maxSize) {
    for (size_t off = startOffset; off < maxSize; off += 4) {
        if (*(int*)((unsigned char*)begin + off) == int{HYPRLANG_END_MAGIC})
            return off;
    }

    return 0;
}

CConfig::CConfig(const char* path, const Hyprlang::SConfigOptions& options_) {
    SConfigOptions options;
    std::memcpy(&options, &options_, seekABIStructSize(&options_, 16, sizeof(SConfigOptions)));

    impl = new CConfigImpl;

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

    std::sort(impl->envVariables.begin(), impl->envVariables.end(), [&](const auto& a, const auto& b) { return a.name.length() > b.name.length(); });

    impl->configOptions = options;
}

CConfig::~CConfig() {
    delete impl;
}

void CConfig::addConfigValue(const char* name, const CConfigValue& value) {
    if (m_bCommenced)
        throw "Cannot addConfigValue after commence()";

    if ((eDataType)value.m_eType != CONFIGDATATYPE_CUSTOM && (eDataType)value.m_eType != CONFIGDATATYPE_STR)
        impl->defaultValues.emplace(name, SConfigDefaultValue{value.getValue(), (eDataType)value.m_eType});
    else if ((eDataType)value.m_eType == CONFIGDATATYPE_STR)
        impl->defaultValues.emplace(name, SConfigDefaultValue{std::string{std::any_cast<const char*>(value.getValue())}, (eDataType)value.m_eType});
    else
        impl->defaultValues.emplace(name,
                                    SConfigDefaultValue{reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->defaultVal, (eDataType)value.m_eType,
                                                        reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->handler,
                                                        reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->dtor});
}

void CConfig::addSpecialConfigValue(const char* cat, const char* name, const CConfigValue& value) {
    const auto IT = std::find_if(impl->specialCategoryDescriptors.begin(), impl->specialCategoryDescriptors.end(), [&](const auto& other) { return other->name == cat; });

    if (IT == impl->specialCategoryDescriptors.end())
        throw "No such category";

    if ((eDataType)value.m_eType != CONFIGDATATYPE_CUSTOM && (eDataType)value.m_eType != CONFIGDATATYPE_STR)
        IT->get()->defaultValues.emplace(name, SConfigDefaultValue{value.getValue(), (eDataType)value.m_eType});
    else if ((eDataType)value.m_eType == CONFIGDATATYPE_STR)
        IT->get()->defaultValues.emplace(name, SConfigDefaultValue{std::string{std::any_cast<const char*>(value.getValue())}, (eDataType)value.m_eType});
    else
        IT->get()->defaultValues.emplace(name,
                                         SConfigDefaultValue{reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->defaultVal, (eDataType)value.m_eType,
                                                             reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->handler,
                                                             reinterpret_cast<CConfigCustomValueType*>(value.m_pData)->dtor});

    const auto CAT = std::find_if(impl->specialCategories.begin(), impl->specialCategories.end(), [cat, name](const auto& other) { return other->name == cat && other->isStatic; });

    if (CAT != impl->specialCategories.end())
        CAT->get()->values[name].defaultFrom(IT->get()->defaultValues[name]);
}

void CConfig::removeSpecialConfigValue(const char* cat, const char* name) {
    const auto IT = std::find_if(impl->specialCategoryDescriptors.begin(), impl->specialCategoryDescriptors.end(), [&](const auto& other) { return other->name == cat; });

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
    std::sort(impl->specialCategories.begin(), impl->specialCategories.end(), [](const auto& a, const auto& b) -> int { return a->name.length() > b->name.length(); });
    std::sort(impl->specialCategoryDescriptors.begin(), impl->specialCategoryDescriptors.end(),
              [](const auto& a, const auto& b) -> int { return a->name.length() > b->name.length(); });
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
    if (VALUE.starts_with("0x")) {
        // Values with 0x are hex
        const auto VALUEWITHOUTHEX = VALUE.substr(2);
        return stoll(VALUEWITHOUTHEX, nullptr, 16);
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

            return a * (Hyprlang::INT)0x1000000 + r.value() * (Hyprlang::INT)0x10000 + g.value() * (Hyprlang::INT)0x100 + b.value();
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = std::stoll(VALUEWITHOUTFUNC, nullptr, 16);

            // now we need to RGBA -> ARGB. The config holds ARGB only.
            return (RGBA >> 8) + 0x1000000 * (RGBA & 0xFF);
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

            return (Hyprlang::INT)0xFF000000 + r.value() * (Hyprlang::INT)0x10000 + g.value() * (Hyprlang::INT)0x100 + b.value();
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            const auto RGB = std::stoll(VALUEWITHOUTFUNC, nullptr, 16);

            return RGB + 0xFF000000;
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

CParseResult CConfig::configSetValueSafe(const std::string& command, const std::string& value) {
    CParseResult result;

    std::string  valueName;
    std::string  catPrefix;
    for (auto& c : impl->categories) {
        valueName += c + ':';
        catPrefix += c + ':';
    }

    valueName += command;

    const auto VALUEONLYNAME = command.starts_with(catPrefix) ? command.substr(catPrefix.length()) : command;

    // FIXME: this will bug with nested.
    if (valueName.contains('[') && valueName.contains(']')) {
        const auto L = valueName.find_first_of('[');
        const auto R = valueName.find_last_of(']');

        if (L < R) {
            const auto CATKEY       = valueName.substr(L + 1, R - L - 1);
            impl->currentSpecialKey = CATKEY;

            valueName = valueName.substr(0, L) + valueName.substr(R + 1);

            // if it doesn't exist, make it
            for (auto& sc : impl->specialCategoryDescriptors) {
                if (sc->key.empty() || !valueName.starts_with(sc->name))
                    continue;

                // bingo
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
                    return result; // will return a success, cuz we want to ignore missing

                break;
            }
        }

        if (!found) {
            // could be a dynamic category that doesnt exist yet
            for (auto& sc : impl->specialCategoryDescriptors) {
                if (sc->key.empty() || !valueName.starts_with(sc->name))
                    continue;

                // category does exist, check if value exists
                if (!sc->defaultValues.contains(VALUEONLYNAME) && VALUEONLYNAME != sc->key)
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
                        if (catt->anonymousID > biggest)
                            biggest = catt->anonymousID;
                    }

                    biggest++;

                    PCAT->values[ANONYMOUS_KEY].setFrom(std::to_string(biggest));
                    impl->currentSpecialKey = std::to_string(biggest);
                    PCAT->anonymousID       = biggest;
                } else {
                    if (VALUEIT == PCAT->values.end() || VALUEIT->first != sc->key) {
                        result.setError(std::format("special category's first value must be the key. Key for <{}> is <{}>", PCAT->name, PCAT->key));
                        return result;
                    }
                    impl->currentSpecialKey = value;
                }

                break;
            }
        }

        if (!found) {
            result.setError(std::format("config option <{}> does not exist.", valueName));
            return result;
        }
    }

    switch (VALUEIT->second.m_eType) {
        case CConfigValue::eDataType::CONFIGDATATYPE_INT: {

            const auto INT = configStringToInt(value);
            if (!INT.has_value()) {
                result.setError(INT.error());
                return result;
            }

            VALUEIT->second.setFrom(INT.value());

            break;
        }
        case CConfigValue::eDataType::CONFIGDATATYPE_FLOAT: {
            try {
                VALUEIT->second.setFrom(std::stof(value));
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

                VALUEIT->second.setFrom(SVector2D{std::stof(LHS), std::stof(RHS)});
            } catch (std::exception& e) {
                result.setError(std::format("failed parsing a vec2: {}", e.what()));
                return result;
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
                return result;
            }
            break;
        }
        default: {
            result.setError("internal error: invalid value found (no type?)");
            return result;
        }
    }

    VALUEIT->second.m_bSetByUser = true;

    return result;
}

CParseResult CConfig::parseVariable(const std::string& lhs, const std::string& rhs, bool dynamic) {
    auto IT = std::find_if(impl->variables.begin(), impl->variables.end(), [&](const auto& v) { return v.name == lhs.substr(1); });

    if (IT != impl->variables.end())
        IT->value = rhs;
    else {
        impl->variables.push_back({lhs.substr(1), rhs});
        std::sort(impl->variables.begin(), impl->variables.end(), [](const auto& lhs, const auto& rhs) { return lhs.name.length() > rhs.name.length(); });
        IT = std::find_if(impl->variables.begin(), impl->variables.end(), [&](const auto& v) { return v.name == lhs.substr(1); });
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

void CConfigImpl::parseComment(const std::string& comment) {
    const auto COMMENT = trim(comment);

    if (!COMMENT.starts_with("hyprlang"))
        return;

    CVarList args(COMMENT, 0, 's', true);

    if (args[1] == "noerror")
        currentFlags.noError = args[2] == "true" || args[2] == "yes" || args[2] == "enable" || args[2] == "enabled" || args[2] == "set";
}

CParseResult CConfig::parseLine(std::string line, bool dynamic) {
    CParseResult result;

    line = trim(line);

    auto commentPos = line.find('#');

    if (commentPos == 0) {
        impl->parseComment(line.substr(1));
        return result;
    }

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
                else
                    var.linesContainingVar.push_back({line, impl->categories, impl->currentSpecialCategory});

                anyMatch = true;
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

        bool found = false;
        for (auto& h : impl->handlers) {
            if (!h.options.allowFlags && h.name != LHS)
                continue;

            if (h.options.allowFlags && (!LHS.starts_with(h.name) || LHS.contains(':') /* avoid cases where a category is called the same as a handler */))
                continue;

            ret   = h.func(LHS.c_str(), RHS.c_str());
            found = true;
        }

        if (!found && !impl->configOptions.verifyOnly)
            ret = configSetValueSafe(LHS, RHS);

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

            impl->currentSpecialKey      = "";
            impl->currentSpecialCategory = nullptr;
            impl->categories.pop_back();
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

    std::string       line    = "";
    int               linenum = 1;

    std::stringstream str(stream);

    while (std::getline(str, line)) {
        const auto RET = parseLine(line);

        if (RET.error && (impl->parseError.empty() || impl->configOptions.throwAllErrors)) {
            if (!impl->parseError.empty())
                impl->parseError += "\n";
            impl->parseError += std::format("Config error at line {}: {}", linenum, RET.errorStdString);
            result.setError(impl->parseError);
        }

        ++linenum;
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

    std::string line    = "";
    int         linenum = 1;

    while (std::getline(iffile, line)) {

        const auto RET = parseLine(line);

        if (!impl->currentFlags.noError && RET.error && (impl->parseError.empty() || impl->configOptions.throwAllErrors)) {
            if (!impl->parseError.empty())
                impl->parseError += "\n";
            impl->parseError += std::format("Config error in file {} at line {}: {}", file, linenum, RET.errorStdString);
            result.setError(impl->parseError);
        }

        ++linenum;
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
    impl->handlers.push_back(SHandler{name, options, func});
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
