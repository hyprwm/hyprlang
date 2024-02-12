#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <algorithm>
#include <cmath>
#include <expected>

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

CConfig::CConfig(const char* path, const Hyprlang::SConfigOptions& options) {
    impl       = new CConfigImpl;
    impl->path = path;

    if (!std::filesystem::exists(impl->path)) {
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

void CConfig::addSpecialConfigValue(const char* cat, const char* name, const CConfigValue value) {
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
}

void CConfig::removeSpecialConfigValue(const char* cat, const char* name) {
    const auto IT = std::find_if(impl->specialCategoryDescriptors.begin(), impl->specialCategoryDescriptors.end(), [&](const auto& other) { return other->name == cat; });

    if (IT == impl->specialCategoryDescriptors.end())
        throw "No such category";

    std::erase_if(IT->get()->defaultValues, [name](const auto& other) { return other.first == name; });
}

void CConfig::addSpecialCategory(const char* name, SSpecialCategoryOptions options) {
    const auto PDESC          = impl->specialCategoryDescriptors.emplace_back(std::make_unique<SSpecialCategoryDescriptor>()).get();
    PDESC->name               = name;
    PDESC->key                = options.key ? options.key : "";
    PDESC->dontErrorOnMissing = options.ignoreMissing;

    if (!options.key) {
        const auto PCAT  = impl->specialCategories.emplace_back(std::make_unique<SSpecialCategory>()).get();
        PCAT->descriptor = PDESC;
        PCAT->name       = name;
        PCAT->key        = options.key ? options.key : "";
        PCAT->isStatic   = true;
        if (!PCAT->key.empty())
            addSpecialConfigValue(name, options.key, CConfigValue("0"));
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

static std::expected<int64_t, std::string> configStringToInt(const std::string& VALUE) {
    if (VALUE.starts_with("0x")) {
        // Values with 0x are hex
        const auto VALUEWITHOUTHEX = VALUE.substr(2);
        return stoll(VALUEWITHOUTHEX, nullptr, 16);
    } else if (VALUE.starts_with("rgba(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = removeBeginEndSpacesTabs(VALUE.substr(5, VALUE.length() - 6));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 3) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t a           = 0;
            try {
                a = std::round(std::stof(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(',')))) * 255.f);
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
        const auto VALUEWITHOUTFUNC = removeBeginEndSpacesTabs(VALUE.substr(4, VALUE.length() - 5));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 2) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(removeBeginEndSpacesTabs(rolling.substr(0, rolling.find(','))));

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
    for (auto& c : impl->categories) {
        valueName += c + ':';
    }

    valueName += command;

    auto VALUEIT = impl->values.find(valueName);
    if (VALUEIT == impl->values.end()) {
        // it might be in a special category
        bool found = false;
        for (auto& sc : impl->specialCategories) {
            if (!valueName.starts_with(sc->name))
                continue;

            if (!sc->isStatic && std::string{std::any_cast<const char*>(sc->values[sc->key].getValue())} != impl->currentSpecialKey)
                continue;

            VALUEIT = sc->values.find(valueName.substr(sc->name.length() + 1));

            if (VALUEIT != sc->values.end())
                found = true;
            else if (sc->descriptor->dontErrorOnMissing)
                return result; // will return a success, cuz we want to ignore missing

            break;
        }

        if (!found) {
            // could be a dynamic category that doesnt exist yet
            for (auto& sc : impl->specialCategoryDescriptors) {
                if (sc->key.empty() || !valueName.starts_with(sc->name))
                    continue;

                // bingo
                const auto PCAT  = impl->specialCategories.emplace_back(std::make_unique<SSpecialCategory>()).get();
                PCAT->descriptor = sc.get();
                PCAT->name       = sc->name;
                PCAT->key        = sc->key;
                addSpecialConfigValue(sc->name.c_str(), sc->key.c_str(), CConfigValue("0"));

                applyDefaultsToCat(*PCAT);

                VALUEIT = PCAT->values.find(valueName.substr(sc->name.length() + 1));

                if (VALUEIT != PCAT->values.end())
                    found = true;

                if (VALUEIT == PCAT->values.end() || VALUEIT->first != sc->key) {
                    result.setError(std::format("special category's first value must be the key. Key for <{}> is <{}>", PCAT->name, PCAT->key));
                    return result;
                }

                impl->currentSpecialKey = value;

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
            reinterpret_cast<CConfigCustomValueType*>(VALUEIT->second.m_pData)->handler(value.c_str(), &reinterpret_cast<CConfigCustomValueType*>(VALUEIT->second.m_pData)->data);
            reinterpret_cast<CConfigCustomValueType*>(VALUEIT->second.m_pData)->lastVal = value;
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
            line       = line.substr(0, commentPos + 1) + line.substr(commentPos + 2);
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

        if (!found && !impl->configOptions.verifyOnly)
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

            impl->currentSpecialKey = "";
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
        impl->values.at(k).defaultFrom(v);
    }
    for (auto& sc : impl->specialCategories) {
        applyDefaultsToCat(*sc);
    }

    bool fileExists = std::filesystem::exists(impl->path);

    // implies options.allowMissingConfig
    if (impl->configOptions.allowMissingConfig && !fileExists)
        return CParseResult{};
    else if (!fileExists) {
        CParseResult res;
        res.setError("Config file is missing");
        return res;
    }

    std::string  canonical = std::filesystem::canonical(impl->path);

    CParseResult fileParseResult = parseFile(canonical.c_str());

    return fileParseResult;
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

        if (RET.error && (impl->parseError.empty() || impl->configOptions.throwAllErrors)) {
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

void CConfig::registerHandler(PCONFIGHANDLERFUNC func, const char* name, SHandlerOptions options) {
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
