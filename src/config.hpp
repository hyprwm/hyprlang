#include "public.hpp"

#include <unordered_map>
#include <string>
#include <vector>

class CConfigImpl {
  public:
    std::string                                             path = "";

    std::unordered_map<std::string, Hyprlang::CConfigValue> values;
    std::unordered_map<std::string, Hyprlang::CConfigValue> defaultValues;

    std::vector<std::string>                                categories;

    std::string                                             parseError = "";
};