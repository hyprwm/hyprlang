#include "public.hpp"
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

CConfigValue::CConfigValue(const char* value) {
    m_pData = calloc(1, strlen(value) + 1);
    strncpy((char*)m_pData, value, strlen(value));
    m_eType = CONFIGDATATYPE_STR;
}

CConfigValue::CConfigValue(const CConfigValue& ref) {
    m_eType = ref.m_eType;
    switch (ref.m_eType) {
        case eDataType::CONFIGDATATYPE_INT: {
            m_pData                              = calloc(1, sizeof(int64_t));
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(ref.getValue());
            break;
        }
        case eDataType::CONFIGDATATYPE_FLOAT: {
            m_pData                            = calloc(1, sizeof(float));
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(ref.getValue());
            break;
        }
        case eDataType::CONFIGDATATYPE_STR: {
            auto str = std::any_cast<const char*>(ref.getValue());
            m_pData  = calloc(1, strlen(str) + 1);
            strncpy((char*)m_pData, str, strlen(str));
            break;
        }
    }
}

void CConfigValue::operator=(const CConfigValue& ref) {
    m_eType = ref.m_eType;
    switch (ref.m_eType) {
        case eDataType::CONFIGDATATYPE_INT: {
            m_pData                              = calloc(1, sizeof(int64_t));
            *reinterpret_cast<int64_t*>(m_pData) = std::any_cast<int64_t>(ref.getValue());
            break;
        }
        case eDataType::CONFIGDATATYPE_FLOAT: {
            m_pData                            = calloc(1, sizeof(float));
            *reinterpret_cast<float*>(m_pData) = std::any_cast<float>(ref.getValue());
            break;
        }
        case eDataType::CONFIGDATATYPE_STR: {
            auto str = std::any_cast<const char*>(ref.getValue());
            m_pData  = calloc(1, strlen(str) + 1);
            strncpy((char*)m_pData, str, strlen(str));
            break;
        }
    }
}

CConfigValue::CConfigValue(CConfigValue&& ref) {
    m_pData     = ref.dataPtr();
    m_eType     = ref.m_eType;
    ref.m_eType = eDataType::CONFIGDATATYPE_EMPTY;
    ref.m_pData = nullptr;
}

CConfigValue::CConfigValue() {
    ;
}

void* CConfigValue::dataPtr() const {
    return m_pData;
}
