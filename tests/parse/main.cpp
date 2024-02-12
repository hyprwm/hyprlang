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
bool                          barrelRoll  = false;
std::string                   flagsFound  = "";
Hyprlang::CConfig*            pConfig     = nullptr;
std::string                   currentPath = "";

static Hyprlang::CParseResult handleDoABarrelRoll(const char* COMMAND, const char* VALUE) {
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
        config.addConfigValue("testInt", INT64_C(0));
        config.addConfigValue("testFloat", 0.F);
        config.addConfigValue("testVec", Hyprlang::SVector2D{69, 420});
        config.addConfigValue("testString", "");
        config.addConfigValue("testEnv", "");
        config.addConfigValue("testVar", INT64_C(0));
        config.addConfigValue("testStringQuotes", "");
        config.addConfigValue("testCategory:testValueInt", INT64_C(0));
        config.addConfigValue("testCategory:testValueHex", INT64_C(0xA));
        config.addConfigValue("testCategory:nested1:testValueNest", INT64_C(0));
        config.addConfigValue("testCategory:nested1:nested2:testValueNest", INT64_C(0));
        config.addConfigValue("testDefault", INT64_C(123));
        config.addConfigValue("testCategory:testColor1", INT64_C(0));
        config.addConfigValue("testCategory:testColor2", INT64_C(0));
        config.addConfigValue("testCategory:testColor3", INT64_C(0));
        config.addConfigValue("myColors:pink", INT64_C(0));
        config.addConfigValue("myColors:green", INT64_C(0));
        config.addConfigValue("myColors:random", INT64_C(0));
        config.addConfigValue("customType", {Hyprlang::CConfigCustomValueType{&handleCustomValueSet, &handleCustomValueDestroy, "def"}});

        config.registerHandler(&handleDoABarrelRoll, "doABarrelRoll", {false});
        config.registerHandler(&handleFlagsTest, "flags", {true});
        config.registerHandler(&handleSource, "source", {false});

        config.addSpecialCategory("special", {"key"});
        config.addSpecialConfigValue("special", "value", INT64_C(0));

        config.commence();

        config.addSpecialCategory("specialGeneric:one", {nullptr, true});
        config.addSpecialConfigValue("specialGeneric:one", "value", INT64_C(0));
        config.addSpecialCategory("specialGeneric:two", {nullptr, true});
        config.addSpecialConfigValue("specialGeneric:two", "value", INT64_C(0));

        const Hyprlang::CConfigValue copyTest = {INT64_C(1)};
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
        auto EXP = Hyprlang::SVector2D{69, 420};
        EXPECT(std::any_cast<Hyprlang::SVector2D>(config.getConfigValue("testVec")), EXP);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testString")), std::string{"Hello World! # This is not a comment!"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringQuotes")), std::string{"\"Hello World!\""});
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueInt")), INT64_C(123456));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueHex")), INT64_C(0xFFFFAABB));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:nested1:testValueNest")), INT64_C(1));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:nested1:nested2:testValueNest")), INT64_C(1));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testDefault")), INT64_C(123));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testColor1")), INT64_C(0xFFFFFFFF));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testColor2")), INT64_C(0xFF000000));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testColor3")), INT64_C(0x22ffeeff));

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

        // test dynamic
        std::cout << " → Testing dynamic\n";
        barrelRoll = false;
        EXPECT(config.parseDynamic("doABarrelRoll = woohoo, some, params").error, false);
        EXPECT(barrelRoll, true);
        EXPECT(config.parseDynamic("testCategory:testValueHex", "0xaabbccdd").error, false);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueHex")), INT64_C(0xAABBCCDD));

        // test variables
        std::cout << " → Testing variables\n";
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testVar")), 13371337);

        // test dynamic variables
        std::cout << " → Testing dynamic variables\n";
        EXPECT(config.parseDynamic("$MY_VAR_2 = 420").error, false);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testVar")), 1337420);

        // test env variables
        std::cout << " → Testing env variables\n";
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testEnv")), std::string{getenv("SHELL")});

        // test special categories
        std::cout << " → Testing special categories\n";
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("special", "value", "a")), 1);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("special", "value", "b")), 2);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:one", "value")), 1);
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:two", "value")), 2);

        // test copying
        EXPECT(std::any_cast<int64_t>(config.getSpecialConfigValue("specialGeneric:one", "copyTest")), 2);

        // test sourcing
        std::cout << " → Testing sourcing\n";
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("myColors:pink")), INT64_C(0xFFc800c8));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("myColors:green")), INT64_C(0xFF14f014));
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("myColors:random")), INT64_C(0xFFFF1337));

        // test custom type
        std::cout << " → Testing custom types\n";
        EXPECT(*reinterpret_cast<int64_t*>(std::any_cast<void*>(config.getConfigValue("customType"))), INT64_C(1));

        std::cout << " → Testing error.conf\n";
        Hyprlang::CConfig errorConfig("./config/error.conf", {.verifyOnly = true, .throwAllErrors = true});

        errorConfig.commence();
        const auto ERRORS = errorConfig.parse();

        EXPECT(ERRORS.error, true);
        const auto ERRORSTR = std::string{ERRORS.getError()};
        EXPECT(std::count(ERRORSTR.begin(), ERRORSTR.end(), '\n'), 1);

    } catch (const char* e) {
        std::cout << Colors::RED << "Error: " << Colors::RESET << e << "\n";
        return 1;
    }

    return ret;
}
