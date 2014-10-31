#ifndef SETTINGS_H
#define SETTINGS_H

#define SI_CONVERT_GENERIC
//#define SI_SUPPORT_IOSTREAMS
#include "SimpleIni.h"
#include <string>

class Settings : private CSimpleIniA
{
public:
    Settings(const std::string& filename);
    ~Settings();
    bool loadFromFile();
    bool saveToFile();
    using CSimpleIniA::GetValue;
    using CSimpleIniA::GetLongValue;
    using CSimpleIni::SetLongValue;
    using CSimpleIniA::SetValue;
private:
    std::string m_config_file_name;
};

#endif // SETTINGS_H
