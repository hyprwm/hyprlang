#include <iostream>

#include <hyprlang.hpp>

#define FUZZ_ITERS 1337

std::string garbage() {
    srand(time(nullptr));

    int         len = rand() % 10000;

    std::string chars;
    for (int i = 0; i < len; ++i) {
        chars += rand() % 254 + 1;
    }

    return chars;
}

int main(int argc, char** argv, char** envp) {

    Hyprlang::CConfig config("./eeeeeeeUnused", {.allowMissingConfig = true});
    config.addConfigValue("test", {INT64_C(0)});

    config.parseDynamic("");
    config.parseDynamic("", "");
    config.parseDynamic("}");
    for (size_t i = 0; i < FUZZ_ITERS; ++i) {
        config.parseDynamic(garbage().c_str(), garbage().c_str());
        config.parseDynamic((garbage() + "=" + garbage()).c_str());
        config.parseDynamic(garbage().c_str());
        config.parseDynamic((garbage() + " {").c_str());
        config.parseDynamic((std::string{"test = "} + garbage()).c_str());
        config.parseDynamic((std::string{"$"} + garbage()).c_str());
        config.parseDynamic((std::string{"$VAR = "} + garbage()).c_str());
    }
    config.parseDynamic("}");

    std::cout << "Success, no fuzzing errors\n";

    return 0;
}
