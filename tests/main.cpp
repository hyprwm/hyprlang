#include <iostream>

#include <hyprlang.hpp>

#define EXPECT(expr, val)                                                                                                                                                          \
    if (const auto RESULT = expr; RESULT != (val)) {                                                                                                                               \
        std::cout << "Failed: " << #expr << ", expected " << #val << " but got " << RESULT << "\n";                                                                                \
        ret = 1;                                                                                                                                                                   \
    } else {                                                                                                                                                                       \
        std::cout << "Passed " << #expr << ". Got " << #val << "\n";                                                                                                               \
    }

int main(int argc, char** argv, char** envp) {
    int ret = 0;

    try {
        std::cout << "Starting test\n";

        Hyprlang::CConfig config("./config/config.conf");

        // setup config
        config.addConfigValue("testInt", 0L);
        config.addConfigValue("testFloat", 0.F);
        config.addConfigValue("testString", "");
        config.addConfigValue("testStringQuotes", "");
        config.addConfigValue("testCategory:testValueInt", 0L);
        config.addConfigValue("testCategory:testValueHex", 0xAL);
        config.addConfigValue("testCategory:nested1:testValueNest", 0L);
        config.addConfigValue("testCategory:nested1:nested2:testValueNest", 0L);
        config.addConfigValue("testDefault", 123L);

        config.commence();

        const auto PARSERESULT = config.parse();
        if (PARSERESULT.error) {
            std::cout << "Parse error: " << PARSERESULT.getError() << "\n";
            return 1;
        }

        EXPECT(PARSERESULT.error, false);

        // test values
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testInt")), 123);
        EXPECT(std::any_cast<float>(config.getConfigValue("testFloat")), 123.456f);
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testString")), std::string{"Hello World! ## This is not a comment!"});
        EXPECT(std::any_cast<const char*>(config.getConfigValue("testStringQuotes")), std::string{"\"Hello World!\""});
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueInt")), 123456L);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:testValueHex")), 0xFFFFAABBL);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:nested1:testValueNest")), 1L);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testCategory:nested1:nested2:testValueNest")), 1L);
        EXPECT(std::any_cast<int64_t>(config.getConfigValue("testDefault")), 123L);
    } catch (const char* e) {
        std::cout << "Error: " << e << "\n";
        return 1;
    }

    return ret;
}