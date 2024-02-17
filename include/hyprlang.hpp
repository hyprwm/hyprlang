#pragma once

#ifndef HYPRLANG_HPP
#define HYPRLANG_HPP

#include <any>
#include <memory>
#include <string>
#include <fstream>
#include <vector>

class CConfigImpl;
struct SConfigDefaultValue;
struct SSpecialCategory;

namespace Hyprlang {

    struct SVector2D;
    class CConfigCustomValueType;

    /* Variable typedefs */

    /*!
        Basic integer config type
    */
    typedef int64_t INT;

    /*!
        Basic float config type
    */
    typedef float FLOAT;

    /*!
        Basic string config type
    */
    typedef const char* STRING;

    /*!
        Basic vec2 config type
    */
    typedef SVector2D VEC2;

    /*!
        Custom config type
    */
    typedef CConfigCustomValueType CUSTOMTYPE;

    /*!
        A very simple vector type 
    */
    struct SVector2D {
        float x = 0, y = 0;

        //
        bool operator==(const SVector2D& rhs) const {
            return x == rhs.x && y == rhs.y;
        }

        friend std::ostream& operator<<(std::ostream& os, const SVector2D& rhs) {
            return os << "[" << rhs.x << ", " << rhs.y << "]";
        }
    };

    class CParseResult {
      public:
        bool error = false;
        /*!
            Get this ParseResult's error string.
            Pointer valid until the error string is changed or this
            object gets destroyed.
        */
        const char* getError() const {
            return errorString;
        }
        /*!
            Set an error contained by this ParseResult.
            Creates a copy of the string, does not take ownership.
        */
        void setError(const char* err);

      private:
        void        setError(const std::string& err);

        std::string errorStdString = "";
        const char* errorString    = nullptr;

        friend class CConfig;
    };

    /*!
        Generic struct for options for the config parser
    */
    struct SConfigOptions {
        /*! 
            Don't throw errors on missing values.
        */
        bool verifyOnly = false;

        /*! 
            Return all errors instead of just the first
        */
        bool throwAllErrors = false;

        /*!
            \since 0.2.0

            Don't throw on a missing config file. Carry on as if nothing happened.
        */
        bool allowMissingConfig = false;
    };

    /*!
        Generic struct for options for handlers
    */
    struct SHandlerOptions {
        bool allowFlags = false;
    };

    /*!
        Generic struct for options for special categories
    */
    struct SSpecialCategoryOptions {
        /*!
            a key is the name of a value that will be the identifier of a special category
            can be left null for no key, aka a generic one
            keys are always strings. Default key value is "0"
        */
        const char* key = nullptr;

        /*!
            don't pop up an error if the config value is missing
        */
        bool ignoreMissing = false;
    };

    /*!
        typedefs
    */
    typedef CParseResult (*PCONFIGHANDLERFUNC)(const char* COMMAND, const char* VALUE);
    typedef CParseResult (*PCONFIGCUSTOMVALUEHANDLERFUNC)(const char* VALUE, void** data);
    typedef void (*PCONFIGCUSTOMVALUEDESTRUCTOR)(void** data);

    /*!
        Container for a custom config value type 
        When creating, pass your handler.
        Handler will receive a void** that points to a void* that you can set to your own
        thing. Pass a dtor to free whatever you allocated when the custom value type is being released.
        data may always be pointing to a nullptr. 
    */
    class CConfigCustomValueType {
      public:
        CConfigCustomValueType(PCONFIGCUSTOMVALUEHANDLERFUNC handler_, PCONFIGCUSTOMVALUEDESTRUCTOR dtor_, const char* defaultValue);
        ~CConfigCustomValueType();

        /*!
            \since 0.3.0

            get the data pointer for the custom value type.
        */
        void* getData() {
            return data;
        }

      private:
        PCONFIGCUSTOMVALUEHANDLERFUNC handler    = nullptr;
        PCONFIGCUSTOMVALUEDESTRUCTOR  dtor       = nullptr;
        void*                         data       = nullptr;
        std::string                   defaultVal = "";
        std::string                   lastVal    = "";

        friend class CConfigValue;
        friend class CConfig;
    };

    /*!
        Container for a config value
    */
    class CConfigValue {
      public:
        CConfigValue();
        CConfigValue(const INT value);
        CConfigValue(const FLOAT value);
        CConfigValue(const STRING value);
        CConfigValue(const VEC2 value);
        CConfigValue(CUSTOMTYPE&& value);
        CConfigValue(CConfigValue&&)       = delete;
        CConfigValue(const CConfigValue&&) = delete;
        CConfigValue(CConfigValue&)        = delete;

        /*!
            \since 0.3.0
        */
        CConfigValue(const CConfigValue&);

        ~CConfigValue();

        /*!
            Return a pointer to the data. Prefer getDataStaticPtr()
        */
        void* dataPtr() const;

        /*!
            \since 0.2.0

            Return a static pointer to the m_pData.
            As long as this configValue is alive, this pointer is valid.
            CConfigValues are alive as long as the owning CConfig is alive.

            Please note only the first (outer) pointer is static. The second
            may (and most likely will) be changing.

            For all types except STRING typeof(**retval) is the config value type
            (e.g. INT or FLOAT)

            Please note STRING is a special type and instead of
            typeof(**retval) being const char*, typeof(\*retval) is a const char*.
        */
        void* const* getDataStaticPtr() const;

        /*!
            Get the contained value as an std::any.
            For strings, this is a const char*.
            For custom data types, this is a CConfigCustomValueType*.
        */
        std::any getValue() const {
            switch (m_eType) {
                case CONFIGDATATYPE_EMPTY: throw;
                case CONFIGDATATYPE_INT: return std::any(*reinterpret_cast<INT*>(m_pData));
                case CONFIGDATATYPE_FLOAT: return std::any(*reinterpret_cast<FLOAT*>(m_pData));
                case CONFIGDATATYPE_STR: return std::any(reinterpret_cast<STRING>(m_pData));
                case CONFIGDATATYPE_VEC2: return std::any(*reinterpret_cast<VEC2*>(m_pData));
                case CONFIGDATATYPE_CUSTOM: return std::any(reinterpret_cast<CUSTOMTYPE*>(m_pData)->data);
                default: throw;
            }
            return {}; // unreachable
        }

        /*!
            \since 0.3.0
            
            a flag to notify whether this value has been set explicitly by the user,
            or not.
        */
        bool m_bSetByUser = false;

      private:
        // remember to also edit config.hpp if editing
        enum eDataType {
            CONFIGDATATYPE_EMPTY,
            CONFIGDATATYPE_INT,
            CONFIGDATATYPE_FLOAT,
            CONFIGDATATYPE_STR,
            CONFIGDATATYPE_VEC2,
            CONFIGDATATYPE_CUSTOM,
        };
        eDataType m_eType = eDataType::CONFIGDATATYPE_EMPTY;
        void*     m_pData = nullptr;
        void      defaultFrom(SConfigDefaultValue& ref);
        void      setFrom(std::any ref);
        void      setFrom(const CConfigValue* const ref);

        friend class CConfig;
    };

    /*!
        Base class for a config file
    */
    class CConfig {
      public:
        CConfig(const char* configPath, const SConfigOptions& options);
        ~CConfig();

        /*!
            Add a config value, for example myCategory:myValue. 
            This has to be done before commence()
            Value provided becomes default.
        */
        void addConfigValue(const char* name, const CConfigValue& value);

        /*!
            Register a handler. Can be called anytime, though not recommended
            to do this dynamically .
        */
        void registerHandler(PCONFIGHANDLERFUNC func, const char* name, SHandlerOptions options);

        /*!
            \since 0.3.0
            
            Unregister a handler.        
        */
        void unregisterHandler(const char* name);

        /*!
            Commence the config state. Config becomes immutable, as in
            no new values may be added or removed. Required for parsing.
        */
        void commence();

        /*!
            Add a special category. Can be done dynamically.
        */
        void addSpecialCategory(const char* name, SSpecialCategoryOptions options);

        /*!
            \since 0.3.0

            Remove a special category. Can be done dynamically.
        */
        void removeSpecialCategory(const char* name);

        /*!
            \since 0.3.2

            Add a config value to a special category.

            \note Before 0.3.2, this takes a `const CConfigValue` (non-ref)
        */
        void addSpecialConfigValue(const char* cat, const char* name, const CConfigValue& value);

        /*!
            Remove a config value from a special category.
        */
        void removeSpecialConfigValue(const char* cat, const char* name);

        /*!
            Parse the config. Refresh the values.
        */
        CParseResult parse();

        /*!
            Same as parse(), but parse a specific file, without any refreshing. 
            recommended to use for stuff like source = path.conf
        */
        CParseResult parseFile(const char* file);

        /*!
            Parse a single "line", dynamically. 
            Values set by this are temporary and will be overwritten 
            by default / config on the next parse()
        */
        CParseResult parseDynamic(const char* line);
        CParseResult parseDynamic(const char* command, const char* value);

        /*!
            Get a config's value ptr. These are static.
            nullptr on fail 
        */
        CConfigValue* getConfigValuePtr(const char* name);

        /*!
           Get a special category's config value ptr. These are only static for static (key-less)
           categories.
           key can be nullptr for static categories. Cannot be nullptr for id-based categories. 
           nullptr on fail.
        */
        CConfigValue* getSpecialConfigValuePtr(const char* category, const char* name, const char* key = nullptr);

        /*!
            Get a config value's stored value. Empty on fail
        */
        std::any getConfigValue(const char* name) {
            CConfigValue* val = getConfigValuePtr(name);
            if (!val)
                return {};
            return val->getValue();
        }

        /*!
            Get a special config value's stored value. Empty on fail.
        */
        std::any getSpecialConfigValue(const char* category, const char* name, const char* key = nullptr) {
            CConfigValue* val = getSpecialConfigValuePtr(category, name, key);
            if (!val)
                return {};
            return val->getValue();
        }

        /*!
            Check whether a special category with the provided key value exists

            \since 0.3.0
        */
        bool specialCategoryExistsForKey(const char* category, const char* key);

        /*!
            Get a vector with all registered keys for a special category

            It's an error to query this for a static or non-existent category

            \since 0.4.0
        */
        std::vector<std::string> listKeysForSpecialCategory(const char* category) {
            const char** cats = nullptr;
            size_t       len  = 0;
            retrieveKeysForCat(category, &cats, &len);

            if (len == 0)
                return {};

            std::vector<std::string> result;
            for (size_t i = 0; i < len; ++i) {
                result.push_back(cats[i]);
            }

            free(cats);

            return result;
        }

      private:
        bool         m_bCommenced = false;

        CConfigImpl* impl;

        CParseResult parseLine(std::string line, bool dynamic = false);
        CParseResult configSetValueSafe(const std::string& command, const std::string& value);
        CParseResult parseVariable(const std::string& lhs, const std::string& rhs, bool dynamic = false);
        void         clearState();
        void         applyDefaultsToCat(SSpecialCategory& cat);
        void         retrieveKeysForCat(const char* category, const char*** out, size_t* len);
    };
};
#endif