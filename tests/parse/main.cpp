#include <iostream>
#include <filesystem>
#include <algorithm>

#include <hyprlang.hpp>

namespace Colors {
    constexpr const char* RED     = "\x1b[31m";
    constexpr const char* GREEN   = "\x1b[32m";
    constexpr const char* YELLOW  = "\x1b[33m";
    constexpr const char* BLUE    = "\x1b[34m";
    constexpr const char* MAGENTA = "\x1b[35m";
    constexpr const char* CYAN    = "\x1b[36m";
    constexpr const char* RESET   = "\x1b[0m";
};

#define EXPECT(expr, val)                                                                                                                                                          \
    if (const auto RESULT = expr; RESULT != (val)) {                                                                                                                               \
        std::cout << Colors::RED << "Failed: " << Colors::RESET << #expr << ", expected " << #val << " but got " << RESULT << "\n";                                                \
        ret = 1;                                                                                                                                                                   \
    } else {                                                                                                                                                                       \
        std::cout << Colors::GREEN << "Passed " << Colors::RESET << #expr << ". Got " << val << "\n";                                                                              \
    }

// globals for testing
bool                            barrelRoll    = false;
std::string                     flagsFound    = "";
Hyprlang::CConfig*              pConfig       = nullptr;
std::string                     currentPath   = "";
std::string                     ignoreKeyword = "";
std::string                     useKeyword    = "";
static std::vector<std::string> categoryKeywordActualValues;

static Hyprlang::CParseResult   handleDoABarrelRoll(const char* COMMAND, const char* VALUE) {
    if (std::string(VALUE) == "woohoo, some, params")
        barrelRoll = true;

    Hyprlang::CParseResult result;
    return result;
}

static Hyprlang::CParseResult handleFlagsTest(const char* COMMAND, const char* VALUE) {
    std::string cmd = COMMAND;
    flagsFound      = cmd.substr(5);

    Hyprlang::CParseResult result;
    return result;
}

static Hyprlang::CParseResult handleCategoryKeyword(const char* COMMAND, const char* VALUE) {
    categoryKeywordActualValues.emplace_back(VALUE);

    return Hyprlang::CParseResult();
}

static Hyprlang::CParseResult handleTestIgnoreKeyword(const char* COMMAND, const char* VALUE) {
    ignoreKeyword = VALUE;

    return Hyprlang::CParseResult();
}

static Hyprlang::CParseResult handleTestUseKeyword(const char* COMMAND, const char* VALUE) {
    useKeyword = VALUE;

    return Hyprlang::CParseResult();
}

static Hyprlang::CParseResult handleNoop(const char* COMMAND, const char* VALUE) {
    return Hyprlang::CParseResult();
}

static Hyprlang::CParseResult handleSource(const char* COMMAND, const char* VALUE) {
    std::string PATH = std::filesystem::canonical(currentPath + "/" + VALUE);
    return pConfig->parseFile(PATH.c_str());
}

static Hyprlang::CParseResult handleCustomValueSet(const char* VALUE, void** data) {
    if (!*data)
        *data = calloc(1, sizeof(int64_t));
    std::string V = VALUE;
    if (V == "abc")
        *reinterpret_cast<int64_t*>(*data) = 1;
    else
        *reinterpret_cast<int64_t*>(*data) = 2;

    Hyprlang::CParseResult result;
    return result;
}

static void handleCustomValueDestroy(void** data) {
    if (*data)
        free(*data);
}

int main(int argc, char** argv, char** envp) {
    int ret = 0;

    try {
        if (!getenv("SHELL"))
            setenv("SHELL", "/bin/sh", true);

        std::cout << "Starting test\n";

        Hyprlang::CConfig config("./config/config.conf", {});
        pConfig     = &config;
        currentPath = std::filesystem::canonical("./config/");

        // setup config
        config.addConfigValue("testInt", (Hyprlang::INT)0);
        config.addConfigValue("testExpr", (Hyprlang::INT)0);
        config.addConfigValue("testEscapedExpr", "");
        config.addConfigValue("testEscapedExpr2", "");
        config.addConfigValue("testEscapedExpr3", "");
        config.addConfigValue("testEscapedEscape", "");
        config.addConfigValue("testMixedEscapedExpression", "");
        config.addConfigValue("testMixedEscapedExpression2", "");
        config.addConfigValue("testImbeddedEscapedExpression", "");
        config.addConfigValue("testDynamicEscapedExpression", "");
        config.addConfigValue("testFloat", 0.F);
        config.addConfigValue("testVec", Hyprlang::SVector2D{.x = 69, .y = 420});
        config.addConfigValue("testString", "");
        config.addConfigValue("testStringColon", "");
        config.addConfigValue("testEnv", "");
        config.addConfigValue("testVar", (Hyprlang::INT)0);
        config.addConfigValue("categoryKeyword", (Hyprlang::STRING) "");
        config.addConfigValue("testStringQuotes", "");
        config.addConfigValue("testStringRecursive", "");
        config.addConfigValue("testCategory:testValueInt", (Hyprlang::INT)0);
        config.addConfigValue("testCategory:testValueHex", (Hyprlang::INT)0xA);
        config.addConfigValue("testCategory:nested1:testValueNest", (Hyprlang::INT)0);
        config.addConfigValue("testCategory:nested1:nested2:testValueNest", (Hyprlang::INT)0);
        config.addConfigValue("testCategory:nested1:categoryKeyword", (Hyprlang::STRING) "");
        config.addConfigValue("testDefault", (Hyprlang::INT)123);
        config.addConfigValue("testCategory:testColor1", (Hyprlang::INT)0);
        config.addConfigValue("testCategory:testColor2", (Hyprlang::INT)0);
        config.addConfigValue("testCategory:testColor3", (Hyprlang::INT)0);
        config.addConfigValue("flagsStuff:value", (Hyprlang::INT)0);
        config.addConfigValue("myColors:pink", (Hyprlang::INT)0);
        config.addConfigValue("myColors:green", (Hyprlang::INT)0);
        config.addConfigValue("myColors:random", (Hyprlang::INT)0);
        config.addConfigValue("customType", {Hyprlang::CConfigCustomValueType{&handleCustomValueSet, &handleCustomValueDestroy, "def"}});

        config.registerHandler(&handleDoABarrelRoll, "doABarrelRoll", {.allowFlags = false});
        config.registerHandler(&handleFlagsTest, "flags", {.allowFlags = true});
        config.registerHandler(&handleSource, "source", {.allowFlags = false});
        config.registerHandler(&handleTestIgnoreKeyword, "testIgnoreKeyword", {.allowFlags = false});
        config.registerHandler(&handleTestUseKeyword, ":testUseKeyword", {.allowFlags = false});
        config.registerHandler(&handleNoop, "testCategory:testUseKeyword", {.allowFlags = false});
        config.registerHandler(&handleCategoryKeyword, "testCategory:categoryKeyword", {.allowFlags = false});

        config.addSpecialCategory("special", {.key = "key"});
        config.addSpecialConfigValue("special", "value", (Hyprlang::INT)0);

        config.addSpecialCategory("specialAnonymous", {.key = nullptr, .ignoreMissing = false, .anonymousKeyBased = true});
        config.addSpecialConfigValue("specialAnonymous", "value", (Hyprlang::INT)0);

        config.addSpecialCategory("specialAnonymousNested", {.key = nullptr, .ignoreMissing = false, .anonymousKeyBased = true});
        config.addSpecialConfigValue("specialAnonymousNested", "nested:value1", (Hyprlang::INT)0);
        config.addSpecialConfigValue("specialAnonymousNested", "nested:value2", (Hyprlang::INT)0);
        config.addSpecialConfigValue("specialAnonymousNested", "nested1:nested2:value1", (Hyprlang::INT)0);
        config.addSpecialConfigValue("specialAnonymousNested", "nested1:nested2:value2", (Hyprlang::INT)0);

        config.addConfigValue("multiline", "");

        config.commence();

        config.addSpecialCategory("specialGeneric:one", {.key = nullptr, .ignoreMissing = true});
        config.addSpecialConfigValue("specialGeneric:one", "value", (Hyprlang::INT)0);
        config.addSpecialCategory("specialGeneric:two", {.key = nullptr, .ignoreMissing = true});
        config.addSpecialConfigValue("specialGeneric:two", "value", (Hyprlang::INT)0);

        const Hyprlang::CConfigValue copyTest = {(Hyprlang::INT)1};
        config.addSpecialConfigValue("specialGeneric:one", "copyTest", copyTest);

        const auto PARSERESULT = config.parse();
        if (PARSERESULT.error) {
            std::cout << "Parse error: " << PARSERESULT.getError() << "\n";
            return 1;
        }

        EXPECT(PARSERESULT.error, false);

        // test values
        std::cout << " → Testing values\n";
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testInt")), 123);
        EXPECT(std::any_cast<float>(config.getConfigValue("testFloat")), 123.456f);
        auto EXP = Hyprlang::SVector2D{.x = 69, .y = 420};
        EXPECT(std::any_cast<Hyprlang::SVector2D>(config.getConfigValue("testVec")), EXP);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testString")), std::string{"Hello World! # This is not a comment!"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringQuotes")), std::string{"\"Hello World!\""});
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueInt")), (Hyprlang::INT)123456);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueHex")), (Hyprlang::INT)0xFFFFAABB);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:nested1:testValueNest")), (Hyprlang::INT)1);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:nested1:nested2:testValueNest")), (Hyprlang::INT)1);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testDefault")), (Hyprlang::INT)123);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testColor1")), (Hyprlang::INT)0xFFFFFFFF);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testColor2")), (Hyprlang::INT)0xFF000000);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testColor3")), (Hyprlang::INT)0x22ffeeff);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringColon")), std::string{"ee:ee:ee"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("categoryKeyword")), std::string{"oops, this one shouldn't call the handler, not fun"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testCategory:nested1:categoryKeyword")), std::string{"this one should not either"});
        EXPECT(ignoreKeyword, "aaa");
        EXPECT(useKeyword, "yes");

        // Test templated wrapper
        auto T1 = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(&config, "testInt");
        auto T2 = Hyprlang::CSimpleConfigValue<Hyprlang::FLOAT>(&config, "testFloat");
        auto T3 = Hyprlang::CSimpleConfigValue<Hyprlang::SVector2D>(&config, "testVec");
        auto T4 = Hyprlang::CSimpleConfigValue<std::string>(&config, "testString");
        EXPECT(*T1, 123);
        EXPECT(*T2, 123.456F);
        EXPECT(*T3, EXP);
        EXPECT(*T4, "Hello World! # This is not a comment!");

        // test expressions
        std::cout << " → Testing expressions\n";
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testExpr")), 1335);

        // test expression escape
        std::cout << " → Testing expression escapes\n";
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testEscapedExpr")), std::string{"{{testInt + 7}}"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testEscapedExpr2")), std::string{"{{testInt + 7}}"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testEscapedExpr3")), std::string{"{{3 + 8}}"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testEscapedEscape")), std::string{"\\5"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testMixedEscapedExpression")), std::string{"-2 {{ {{50 + 50}} / {{10 * 5}} }}"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testMixedEscapedExpression2")), std::string{"{{8\\13}} should equal \"{{8\\13}}\""});

        EXPECT(std::any_cast<const char*>(config.getConfigValue("testImbeddedEscapedExpression")), std::string{"{{10 + 10}}"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testDynamicEscapedExpression")), std::string{"{{ moved: 500 expr: {{1000 / 2}} }}"});

        // test static values
        std::cout << " → Testing static values\n";
        static auto* const PTESTINT = config.getConfigValuePtr("testInt")->getDataStaticPtr();
        EXPECT(*reinterpret_cast<int64_t*>(*PTESTINT), 123);
        config.parse();
        EXPECT(*reinterpret_cast<int64_t*>(*PTESTINT), 123);

        // test handlers
        std::cout << " → Testing handlers\n";
        EXPECT(barrelRoll, true);
        EXPECT(flagsFound, std::string{"abc"});

        EXPECT(categoryKeywordActualValues.at(0), "we are having fun");
        EXPECT(categoryKeywordActualValues.at(1), "so much fun");
        EXPECT(categoryKeywordActualValues.at(2), "im the fun one at parties");

        // test dynamic
        std::cout << " → Testing dynamic\n";
        barrelRoll = false;
        EXPECT(config.parseDynamic("doABarrelRoll = woohoo, some, params").error, false);
        EXPECT(barrelRoll, true);
        EXPECT(config.parseDynamic("testCategory:testValueHex", "0xaabbccdd").error, false);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueHex")), (Hyprlang::INT)0xAABBCCDD);
        EXPECT(config.parseDynamic("testStringColon", "1:3:3:7").error, false);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringColon")), std::string{"1:3:3:7"});
        EXPECT(config.parseDynamic("flagsStuff:value = 69").error, false);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("flagsStuff:value")), (Hyprlang::INT)69);

        // test dynamic special
        config.addSpecialConfigValue("specialGeneric:one", "boom", (Hyprlang::INT)0);
        EXPECT(config.parseDynamic("specialGeneric:one:boom = 1").error, false);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:one", "boom")), (Hyprlang::INT)1);

        // test variables
        std::cout << " → Testing variables\n";
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testVar")), 13371337);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringRecursive")), std::string{"abc"});

        // test dynamic variables
        std::cout << " → Testing dynamic variables\n";
        EXPECT(config.parseDynamic("$MY_VAR_2 = 420").error, false);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testVar")), 1337420);

        EXPECT(config.parseDynamic("$RECURSIVE1 = d").error, false);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringRecursive")), std::string{"dbc"});

        // test expression escape with dynamic vars
        EXPECT(config.parseDynamic("$MOVING_VAR = 500").error, false);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testDynamicEscapedExpression")), std::string{"{{ moved: 250 expr: {{500 / 2}} }}"});

        // test dynamic exprs
        EXPECT(config.parseDynamic("testExpr = {{EXPR_VAR * 2}}").error, false);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testExpr")), 1339L * 2);

        // test env variables
        std::cout << " → Testing env variables\n";
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testEnv")), std::string{getenv("SHELL")});

        // test special categories
        std::cout << " → Testing special categories\n";
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("special", "value", "a")), 1);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("special", "value", "b")), 2);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:one", "value")), 1);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:two", "value")), 2);
        EXPECT(config.parseDynamic("special[b]:value = 3").error, false);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("special", "value", "b")), 3);

        // test dynamic special variable
        EXPECT(config.parseDynamic("$SPECIALVAL1 = 2").error, false);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("special", "value", "a")), (Hyprlang::INT)2);

        // test copying
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:one", "copyTest")), 2);

        // test listing keys
        EXPECT(config.listKeysForSpecialCategory("special")[1], "b");

        // test anonymous
        EXPECT(config.listKeysForSpecialCategory("specialAnonymous").size(), 2);
        const auto KEYS = config.listKeysForSpecialCategory("specialAnonymous");
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymous", "value", KEYS[0].c_str())), 2);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymous", "value", KEYS[1].c_str())), 3);

        // test anonymous nested
        EXPECT(config.listKeysForSpecialCategory("specialAnonymousNested").size(), 2);
        const auto KEYS2 = config.listKeysForSpecialCategory("specialAnonymousNested");
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested:value1", KEYS2[0].c_str())), 1);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested:value2", KEYS2[0].c_str())), 2);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested:value1", KEYS2[1].c_str())), 3);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested:value2", KEYS2[1].c_str())), 4);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested1:nested2:value1", KEYS2[0].c_str())), 10);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested1:nested2:value2", KEYS2[0].c_str())), 11);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested1:nested2:value1", KEYS2[1].c_str())), 12);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialAnonymousNested", "nested1:nested2:value2", KEYS2[1].c_str())), 13);

        // test sourcing
        std::cout << " → Testing sourcing\n";
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("myColors:pink")), (Hyprlang::INT)0xFFc800c8);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("myColors:green")), (Hyprlang::INT)0xFF14f014);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("myColors:random")), (Hyprlang::INT)0xFFFF1337);

        // test custom type
        std::cout << " → Testing custom types\n";
        EXPECT(*reinterpret_cast<int64_t*>(std::any_cast<void*>(config.getConfigValue("customType"))), (Hyprlang::INT)1);

        // test multiline config
        EXPECT(std::any_cast<const char*>(config.getConfigValue("multiline")), std::string{"very        long            command"});

        std::cout << " → Testing error.conf\n";
        Hyprlang::CConfig errorConfig("./config/error.conf", {.verifyOnly = true, .throwAllErrors = true});

        errorConfig.commence();
        const auto ERRORS = errorConfig.parse();

        EXPECT(ERRORS.error, true);
        const auto ERRORSTR = std::string{ERRORS.getError()};
        EXPECT(std::count(ERRORSTR.begin(), ERRORSTR.end(), '\n'), 1);

        std::cout << " → Testing invalid-numbers.conf\n";
        Hyprlang::CConfig invalidNumbersConfig("./config/invalid-numbers.conf", {.throwAllErrors = true});
        invalidNumbersConfig.addConfigValue("invalidHex", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("emptyHex", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("hugeHex", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("invalidInt", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("emptyInt", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("invalidColor", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("invalidFirstCharColor", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("invalidColorAlpha", (Hyprlang::INT)0);
        invalidNumbersConfig.addConfigValue("invalidFirstCharColorAlpha", (Hyprlang::INT)0);

        invalidNumbersConfig.commence();
        const auto ERRORS2 = invalidNumbersConfig.parse();

        EXPECT(ERRORS2.error, true);
        const auto ERRORSTR2 = std::string{ERRORS2.getError()};
        EXPECT(std::count(ERRORSTR2.begin(), ERRORSTR2.end(), '\n'), 9 - 1);

        Hyprlang::CConfig multilineErrorConfig("./config/multiline-errors.conf", {.verifyOnly = true, .throwAllErrors = true});
        multilineErrorConfig.commence();
        const auto ERRORS3 = multilineErrorConfig.parse();
        EXPECT(ERRORS3.error, true);
        const auto ERRORSTR3 = std::string{ERRORS3.getError()};

        // Error on line 12
        EXPECT(ERRORSTR3.contains("12"), true);
        // Backslash at end of file
        EXPECT(ERRORSTR3.contains("backslash"), true);
    } catch (const char* e) {
        std::cout << Colors::RED << "Error: " << Colors::RESET << e << "\n";
        return 1;
    }

    return ret;
}
