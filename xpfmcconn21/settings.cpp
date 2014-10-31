#include "settings.h"

Settings::Settings(const std::string& filename):
        m_config_file_name(filename)
{
}

Settings::~Settings()
{
    //this->SaveFile(m_config_filename.c_str());
}

bool Settings::loadFromFile()
{
    return (SI_OK == this->LoadFile(m_config_file_name.c_str()));
}

bool Settings::saveToFile()
{
    return (SI_OK == this->SaveFile(m_config_file_name.c_str()));
}
