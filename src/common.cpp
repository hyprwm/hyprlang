#include "public.hpp"
#include "config.hpp"
#include <string.h>

using namespace Hyprlang;

void CParseResult::setError(const std::string& err) {
    error          = true;
    errorStdString = err;
    errorString    = errorStdString.c_str();
}

void CParseResult::setError(const char* err) {
    error          = true;
    errorStdString = err;
    errorString    = errorStdString.c_str();
}

CConfigValue::~CConfigValue() {
    if (m_pData) {
        switch (m_eType) {
            case CONFIGDATATYPE_INT: delete (int64_t*)m_pData; break;
            case CONFIGDATATYPE_FLOAT: delete (float*)m_pData; break;
            case CONFIGDATATYPE_VEC2: delete (SVector2D*)m_pData; break;
            case CONFIGDATATYPE_CUSTOM: delete (CConfigCustomValueType*)m_pData; break;
            case CONFIGDATATYPE_STR: delete[] (char*)m_pData; break;

            default: break; // oh no?
        }
    }
}

CConfigValue::CConfigValue(const int64_t value) {
    m_pData                              = new int64_t;
    *reinterpret_cast<int64_t*>(m_pData) = value;
    m_eType                              = CONFIGDATATYPE_INT;
}

CConfigValue::CConfigValue(const float value) {
    m_pData                            = new float;
    *reinterpret_cast<float*>(m_pData) = value;
    m_eType                            = CONFIGDATATYPE_FLOAT;
}

CConfigValue::CConfigValue(const SVector2D value) {
    m_pData                                = new SVector2D;
    *reinterpret_cast<SVector2D*>(m_pData) = value;
    m_eType                                = CONFIGDATATYPE_VEC2;
}

CConfigValue::CConfigValue(const char* value) {
    m_pData = new char[strlen(value) + 1];
    strncpy((char*)m_pData, value, strlen(value));
    ((char*)m_pData)[strlen(value)] = '\0';
    m_eType                         = CONFIGDATATYPE_STR;
}

CConfigValue::CConfigValue(CConfigCustomValueType&& value) {
    m_pData = new CConfigCustomValueType(value);
    m_eType = CONFIGDATATYPE_CUSTOM;
}

CConfigValue::CConfigValue(const CConfigValue& other) {
    m_eType = other.m_eType;
    setFrom(&other);
}

CConfigValue::CConfigValue() {
    ;
}

void* CConfigValue::dataPtr() const {
    return m_pData;
}

void* const* CConfigValue::getDataStaticPtr() const {
    return &m_pData;
}

CConfigCustomValueType::CConfigCustomValueType(PCONFIGCUSTOMVALUEHANDLERFUNC handler_, PCONFIGCUSTOMVALUEDESTRUCTOR dtor_, const char* def) {
    handler    = handler_;
    dtor       = dtor_;
    defaultVal = def;
    lastVal    = def;
}

CConfigCustomValueType::~CConfigCustomValueType() {
    dtor(&data);
}

void CConfigValue::defaultFrom(SConfigDefaultValue& ref) {
    m_eType = (CConfigValue::eDataType)ref.type;
    switch (m_eType) {
        case CONFIGDATATYPE_FLOAT: {
            if (!m_pData)
                m_pData = new float;
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(ref.data);
            break;
        }
        case CONFIGDATATYPE_INT: {
            if (!m_pData)
                m_pData = new int64_t;
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(ref.data);
            break;
        }
        case CONFIGDATATYPE_STR: {
            if (m_pData)
                delete[] (char*)m_pData;
            std::string str = std::any_cast<std::string>(ref.data);
            m_pData         = new char[str.length() + 1];
            strncpy((char*)m_pData, str.c_str(), str.length()); // TODO: please just wrap this
            ((char*)m_pData)[str.length()] = '\0';
            break;
        }
        case CONFIGDATATYPE_VEC2: {
            if (!m_pData)
                m_pData = new SVector2D;
            *reinterpret_cast<SVector2D*>(m_pData) = std::any_cast<SVector2D>(ref.data);
            break;
        }
        case CONFIGDATATYPE_CUSTOM: {
            if (!m_pData)
                m_pData = new CConfigCustomValueType(ref.handler, ref.dtor, std::any_cast<std::string>(ref.data).c_str());
            CConfigCustomValueType* type = reinterpret_cast<CConfigCustomValueType*>(m_pData);
            type->handler(std::any_cast<std::string>(ref.data).c_str(), &type->data);
            type->lastVal = std::any_cast<std::string>(ref.data);
            break;
        }
        default: {
            throw "bad defaultFrom type";
        }
    }

    m_bSetByUser = false;
}

void CConfigValue::setFrom(const CConfigValue* const ref) {
    switch (m_eType) {
        case CONFIGDATATYPE_FLOAT: {
            if (!m_pData)
                m_pData = new float;
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(ref->getValue());
            break;
        }
        case CONFIGDATATYPE_INT: {
            if (!m_pData)
                m_pData = new int64_t;
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(ref->getValue());
            break;
        }
        case CONFIGDATATYPE_STR: {
            if (m_pData)
                delete[] (char*)m_pData;
            std::string str = std::any_cast<std::string>(ref->getValue());
            m_pData         = new char[str.length() + 1];
            strncpy((char*)m_pData, str.c_str(), str.length());
            ((char*)m_pData)[str.length()] = '\0';
            break;
        }
        case CONFIGDATATYPE_VEC2: {
            if (!m_pData)
                m_pData = new SVector2D;
            *reinterpret_cast<SVector2D*>(m_pData) = std::any_cast<SVector2D>(ref->getValue());
            break;
        }
        case CONFIGDATATYPE_CUSTOM: {
            CConfigCustomValueType* reftype = reinterpret_cast<CConfigCustomValueType*>(ref->m_pData);

            if (!m_pData)
                m_pData = new CConfigCustomValueType(reftype->handler, reftype->dtor, reftype->defaultVal.c_str());

            CConfigCustomValueType* type = reinterpret_cast<CConfigCustomValueType*>(m_pData);
            type->handler(reftype->lastVal.c_str(), &type->data);
            break;
        }
        default: {
            throw "bad defaultFrom type";
        }
    }
}

void CConfigValue::setFrom(std::any ref) {
    switch (m_eType) {
        case CONFIGDATATYPE_FLOAT: {
            if (!m_pData)
                m_pData = new float;
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(ref);
            break;
        }
        case CONFIGDATATYPE_INT: {
            if (!m_pData)
                m_pData = new int64_t;
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(ref);
            break;
        }
        case CONFIGDATATYPE_STR: {
            if (m_pData)
                delete[] (char*)m_pData;
            std::string str = std::any_cast<std::string>(ref);
            m_pData         = new char[str.length() + 1];
            strncpy((char*)m_pData, str.c_str(), str.length());
            ((char*)m_pData)[str.length()] = '\0';
            break;
        }
        case CONFIGDATATYPE_VEC2: {
            if (!m_pData)
                m_pData = new SVector2D;
            *reinterpret_cast<SVector2D*>(m_pData) = std::any_cast<SVector2D>(ref);
            break;
        }
        case CONFIGDATATYPE_CUSTOM: {
            throw "bad defaultFrom type (cannot custom from std::any)";
            break;
        }
        default: {
            throw "bad defaultFrom type";
        }
    }
}