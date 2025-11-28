// pipe.cpp
// PIMPL implementation of pipe.hpp
// HEAD→BODY→TAIL builder pattern
//
// ARCHITECTURE: RAWS provides camera-native RGB + metadata.
// HEAD applies WB + ColorMatrix to convert to scene-linear sRGB.
// This separation ensures:
//   - RAWS = sensor-specific extraction only
//   - LABS = all color science (camera-agnostic)

#include <pipe.hpp>
#include "view.hpp"
#include "link.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <iostream>

// RAW decoder (from RAWS library)
#include "raws.hpp"

namespace pipe
{

using namespace internal;

// ============================================================
// Color Science (applied in HEAD)
// ============================================================

// Apply white balance to camera-native RGB
// WB is multiplicative: R *= wb_r, G *= 1.0, B *= wb_b
static View applyWhiteBalance(View input, float wb_r, float wb_g, float wb_b)
{
    if (input.empty()) return input;

    // Create gain vector for multiply
    cv::Scalar gains(wb_r, wb_g, wb_b);  // RGB order

    View output;
    cv::multiply(input, gains, output);

    std::cout << "  [HEAD] WB applied: R=" << wb_r << " G=" << wb_g << " B=" << wb_b << std::endl;
    return output;
}

// Apply color matrix to convert camera RGB → sRGB
static View applyColorMatrix(View input, const cv::Matx33f& matrix)
{
    if (input.empty()) return input;

    // Check if matrix is identity (skip if so)
    cv::Matx33f identity = cv::Matx33f::eye();
    bool is_identity = true;
    for (int i = 0; i < 9; i++) {
        if (std::abs(matrix.val[i] - identity.val[i]) > 0.001f) {
            is_identity = false;
            break;
        }
    }

    if (is_identity) {
        std::cout << "  [HEAD] ColorMatrix: identity (skipped)" << std::endl;
        return input;
    }

    // Apply matrix: output = matrix * input (per-pixel)
    cv::Mat matrixMat(matrix);
    View output;
    cv::transform(input, output, matrixMat);

    std::cout << "  [HEAD] ColorMatrix applied:" << std::endl;
    std::cout << "    [" << matrix(0,0) << ", " << matrix(0,1) << ", " << matrix(0,2) << "]" << std::endl;
    std::cout << "    [" << matrix(1,0) << ", " << matrix(1,1) << ", " << matrix(1,2) << "]" << std::endl;
    std::cout << "    [" << matrix(2,0) << ", " << matrix(2,1) << ", " << matrix(2,2) << "]" << std::endl;

    return output;
}

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

        View display = toDisplayView(linear);

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

        return toDisplayView(linear);
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

        return toDisplayView(linear, max_dim);
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
        : m_data(std::move(dataInfo), std::move(dataView))
        , m_view(std::move(viewInfo), std::move(viewImage)) {}

    Data& data() override { return m_data; }
    Data& view() override { return m_view; }

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

        // LABS applies color science that RAWS deferred:
        // 1. White Balance (from camera metadata)
        // 2. Color Matrix (camera RGB → sRGB)
        //
        // This keeps RAWS as pure sensor extraction,
        // and LABS handles all color science (camera-agnostic).

        std::cout << "[HEAD] Applying color science from metadata..." << std::endl;

        // Step 1: Apply white balance
        View balanced = applyWhiteBalance(
            raw.data,
            raw.colorMeta.wb_r,
            raw.colorMeta.wb_g,
            raw.colorMeta.wb_b);

        // Step 2: Apply color matrix
        View corrected = applyColorMatrix(balanced, raw.colorMeta.color_matrix);

        // Update color_space in metadata
        raw.dataInfo["color_space"] = "scene_linear_srgb";

        return pqtr::Hold<Head>(new HeadImpl(
            std::move(raw.dataInfo), std::move(corrected),
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
