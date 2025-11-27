// data.cpp
// Data persistence implementation for LABS

#include <data.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace data
{
    namespace diff
    {
        std::string toJson(const ::diff::Data& d)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6);
            ss << "{\n";
            ss << "  \"spectral\": " << d.spectral << ",\n";
            ss << "  \"frequency\": " << d.frequency << "\n";
            ss << "}";
            return ss.str();
        }

        ::diff::Data fromJson(const std::string& json)
        {
            ::diff::Data d;
            // Simple parser - look for "spectral": and "frequency":
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

        bool save(const ::diff::Data& d, const std::string& path)
        {
            std::ofstream file(path);
            if (!file.is_open())
                return false;
            file << toJson(d);
            file.close();
            return true;
        }

        ::diff::Data load(const std::string& path)
        {
            std::ifstream file(path);
            if (!file.is_open())
                return ::diff::Data{};
            std::stringstream buffer;
            buffer << file.rdbuf();
            return fromJson(buffer.str());
        }

    } // namespace diff

} // namespace data
