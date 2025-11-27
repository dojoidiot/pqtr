// data.cpp
// Serialization for tune data types

#include <data.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace data::tune
{

    std::string toJson(const ::tune::Data& d)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << "{\n";
        ss << "  \"spectral\": " << d.spectral << ",\n";
        ss << "  \"frequency\": " << d.frequency << "\n";
        ss << "}";
        return ss.str();
    }

    ::tune::Data fromJson(const std::string& json)
    {
        ::tune::Data d;
        size_t pos = json.find("\"spectral\"");
        if (pos != std::string::npos)
        {
            pos = json.find(':', pos);
            if (pos != std::string::npos)
                d.spectral = std::stof(json.substr(pos + 1));
        }
        pos = json.find("\"frequency\"");
        if (pos != std::string::npos)
        {
            pos = json.find(':', pos);
            if (pos != std::string::npos)
                d.frequency = std::stof(json.substr(pos + 1));
        }
        return d;
    }

    bool save(const ::tune::Data& d, const std::string& path)
    {
        std::ofstream file(path);
        if (!file.is_open())
            return false;
        file << toJson(d);
        file.close();
        return true;
    }

    ::tune::Data load(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
            return ::tune::Data{};
        std::stringstream buffer;
        buffer << file.rdbuf();
        return fromJson(buffer.str());
    }

} // namespace data::tune
