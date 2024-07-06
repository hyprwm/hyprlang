#include "public.hpp"

#include <unordered_map>
#include <string>
#include <vector>

static const char* MULTILINE_SPACE_CHARSET = " \t";

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

struct SMultiline {
    std::string buffer;
    char        delimiter = 0;
    bool        active    = false;
    std::string lhs;
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
    bool                                                     isSpecialCategory      = false;
    SMultiline                                               multiline;

    std::string                                              parseError = "";

    Hyprlang::SConfigOptions                                 configOptions;

    void                                                     parseComment(const std::string& comment);

    struct {
        bool noError = false;
    } currentFlags;
};
