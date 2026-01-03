// pipe.cpp - Labs namespace implementation
//
// Stem, Flow, and Pipe implementations

#include "pqtr.hpp"
#include <map>
#include <sstream>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/pipe_state.h"
}

namespace pqtr::Labs
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

    class TemperatureImpl : public Flow::Temperature
    {
        PipeState &state_;
    public:
        TemperatureImpl(PipeState &s) : state_(s) {}
        bool enabled() override { return state_.temperature.enabled != 0; }
        void enabled(bool v) override { state_.temperature.enabled = v ? 1 : 0; }
        float coeff(int ch) override { return state_.temperature.coeffs[ch]; }
        void coeff(int ch, float v) override { state_.temperature.coeffs[ch] = v; }
    };

    class ChromaImpl : public Flow::Chroma
    {
        PipeState &state_;
    public:
        ChromaImpl(PipeState &s) : state_(s) {}
        double D65(int ch) override { return state_.chroma.D65coeffs[ch]; }
        void D65(int ch, double v) override { state_.chroma.D65coeffs[ch] = v; }
        double asShot(int ch) override { return state_.chroma.as_shot[ch]; }
        void asShot(int ch, double v) override { state_.chroma.as_shot[ch] = v; }
        bool lateCorrection() override { return state_.chroma.late_correction != 0; }
        void lateCorrection(bool v) override { state_.chroma.late_correction = v ? 1 : 0; }
    };

    class FlowImpl : public Flow
    {
        PipeState state_;                // execution state (C struct)
        TemperatureImpl temp_;
        ChromaImpl chroma_;
        Hold<StemImpl> head_;            // persistence (tree)
        Hold<StemImpl> flow_;
        Hold<StemImpl> tail_;
        std::vector<uint8_t> data_;

    public:
        FlowImpl()
            : state_{}
            , temp_(state_)
            , chroma_(state_)
            , head_(std::make_unique<StemImpl>())
            , flow_(std::make_unique<StemImpl>())
            , tail_(std::make_unique<StemImpl>())
        {
            memset(&state_, 0, sizeof(state_));
        }

        // Image geometry
        int width() override { return state_.width; }
        void width(int v) override { state_.width = v; }
        int height() override { return state_.height; }
        void height(int v) override { state_.height = v; }
        uint32_t filters() override { return state_.filters; }
        void filters(uint32_t v) override { state_.filters = v; }

        // Camera data
        float exposureBias() override { return state_.exposure_bias; }
        void exposureBias(float v) override { state_.exposure_bias = v; }

        // Temperature and chroma
        Temperature &temperature() override { return temp_; }
        Chroma &chroma() override { return chroma_; }

        // Color matrices
        float adobeXYZtoCAM(int row, int col) override { return state_.adobe_XYZ_to_CAM[row][col]; }
        void adobeXYZtoCAM(int row, int col, float v) override { state_.adobe_XYZ_to_CAM[row][col] = v; }
        float d65ColorMatrix(int idx) override { return state_.d65_color_matrix[idx]; }
        void d65ColorMatrix(int idx, float v) override { state_.d65_color_matrix[idx] = v; }

        // Tree persistence
        Stem &head() override { return *head_; }
        Stem &flow() override { return *flow_; }
        Stem &tail() override { return *tail_; }

        // Pixel data buffer
        void *data() override { return data_.data(); }
        void resize(size_t bytes) override { data_.resize(bytes); }

        std::vector<uint8_t> &buffer() { return data_; }

        // C interop - returns internal PipeState for C modules
        PipeState &nativeState() { return state_; }

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

    // Helper to get native state from Flow (for C module interop)
    inline PipeState &nativeState(Flow &flow)
    {
        return static_cast<FlowImpl&>(flow).nativeState();
    }

    // -------------------------------------------------------------------------
    // PipeImpl
    // -------------------------------------------------------------------------

    class PipeImpl : public Pipe
    {
        List<Hold<Head>> heads_;
        List<std::pair<Name, Hold<Step>>> steps_;
        List<Hold<Tail>> tails_;

    public:
        Pipe& head(Hold<Head> head) override
        {
            heads_.push_back(std::move(head));
            return *this;
        }

        Pipe& body(Name name, Hold<Step> step) override
        {
            steps_.emplace_back(std::move(name), std::move(step));
            return *this;
        }

        Pipe& tail(Hold<Tail> tail) override
        {
            tails_.push_back(std::move(tail));
            return *this;
        }

        void *pump(void *data, size_t size) override
        {
            if (heads_.empty())
                return nullptr;

            // Create flow and run heads in order
            auto flow = std::make_unique<FlowImpl>();
            for (auto& h : heads_)
                h->load(*flow, data, size);

            // Run each step
            for (auto &[name, step] : steps_)
                step->exec(*flow);

            // Run tails in order
            for (auto& t : tails_)
                t->save(*flow);

            return flow->data();
        }
    };

} // namespace pqtr::Labs

namespace pqtr
{
    std::unique_ptr<Labs::Pipe> pipe()
    {
        return std::make_unique<Labs::PipeImpl>();
    }
}
