// tree.cpp - Flow tree data structure with JSON serialization
//
// LeafImpl, StemImpl, TreeImpl implementations

#include "flow.hpp"
#include <map>
#include <sstream>

namespace flow
{
    using Name = std::string;
    using Text = std::string;

    template <typename T>
    using List = std::vector<T>;

    template <typename T>
    using Hold = std::unique_ptr<T>;

    template <typename K, typename V>
    using Dict = std::map<K, V>;

    // -------------------------------------------------------------------------
    // LeafImpl
    // -------------------------------------------------------------------------

    class LeafImpl : public Leaf
    {
        float dial_ = 0.0f;
        Text text_;
        bool live_ = false;
        bool has_dial_ = false;

    public:
        float dial() override { return dial_; }

        void dial(float val) override
        {
            dial_ = val;
            has_dial_ = true;
            live_ = true;
        }

        Text &text() override
        {
            if (text_.empty() && has_dial_)
            {
                std::ostringstream oss;
                oss << dial_;
                text_ = oss.str();
            }
            return text_;
        }

        void text(Text &val) override
        {
            text_ = val;
            live_ = true;
        }

        bool live() override { return live_; }
    };

    // -------------------------------------------------------------------------
    // StemImpl
    // -------------------------------------------------------------------------

    class StemImpl : public Stem
    {
        Dict<Name, Hold<Node>> children_;

    public:
        Stem &next(const Name &name) override
        {
            auto it = children_.find(name);
            if (it != children_.end())
            {
                if (auto *stem = dynamic_cast<Stem *>(it->second.get()))
                    return *stem;
            }
            auto stem = std::make_unique<StemImpl>();
            Stem &ref = *stem;
            children_[name] = std::move(stem);
            return ref;
        }

        Leaf &leaf(const Name &name) override
        {
            auto it = children_.find(name);
            if (it != children_.end())
            {
                if (auto *leaf = dynamic_cast<Leaf *>(it->second.get()))
                    return *leaf;
            }
            auto leaf = std::make_unique<LeafImpl>();
            Leaf &ref = *leaf;
            children_[name] = std::move(leaf);
            return ref;
        }

        Node *find(const Name &name) override
        {
            auto it = children_.find(name);
            return it != children_.end() ? it->second.get() : nullptr;
        }

        size_t size() const override { return children_.size(); }

        bool test(const Name &name) const override
        {
            return children_.find(name) != children_.end();
        }

        void tidy() override
        {
            for (auto it = children_.begin(); it != children_.end();)
            {
                if (auto *leaf = dynamic_cast<LeafImpl *>(it->second.get()))
                {
                    if (!leaf->live())
                    {
                        it = children_.erase(it);
                        continue;
                    }
                }
                else if (auto *stem = dynamic_cast<StemImpl *>(it->second.get()))
                {
                    stem->tidy();
                    if (stem->size() == 0)
                    {
                        it = children_.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }

        List<Name> list() override
        {
            List<Name> names;
            names.reserve(children_.size());
            for (const auto &[name, _] : children_)
                names.push_back(name);
            return names;
        }
    };

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
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
            }
        }
        return out;
    }

    // -------------------------------------------------------------------------
    // TreeImpl
    // -------------------------------------------------------------------------

    class TreeImpl : public Tree
    {
        Hold<StemImpl> root_;

        void write_json(std::ostringstream &oss, Stem &stem, int indent)
        {
            Text pad(indent * 2, ' ');
            Text pad2((indent + 1) * 2, ' ');
            auto names = stem.list();

            List<Name> leaves, stems;
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
                auto *leaf = dynamic_cast<Leaf *>(stem.find(name));
                oss << pad2 << "\"" << json_escape(name) << "\": ";
                oss << "\"" << json_escape(leaf->text()) << "\"";
                if (++idx < total)
                    oss << ",";
                oss << "\n";
            }

            for (const auto &name : stems)
            {
                auto *child = dynamic_cast<Stem *>(stem.find(name));
                oss << pad2 << "\"" << json_escape(name) << "\": ";
                write_json(oss, *child, indent + 1);
                if (++idx < total)
                    oss << ",";
                oss << "\n";
            }

            oss << pad << "}";
        }

        size_t skip_ws(const Text &json, size_t pos)
        {
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
                ++pos;
            return pos;
        }

        Text parse_string(const Text &json, size_t &pos)
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
                    case 'n':
                        result += '\n';
                        break;
                    case 'r':
                        result += '\r';
                        break;
                    case 't':
                        result += '\t';
                        break;
                    default:
                        result += json[pos];
                    }
                }
                else
                {
                    result += json[pos];
                }
                ++pos;
            }
            if (pos < json.size())
                ++pos;
            return result;
        }

        void parse_object(const Text &json, size_t &pos, Stem &stem)
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

    public:
        TreeImpl() : root_(std::make_unique<StemImpl>()) {}

        Stem &root() override { return *root_; }

        Text json() override
        {
            std::ostringstream oss;
            write_json(oss, *root_, 0);
            return oss.str();
        }

        void read(Text json) override
        {
            root_ = std::make_unique<StemImpl>();
            size_t pos = 0;
            parse_object(json, pos, *root_);
        }
    };

    // Factory for TreeImpl (used by load.cpp)
    std::unique_ptr<Tree> makeTree()
    {
        return std::make_unique<TreeImpl>();
    }

} // namespace flow
