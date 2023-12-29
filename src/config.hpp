#include "public.hpp"

#include <unordered_map>
#include <string>
#include <vector>

struct SHandler {
    std::string                  name = "";
    Hyprlang::SHandlerOptions    options;
    Hyprlang::PCONFIGHANDLERFUNC func = nullptr;
};

struct SVariable {
    std::string              name  = "";
    std::string              value = "";
    std::vector<std::string> linesContainingVar; // for dynamic updates
};

class CConfigImpl {
  public:
    std::string                                             path = "";

    std::unordered_map<std::string, Hyprlang::CConfigValue> values;
    std::unordered_map<std::string, Hyprlang::CConfigValue> defaultValues;
    std::vector<SHandler>                                   handlers;
    std::vector<SVariable>                                  variables;
    std::vector<SVariable>                                  envVariables;

    std::vector<std::string>                                categories;

    std::string                                             parseError = "";
};