#pragma once
#include <any>
#include <memory>
#include <string>
#include <fstream>

class CConfigImpl;

namespace Hyprlang {

    /* types */
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

    struct SConfigValueImpl;
    /* Container for a config value */
    class CConfigValue {
      public:
        CConfigValue();
        CConfigValue(const int64_t value);
        CConfigValue(const float value);
        CConfigValue(const char* value);
        CConfigValue(const SVector2D value);
        CConfigValue(const CConfigValue&);
        CConfigValue(CConfigValue&&);
        void operator=(const CConfigValue&);
        ~CConfigValue();

        void*    dataPtr() const;
        std::any getValue() const {
            switch (m_eType) {
                case CONFIGDATATYPE_EMPTY: throw;
                case CONFIGDATATYPE_INT: return std::any(*reinterpret_cast<int64_t*>(m_pData));
                case CONFIGDATATYPE_FLOAT: return std::any(*reinterpret_cast<float*>(m_pData));
                case CONFIGDATATYPE_STR: return std::any(reinterpret_cast<const char*>(m_pData));
                case CONFIGDATATYPE_VEC2: return std::any(*reinterpret_cast<SVector2D*>(m_pData));
                default: throw;
            }
            return {}; // unreachable
        }

      private:
        enum eDataType {
            CONFIGDATATYPE_EMPTY,
            CONFIGDATATYPE_INT,
            CONFIGDATATYPE_FLOAT,
            CONFIGDATATYPE_STR,
            CONFIGDATATYPE_VEC2,
        };
        eDataType m_eType = eDataType::CONFIGDATATYPE_EMPTY;
        void*     m_pData = nullptr;

        friend class CConfig;
    };

    class CParseResult {
      public:
        bool        error = false;
        const char* getError() const {
            return errorString;
        }
        void setError(const char* err);

      private:
        void        setError(const std::string& err);

        std::string errorStdString = "";
        const char* errorString    = nullptr;

        friend class CConfig;
    };

    /* Base class for a config file */
    class CConfig {
      public:
        CConfig(const char* configPath);
        ~CConfig();

        /* Add a config value, for example myCategory:myValue. 
           This has to be done before commence()
           Value provided becomes default */
        void addConfigValue(const char* name, const CConfigValue value);

        /* Commence the config state. Config becomes immutable, as in
           no new values may be added or removed. Required for parsing. */
        void commence();

        /* Parse the config. Refresh the values. */
        CParseResult parse();

        /* Get a config's value ptr. These are static.
           nullptr on fail */
        CConfigValue* getConfigValuePtr(const char* name);

        /* Get a config value's stored value. Empty on fail*/
        std::any getConfigValue(const char* name) {
            CConfigValue* val = getConfigValuePtr(name);
            if (!val)
                return {};
            return val->getValue();
        }

      private:
        bool         m_bCommenced = false;

        CConfigImpl* impl;

        CParseResult parseLine(std::string line);
        CParseResult configSetValueSafe(const std::string& command, const std::string& value);
    };
};