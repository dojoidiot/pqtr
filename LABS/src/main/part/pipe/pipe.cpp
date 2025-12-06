// pipe.cpp
// PIMPL implementation of pipe.hpp
// HEAD→BODY→TAIL builder pattern
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

// RAW decoder (from RAWS library)
#include "raws.hpp"

namespace pipe
{

using namespace internal;

// ============================================================
// DataImpl
// ============================================================

class DataImpl : public Data
{
    Info m_info;
    View m_view;

public:
    DataImpl() = default;
    DataImpl(Info info, View view) : m_info(std::move(info)), m_view(std::move(view)) {}

    Info info() override { return m_info; }
    View view() override { return m_view; }

    void setView(View view) { m_view = std::move(view); }
};

// ============================================================
// TailImpl - Has access to full-res data and links for export
// ============================================================

class TailImpl : public Tail
{
    DataImpl& m_full_data;
    std::vector<std::unique_ptr<LinkImpl>>& m_links;

public:
    TailImpl(DataImpl& full_data, std::vector<std::unique_ptr<LinkImpl>>& links)
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
    DataImpl& m_working;
    DataImpl& m_full;
    std::vector<std::unique_ptr<LinkImpl>> m_links;
    std::unique_ptr<IteratorImpl> m_iterator;
    std::unique_ptr<TailImpl> m_tail;

public:
    BodyImpl(DataImpl& working, DataImpl& full) : m_working(working), m_full(full) {}

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

    Data& data() override { return m_working; }

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
    DataImpl m_data;
    DataImpl m_view;
    DataImpl m_working;
    std::unique_ptr<BodyImpl> m_body;
    int m_current_working_size = 0;

public:
    HeadImpl(Info dataInfo, View dataView, Info viewInfo, View viewImage)
        : m_view(std::move(viewInfo), std::move(viewImage))
    {
        // Apply generic camera baseline (works for any camera's scene-linear output)
        // Includes: highlight recovery + exposure boost (+0.7 EV)
        // This bridges the gap from flat scene-linear to "looks good" starting point
        View baselined;
        if (mods::baseline_default(dataView, baselined))
        {
            m_data = DataImpl(std::move(dataInfo), std::move(baselined));
        }
        else
        {
            // Fallback: use original data if baseline fails
            m_data = DataImpl(std::move(dataInfo), std::move(dataView));
        }
    }

    Data& data() override { return m_data; }
    Data& view() override { return m_view; }

    // Curve estimation now belongs in LABS, not RAWS
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
                    m_working = DataImpl(m_data.info(), std::move(small));
                }
                else
                {
                    m_working = DataImpl(m_data.info(), full);
                }
            }
            else
            {
                m_working = DataImpl(m_data.info(), full);
            }

            m_body = std::make_unique<BodyImpl>(m_working, m_data);
        }
        return *m_body;
    }
};

// ============================================================
// PipeImpl
// ============================================================

class PipeImpl : public Pipe
{
public:
    pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) override
    {
        raws::Result raw = raws::decode(*sink);
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
