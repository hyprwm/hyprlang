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

struct SSpecialCategoryDescriptor {
    std::string                                             name = "";
    std::string                                             key  = "";
    std::unordered_map<std::string, Hyprlang::CConfigValue> defaultValues;
    bool                                                    dontErrorOnMissing = false;
};

struct SSpecialCategory {
    SSpecialCategoryDescriptor*                             descriptor = nullptr;
    std::string                                             name       = "";
    std::string                                             key        = ""; // empty means no key
    std::unordered_map<std::string, Hyprlang::CConfigValue> values;
    bool                                                    isStatic = false;

    void                                                    applyDefaults();
};

class CConfigImpl {
  public:
    std::string                                              path = "";

    std::unordered_map<std::string, Hyprlang::CConfigValue>  values;
    std::unordered_map<std::string, Hyprlang::CConfigValue>  defaultValues;
    std::vector<SHandler>                                    handlers;
    std::vector<SVariable>                                   variables;
    std::vector<SVariable>                                   envVariables;
    std::vector<std::unique_ptr<SSpecialCategory>>           specialCategories;
    std::vector<std::unique_ptr<SSpecialCategoryDescriptor>> specialCategoryDescriptors;

    std::vector<std::string>                                 categories;
    std::string                                              currentSpecialKey = "";

    std::string                                              parseError = "";
};