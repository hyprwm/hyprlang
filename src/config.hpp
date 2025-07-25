#include "public.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <expected>

struct SHandler {
    std::string                  name = "";
    Hyprlang::SHandlerOptions    options;
    Hyprlang::PCONFIGHANDLERFUNC func = nullptr;
};

struct SVariable {
    std::string name  = "";
    std::string value = "";

    struct SVarLine {
        std::string              line;
        std::vector<std::string> categories;
        SSpecialCategory*        specialCategory = nullptr; // if applicable
    };

    std::vector<SVarLine> linesContainingVar; // for dynamic updates

    bool                  truthy() {
        return value.length() > 0;
    }
};

// remember to also edit CConfigValue if editing
enum eDataType {
    CONFIGDATATYPE_EMPTY,
    CONFIGDATATYPE_INT,
    CONFIGDATATYPE_FLOAT,
    CONFIGDATATYPE_STR,
    CONFIGDATATYPE_VEC2,
    CONFIGDATATYPE_CUSTOM,
};

// CUSTOM is stored as STR!!
struct SConfigDefaultValue {
    std::any  data;
    eDataType type = CONFIGDATATYPE_EMPTY;

    // this sucks but I have no better idea
    Hyprlang::PCONFIGCUSTOMVALUEHANDLERFUNC handler = nullptr;
    Hyprlang::PCONFIGCUSTOMVALUEDESTRUCTOR  dtor    = nullptr;
};

struct SSpecialCategoryDescriptor {
    std::string                                          name = "";
    std::string                                          key  = "";
    std::unordered_map<std::string, SConfigDefaultValue> defaultValues;
    bool                                                 dontErrorOnMissing = false;
    bool                                                 anonymous          = false;
};

struct SSpecialCategory {
    SSpecialCategoryDescriptor*                             descriptor = nullptr;
    std::string                                             name       = "";
    std::string                                             key        = ""; // empty means no key
    std::unordered_map<std::string, Hyprlang::CConfigValue> values;
    bool                                                    isStatic = false;

    void                                                    applyDefaults();

    // for easy anonymous ID'ing
    size_t anonymousID = 0;
};

enum eGetNextLineFailure : uint8_t {
    GETNEXTLINEFAILURE_EOF = 0,
    GETNEXTLINEFAILURE_BACKSLASH,
};

class CConfigImpl {
  public:
    std::string path         = "";
    std::string originalPath = "";

    // if not-empty, used instead of path
    std::string                                              rawConfigString = "";

    std::unordered_map<std::string, Hyprlang::CConfigValue>  values;
    std::unordered_map<std::string, SConfigDefaultValue>     defaultValues;
    std::vector<SHandler>                                    handlers;
    std::vector<SVariable>                                   variables;
    std::vector<SVariable>                                   envVariables;
    std::vector<std::unique_ptr<SSpecialCategory>>           specialCategories;
    std::vector<std::unique_ptr<SSpecialCategoryDescriptor>> specialCategoryDescriptors;

    std::vector<std::string>                                 categories;
    std::string                                              currentSpecialKey      = "";
    SSpecialCategory*                                        currentSpecialCategory = nullptr; // if applicable

    std::string                                              parseError = "";

    Hyprlang::SConfigOptions                                 configOptions;

    std::optional<std::string>                               parseComment(const std::string& comment);
    std::expected<float, std::string>                        parseExpression(const std::string& s);
    SVariable*                                               getVariable(const std::string& name);

    struct {
        bool noError       = false;
        bool inAnIfBlock   = false;
        bool ifBlockFailed = false;
    } currentFlags;
};
