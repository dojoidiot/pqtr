// json.cpp - JSON serialization/deserialization for Flow
//
// Provides stem_to_json() and json_to_stem() functions

#include "labs.hpp"
#include <sstream>

namespace pqtr
{
    using Text = std::string;

    // -------------------------------------------------------------------------
    // JSON helpers
    // -------------------------------------------------------------------------

    static Text json_escape(const Text &s)
    {
        Text out;
        out.reserve(s.size() + 8);
        for (char c : s)
        {
            switch (c)
            {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
            }
        }
        return out;
    }

    static void write_stem_json(std::ostringstream &oss, Stem &stem, int indent)
    {
        Text pad(indent * 2, ' ');
        Text pad2((indent + 1) * 2, ' ');
        auto names = stem.list();

        std::vector<Text> leaves, stems;
        for (const auto &name : names)
        {
            Node *node = stem.find(name);
            if (dynamic_cast<Leaf *>(node))
                leaves.push_back(name);
            else
                stems.push_back(name);
        }

        oss << "{\n";
        size_t total = leaves.size() + stems.size();
        size_t idx = 0;

        for (const auto &name : leaves)
        {
            auto *lf = dynamic_cast<Leaf *>(stem.find(name));
            oss << pad2 << "\"" << json_escape(name) << "\": ";
            oss << "\"" << json_escape(lf->text()) << "\"";
            if (++idx < total) oss << ",";
            oss << "\n";
        }

        for (const auto &name : stems)
        {
            auto *child = dynamic_cast<Stem *>(stem.find(name));
            oss << pad2 << "\"" << json_escape(name) << "\": ";
            write_stem_json(oss, *child, indent + 1);
            if (++idx < total) oss << ",";
            oss << "\n";
        }

        oss << pad << "}";
    }

    static size_t skip_ws(const Text &json, size_t pos)
    {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
            ++pos;
        return pos;
    }

    static Text parse_string(const Text &json, size_t &pos)
    {
        if (pos >= json.size() || json[pos] != '"')
            return "";
        ++pos;
        Text result;
        while (pos < json.size() && json[pos] != '"')
        {
            if (json[pos] == '\\' && pos + 1 < json.size())
            {
                ++pos;
                switch (json[pos])
                {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += json[pos];
                }
            }
            else
            {
                result += json[pos];
            }
            ++pos;
        }
        if (pos < json.size()) ++pos;
        return result;
    }

    static void parse_object(const Text &json, size_t &pos, Stem &stem)
    {
        pos = skip_ws(json, pos);
        if (pos >= json.size() || json[pos] != '{')
            return;
        ++pos;

        while (true)
        {
            pos = skip_ws(json, pos);
            if (pos >= json.size() || json[pos] == '}')
                break;

            Text key = parse_string(json, pos);
            if (key.empty())
                break;

            pos = skip_ws(json, pos);
            if (pos >= json.size() || json[pos] != ':')
                break;
            ++pos;

            pos = skip_ws(json, pos);
            if (pos >= json.size())
                break;

            if (json[pos] == '{')
            {
                parse_object(json, pos, stem.next(key));
            }
            else if (json[pos] == '"')
            {
                Text val = parse_string(json, pos);
                stem.leaf(key).text(val);
            }
            else
            {
                while (pos < json.size() && json[pos] != ',' && json[pos] != '}')
                    ++pos;
            }

            pos = skip_ws(json, pos);
            if (pos < json.size() && json[pos] == ',')
                ++pos;
        }

        if (pos < json.size() && json[pos] == '}')
            ++pos;
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    Text stem_to_json(Stem &stem, int indent)
    {
        std::ostringstream oss;
        write_stem_json(oss, stem, indent);
        return oss.str();
    }

    void json_to_stem(const Text &json, Stem &stem)
    {
        size_t pos = 0;
        parse_object(json, pos, stem);
    }

} // namespace pqtr
