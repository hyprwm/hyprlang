#include "public.hpp"

#include <unordered_map>
#include <string>
#include <vector>

struct SHandler {
    std::string                  name = "";
    Hyprlang::SHandlerOptions    options;
    Hyprlang::PCONFIGHANDLERFUNC func = nullptr;
};

class CConfigImpl {
  public:
    std::string                                             path = "";

    std::unordered_map<std::string, Hyprlang::CConfigValue> values;
    std::unordered_map<std::string, Hyprlang::CConfigValue> defaultValues;
    std::vector<SHandler>                                   handlers;

    std::vector<std::string>                                categories;

    std::string                                             parseError = "";
};