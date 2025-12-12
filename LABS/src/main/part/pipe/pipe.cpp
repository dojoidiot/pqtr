// pipe.cpp
// PIMPL implementation of pipe.hpp
//
// Two APIs:
//   NEW: Pipe with Task chain (add/view/tune)
//   LEGACY: HEAD→BODY→TAIL builder pattern
//
// Pipeline flow:
//   HEAD (decode) → BODY (links process scene-linear) → sigmoid → gamma → display

#include <pipe.hpp>
#include "view.hpp"
#include "link.hpp"
#include "mods/mods.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <sstream>

// RAW decoder (from GEAR library)
#include "gear.hpp"

namespace pipe
{

// ============================================================
// Info - Tree-structured metadata implementation
// ============================================================

Info& Info::node(const Name& name)
{
    auto it = _nodes.find(name);
    if (it == _nodes.end())
    {
        _nodes[name] = std::make_shared<Info>();
        return *_nodes[name];
    }
    return *it->second;
}

const Info* Info::node(const Name& name) const
{
    auto it = _nodes.find(name);
    if (it == _nodes.end()) return nullptr;
    return it->second.get();
}

void Info::set(const Name& key, const std::string& value)
{
    _values[key] = value;
}

std::string Info::get(const Name& key, const std::string& fallback) const
{
    auto it = _values.find(key);
    if (it == _values.end()) return fallback;
    return it->second;
}

bool Info::has(const Name& key) const
{
    return _values.find(key) != _values.end();
}

std::string Info::path(const std::string& dotpath, const std::string& fallback) const
{
    // Split by '.' and traverse
    std::istringstream ss(dotpath);
    std::string segment;
    const Info* current = this;

    std::vector<std::string> parts;
    while (std::getline(ss, segment, '.'))
        parts.push_back(segment);

    if (parts.empty()) return fallback;

    // Navigate to parent node
    for (size_t i = 0; i < parts.size() - 1; ++i)
    {
        current = current->node(parts[i]);
        if (!current) return fallback;
    }

    // Get leaf value
    return current->get(parts.back(), fallback);
}

std::vector<Name> Info::keys() const
{
    std::vector<Name> result;
    result.reserve(_values.size());
    for (const auto& kv : _values)
        result.push_back(kv.first);
    return result;
}

std::vector<Name> Info::children() const
{
    std::vector<Name> result;
    result.reserve(_nodes.size());
    for (const auto& kv : _nodes)
        result.push_back(kv.first);
    return result;
}

void Info::merge(const Info& other)
{
    for (const auto& kv : other._values)
        _values[kv.first] = kv.second;
    for (const auto& kv : other._nodes)
    {
        if (_nodes.find(kv.first) == _nodes.end())
            _nodes[kv.first] = std::make_shared<Info>();
        _nodes[kv.first]->merge(*kv.second);
    }
}

void Info::clear()
{
    _values.clear();
    _nodes.clear();
}

using namespace internal;

// ============================================================
// LegacyDataImpl - Old virtual interface for HEAD/BODY/TAIL
// ============================================================

class LegacyDataImpl : public LegacyData
{
    InfoMap m_info;
    View m_view;

public:
    LegacyDataImpl() = default;
    LegacyDataImpl(InfoMap info, View view) : m_info(std::move(info)), m_view(std::move(view)) {}

    InfoMap info() override { return m_info; }
    View view() override { return m_view; }

    void setView(View view) { m_view = std::move(view); }
};

// ============================================================
// TailImpl - Has access to full-res data and links for export
// ============================================================

class TailImpl : public Tail
{
    LegacyDataImpl& m_full_data;
    std::vector<std::unique_ptr<LinkImpl>>& m_links;

public:
    TailImpl(LegacyDataImpl& full_data, std::vector<std::unique_ptr<LinkImpl>>& links)
        : m_full_data(full_data), m_links(links) {}

    bool save(const std::string& path, int max_dim) override
    {
        View linear = m_full_data.view();

        if (max_dim > 0)
        {
            float scale = (float)max_dim / std::max(linear.cols, linear.rows);
            if (scale < 1.0f)
            {
                View small;
                cv::resize(linear, small, cv::Size(), scale, scale, cv::INTER_AREA);
                linear = small;
            }
        }

        for (auto& link : m_links)
            linear = link->run(linear);

        // Sigmoid: scene→display tone mapping (darktable scene-referred default)
        View tonemapped;
        mods::sigmoid_default(linear, tonemapped);

        View display = toDisplayView(tonemapped);

        // Pipeline is BGR (OpenCV native) - save directly
        cv::Mat cpu;
        display.copyTo(cpu);

        return cv::imwrite(path, cpu);
    }

    View view(int max_dim) override
    {
        View linear = m_full_data.view();

        if (max_dim > 0)
        {
            float scale = (float)max_dim / std::max(linear.cols, linear.rows);
            if (scale < 1.0f)
            {
                View small;
                cv::resize(linear, small, cv::Size(), scale, scale, cv::INTER_AREA);
                linear = small;
            }
        }

        for (auto& link : m_links)
            linear = link->run(linear);

        // Sigmoid: scene→display tone mapping (darktable scene-referred default)
        View tonemapped;
        mods::sigmoid_default(linear, tonemapped);

        return toDisplayView(tonemapped);
    }
};

// ============================================================
// BodyImpl
// ============================================================

class BodyImpl : public Body
{
    LegacyDataImpl& m_working;
    LegacyDataImpl& m_full;
    std::vector<std::unique_ptr<LinkImpl>> m_links;
    std::unique_ptr<IteratorImpl> m_iterator;
    std::unique_ptr<TailImpl> m_tail;

public:
    BodyImpl(LegacyDataImpl& working, LegacyDataImpl& full) : m_working(working), m_full(full) {}

    Link& add(Name name) override
    {
        m_links.push_back(std::make_unique<LinkImpl>(std::move(name)));
        return *m_links.back();
    }

    Link& get(Name name) override
    {
        for (auto& link : m_links)
            if (link->name() == name) return *link;
        throw std::runtime_error("Link not found: " + name);
    }

    Iterator& links() override
    {
        if (!m_iterator) m_iterator = std::make_unique<IteratorImpl>(m_links);
        m_iterator->reset();
        return *m_iterator;
    }

    LegacyData& data() override { return m_working; }

    View view(int max_dim) override
    {
        // Clone to prevent link processing from corrupting the original working data
        // (cv::UMat shares data on copy; link ops may modify in-place)
        View linear;
        m_working.view().copyTo(linear);

        for (auto& link : m_links)
            linear = link->run(linear);

        // Sigmoid: scene→display tone mapping (darktable scene-referred default)
        View tonemapped;
        mods::sigmoid_default(linear, tonemapped);

        return toDisplayView(tonemapped, max_dim);
    }

    Tail& tail() override
    {
        if (!m_tail) m_tail = std::make_unique<TailImpl>(m_full, m_links);
        return *m_tail;
    }
};

// ============================================================
// HeadImpl
// ============================================================

class HeadImpl : public Head
{
    LegacyDataImpl m_data;
    LegacyDataImpl m_view;
    LegacyDataImpl m_working;
    std::unique_ptr<BodyImpl> m_body;
    int m_current_working_size = 0;

public:
    HeadImpl(InfoMap dataInfo, View dataView, InfoMap viewInfo, View viewImage)
        : m_view(std::move(viewInfo), std::move(viewImage))
    {
        // Apply generic camera baseline (works for any camera's scene-linear output)
        // Includes: highlight recovery + exposure boost (+0.7 EV)
        // This bridges the gap from flat scene-linear to "looks good" starting point
        View baselined;
        if (mods::baseline_default(dataView, baselined))
        {
            m_data = LegacyDataImpl(std::move(dataInfo), std::move(baselined));
        }
        else
        {
            // Fallback: use original data if baseline fails
            m_data = LegacyDataImpl(std::move(dataInfo), std::move(dataView));
        }
    }

    LegacyData& data() override { return m_data; }
    LegacyData& view() override { return m_view; }

    // Curve estimation now belongs in LABS, not GEAR
    // Return nullptr/false until LABS implements estimation
    const float* baseCurve() const override { return nullptr; }
    bool hasBaseCurve() const override { return false; }

    Body& body(int working_size) override
    {
        if (!m_body || working_size != m_current_working_size)
        {
            m_current_working_size = working_size;

            View full = m_data.view();

            if (working_size > 0)
            {
                float scale = (float)working_size / std::max(full.cols, full.rows);
                if (scale < 1.0f)
                {
                    View small;
                    cv::resize(full, small, cv::Size(), scale, scale, cv::INTER_AREA);
                    m_working = LegacyDataImpl(m_data.info(), std::move(small));
                }
                else
                {
                    m_working = LegacyDataImpl(m_data.info(), full);
                }
            }
            else
            {
                m_working = LegacyDataImpl(m_data.info(), full);
            }

            m_body = std::make_unique<BodyImpl>(m_working, m_data);
        }
        return *m_body;
    }
};

// ============================================================
// PipeImpl - Task chain + legacy open()
// ============================================================

class PipeImpl : public Pipe
{
    std::vector<pqtr::Hold<Task>> m_tasks;

public:
    // NEW: Task chain interface
    Pipe& add(pqtr::Hold<Task> task) override
    {
        m_tasks.push_back(std::move(task));
        return *this;
    }

    Data view(Data in) override
    {
        Data current = std::move(in);
        for (auto& task : m_tasks)
            current = task->view(std::move(current));
        return current;
    }

    Data tune(Data in) override
    {
        Data current = std::move(in);
        for (auto& task : m_tasks)
            current = task->tune(std::move(current));
        return current;
    }

    size_t size() const override { return m_tasks.size(); }
    Task& at(size_t i) override { return *m_tasks[i]; }

    // LEGACY: HEAD→BODY→TAIL interface
    pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) override
    {
        gear::Result raw = gear::decode(*sink);
        if (!raw.success)
            return pqtr::Hold<Head>(nullptr);

        return pqtr::Hold<Head>(new HeadImpl(
            std::move(raw.dataInfo), std::move(raw.data),
            std::move(raw.previewInfo), std::move(raw.preview)));
    }
};

// ============================================================
// Factory
// ============================================================

pqtr::Hold<Pipe> make()
{
    return pqtr::Hold<Pipe>(new PipeImpl());
}

} // namespace pipe
