// link.hpp
// Internal: Module implementations and LinkImpl
// Not a public header - used only within pipe module

#pragma once

#include <pipe.hpp>
#include <memory>
#include <vector>

namespace pipe::internal
{

    // Forward declarations for module implementations
    class GeometricImpl;
    class ColorCorrectionImpl;
    class LutCurveImpl;
    class ToneMappingImpl;
    class GlobalColorImpl;
    class SplitToneImpl;
    class SelectiveColourImpl;
    class DetailImpl;

    // LinkImpl - Holds all module implementations
    // Only runs active modules (those with dials set)
    class LinkImpl : public Body::Link
    {
    public:
        explicit LinkImpl(Name name);
        ~LinkImpl();  // Defined in link.cpp where impl types are complete

        Name name() override;
        Geometric& geometric() override;
        ColorCorrection& colorCorrection() override;
        LutCurve& lutCurve() override;
        ToneMapping& toneMapping() override;
        GlobalColor& globalColor() override;
        SplitTone& splitTone() override;
        SelectiveColour& selectiveColour() override;
        Detail& detail() override;

        View run(View view) override;

    private:
        Name m_name;
        std::unique_ptr<GeometricImpl> m_geometric;
        std::unique_ptr<ColorCorrectionImpl> m_colorCorrection;
        std::unique_ptr<LutCurveImpl> m_lutCurve;
        std::unique_ptr<ToneMappingImpl> m_toneMapping;
        std::unique_ptr<GlobalColorImpl> m_globalColor;
        std::unique_ptr<SplitToneImpl> m_splitTone;
        std::unique_ptr<SelectiveColourImpl> m_selectiveColour;
        std::unique_ptr<DetailImpl> m_detail;
    };

    // IteratorImpl for traversing links
    class IteratorImpl : public Body::Iterator
    {
    public:
        explicit IteratorImpl(std::vector<std::unique_ptr<LinkImpl>>& links);

        Body::Link& current() override;
        bool next() override;
        void reset() override;

    private:
        std::vector<std::unique_ptr<LinkImpl>>& m_links;
        size_t m_index = 0;
    };

} // namespace pipe::internal
