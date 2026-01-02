// pipe.cpp - Labs namespace implementation
//
// Stem, Flow, and Pipe implementations

#include "labs.hpp"
#include <map>
#include <sstream>

namespace pqtr
{
    using Name = std::string;
    using Text = std::string;

    template <typename T>
    using List = std::vector<T>;

    template <typename T>
    using Hold = std::unique_ptr<T>;

    template <typename K, typename V>
    using Dict = std::map<K, V>;

    // Forward declarations from json.cpp
    Text stem_to_json(Stem &stem, int indent);
    void json_to_stem(const Text &json, Stem &stem);

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
                if (auto *lf = dynamic_cast<Leaf *>(it->second.get()))
                    return *lf;
            }
            auto lf = std::make_unique<LeafImpl>();
            Leaf &ref = *lf;
            children_[name] = std::move(lf);
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
                if (auto *lf = dynamic_cast<LeafImpl *>(it->second.get()))
                {
                    if (!lf->live())
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
    // FlowImpl
    // -------------------------------------------------------------------------

    class FlowImpl : public Flow
    {
        Hold<StemImpl> head_;
        Hold<StemImpl> flow_;
        Hold<StemImpl> tail_;
        std::vector<uint8_t> data_;

    public:
        FlowImpl()
            : head_(std::make_unique<StemImpl>())
            , flow_(std::make_unique<StemImpl>())
            , tail_(std::make_unique<StemImpl>())
        {}

        Stem &head() override { return *head_; }
        Stem &flow() override { return *flow_; }
        Stem &tail() override { return *tail_; }
        void *data() override { return data_.data(); }

        std::vector<uint8_t> &buffer() { return data_; }

        Text json() override
        {
            std::ostringstream oss;
            oss << "{\n";
            oss << "  \"head\": ";
            oss << stem_to_json(*head_, 1);
            oss << ",\n  \"flow\": ";
            oss << stem_to_json(*flow_, 1);
            oss << ",\n  \"tail\": ";
            oss << stem_to_json(*tail_, 1);
            oss << "\n}";
            return oss.str();
        }

        void read(Text json) override
        {
            head_ = std::make_unique<StemImpl>();
            flow_ = std::make_unique<StemImpl>();
            tail_ = std::make_unique<StemImpl>();

            // Simple top-level parse for head/flow/tail
            size_t pos = 0;
            while (pos < json.size() && json[pos] != '{') ++pos;
            if (pos >= json.size()) return;
            ++pos;

            while (pos < json.size())
            {
                // Skip whitespace
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
                    ++pos;
                if (pos >= json.size() || json[pos] == '}') break;

                // Parse key
                if (json[pos] != '"') break;
                ++pos;
                Text key;
                while (pos < json.size() && json[pos] != '"')
                    key += json[pos++];
                if (pos < json.size()) ++pos;

                // Skip to colon
                while (pos < json.size() && json[pos] != ':') ++pos;
                if (pos >= json.size()) break;
                ++pos;

                // Skip whitespace
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
                    ++pos;

                // Find matching brace for nested object
                if (pos < json.size() && json[pos] == '{')
                {
                    size_t start = pos;
                    int depth = 1;
                    ++pos;
                    while (pos < json.size() && depth > 0)
                    {
                        if (json[pos] == '{') ++depth;
                        else if (json[pos] == '}') --depth;
                        ++pos;
                    }
                    Text nested = json.substr(start, pos - start);

                    if (key == "head")
                        json_to_stem(nested, *head_);
                    else if (key == "flow")
                        json_to_stem(nested, *flow_);
                    else if (key == "tail")
                        json_to_stem(nested, *tail_);
                }

                // Skip comma
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t' || json[pos] == ','))
                    ++pos;
            }
        }
    };

    // -------------------------------------------------------------------------
    // PipeImpl
    // -------------------------------------------------------------------------

    class PipeImpl : public Pipe
    {
        Hold<Head> head_;
        Hold<Tail> tail_;
        List<std::pair<Name, Hold<Step>>> steps_;

    public:
        void join(Hold<Head> head, Hold<Tail> tail) override
        {
            head_ = std::move(head);
            tail_ = std::move(tail);
        }

        void join(Name name, Hold<Step> step) override
        {
            steps_.emplace_back(std::move(name), std::move(step));
        }

        void *pump(void *data, size_t size) override
        {
            if (!head_)
                return nullptr;

            // Create flow and load raw data
            auto flow = std::make_unique<FlowImpl>();
            if (!head_->load(*flow, data, size))
                return nullptr;

            // Run each step
            for (auto &[name, step] : steps_)
            {
                step->exec(*flow);
            }

            // Save and return result (if tail exists)
            if (tail_)
                return tail_->save(*flow);

            return flow->data();  // Return flow data if no tail
        }
    };

    // -------------------------------------------------------------------------
    // Factory
    // -------------------------------------------------------------------------

    std::unique_ptr<Pipe> make()
    {
        return std::make_unique<PipeImpl>();
    }

    std::unique_ptr<Flow> makeFlow()
    {
        return std::make_unique<FlowImpl>();
    }

} // namespace pqtr
