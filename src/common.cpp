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
    if (m_eType == CONFIGDATATYPE_CUSTOM)
        reinterpret_cast<CConfigCustomValueType*>(m_pData)->dtor(&reinterpret_cast<CConfigCustomValueType*>(m_pData)->data);

    if (m_pData)
        free(m_pData);
}

CConfigValue::CConfigValue(const int64_t value) {
    m_pData                              = calloc(1, sizeof(int64_t));
    *reinterpret_cast<int64_t*>(m_pData) = value;
    m_eType                              = CONFIGDATATYPE_INT;
}

CConfigValue::CConfigValue(const float value) {
    m_pData                            = calloc(1, sizeof(float));
    *reinterpret_cast<float*>(m_pData) = value;
    m_eType                            = CONFIGDATATYPE_FLOAT;
}

CConfigValue::CConfigValue(const SVector2D value) {
    m_pData                                = calloc(1, sizeof(SVector2D));
    *reinterpret_cast<SVector2D*>(m_pData) = value;
    m_eType                                = CONFIGDATATYPE_VEC2;
}

CConfigValue::CConfigValue(const char* value) {
    m_pData = calloc(1, strlen(value) + 1);
    strncpy((char*)m_pData, value, strlen(value));
    m_eType = CONFIGDATATYPE_STR;
}

CConfigValue::CConfigValue(CConfigCustomValueType&& value) {
    m_pData = calloc(1, sizeof(CConfigCustomValueType));
    new (m_pData) CConfigCustomValueType(value);
    m_eType = CONFIGDATATYPE_CUSTOM;
}

CConfigValue::CConfigValue(const CConfigValue& other) {
    m_eType = other.m_eType;
    setFrom(other.getValue());
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
}

CConfigCustomValueType::~CConfigCustomValueType() {
    dtor(&data);
}

void CConfigValue::defaultFrom(SConfigDefaultValue& ref) {
    m_eType = (CConfigValue::eDataType)ref.type;
    switch (m_eType) {
        case CONFIGDATATYPE_FLOAT: {
            if (!m_pData)
                m_pData = calloc(1, sizeof(float));
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(ref.data);
            break;
        }
        case CONFIGDATATYPE_INT: {
            if (!m_pData)
                m_pData = calloc(1, sizeof(int64_t));
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(ref.data);
            break;
        }
        case CONFIGDATATYPE_STR: {
            if (m_pData)
                free(m_pData);
            std::string str = std::any_cast<std::string>(ref.data);
            m_pData         = calloc(1, str.length() + 1);
            strncpy((char*)m_pData, str.c_str(), str.length());
            break;
        }
        case CONFIGDATATYPE_VEC2: {
            if (!m_pData)
                m_pData = calloc(1, sizeof(SVector2D));
            *reinterpret_cast<SVector2D*>(m_pData) = std::any_cast<SVector2D>(ref.data);
            break;
        }
        case CONFIGDATATYPE_CUSTOM: {
            if (!m_pData)
                m_pData = calloc(1, sizeof(CConfigCustomValueType));
            CConfigCustomValueType* type = reinterpret_cast<CConfigCustomValueType*>(m_pData);
            type->handler                = ref.handler;
            type->dtor                   = ref.dtor;
            type->handler(std::any_cast<std::string>(ref.data).c_str(), &type->data);
            break;
        }
        default: {
            throw "bad defaultFrom type";
        }
    }

    m_bSetByUser = false;
}

void CConfigValue::setFrom(std::any value) {
    switch (m_eType) {
        case CONFIGDATATYPE_FLOAT: {
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(value);
            break;
        }
        case CONFIGDATATYPE_INT: {
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(value);
            break;
        }
        case CONFIGDATATYPE_STR: {
            if (m_pData)
                free(m_pData);
            std::string str = std::any_cast<std::string>(value);
            m_pData         = calloc(1, str.length() + 1);
            strncpy((char*)m_pData, str.c_str(), str.length());
            break;
        }
        case CONFIGDATATYPE_VEC2: {
            *reinterpret_cast<SVector2D*>(m_pData) = std::any_cast<SVector2D>(value);
            break;
        }
        // case CONFIGDATATYPE_CUSTOM: {
        //     CConfigCustomValueType* type = reinterpret_cast<CConfigCustomValueType*>(m_pData);
        //     type->handler                = ref.handler;
        //     type->dtor                   = ref.dtor;
        //     type->handler(std::any_cast<std::string>(ref.data).c_str(), &type->data);
        //     break;
        // }
        default: {
            throw "bad defaultFrom type";
        }
    }
}