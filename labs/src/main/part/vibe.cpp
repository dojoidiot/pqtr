// vibe.cpp - Vibe parameter model implementation
//
// PIMPL implementation of vibe.hpp

#include "pqtr.hpp"
#include <sstream>

namespace pqtr::Vibe
{
    using Text = std::string;

    // =========================================================================
    // QuadImpl - 4-channel storage
    // =========================================================================

    class QuadImpl : public Quad
    {
        float v_[4] = {0, 0, 0, 0};
    public:
        QuadImpl() = default;
        QuadImpl(float r, float g, float b, float l) : v_{r, g, b, l} {}

        float r() override { return v_[0]; }
        void r(float v) override { v_[0] = v; }
        float g() override { return v_[1]; }
        void g(float v) override { v_[1] = v; }
        float b() override { return v_[2]; }
        void b(float v) override { v_[2] = v; }
        float l() override { return v_[3]; }
        void l(float v) override { v_[3] = v; }

        float* data() { return v_; }
    };

    // =========================================================================
    // HighlightsImpl
    // =========================================================================

    class HighlightsImpl : public Highlights
    {
        int mode_ = 2;  // OPPOSED
        float clip_ = 1.0f;
        float strength_ = 1.0f;
        float candidating_ = 0.4f;
        float combine_ = 2.0f;

        class BlendImpl : public Blend
        {
            float L_ = 1.0f;
            float C_ = 0.0f;
        public:
            float L() override { return L_; }
            void L(float v) override { L_ = v; }
            float C() override { return C_; }
            void C(float v) override { C_ = v; }
        } blend_;

        class InpaintImpl : public Inpaint
        {
            float noiseLevel_ = 0.0f;
            float solidColor_ = 0.0f;
            int iterations_ = 30;
            int scales_ = 6;
        public:
            float noiseLevel() override { return noiseLevel_; }
            void noiseLevel(float v) override { noiseLevel_ = v; }
            float solidColor() override { return solidColor_; }
            void solidColor(float v) override { solidColor_ = v; }
            int iterations() override { return iterations_; }
            void iterations(int v) override { iterations_ = v; }
            int scales() override { return scales_; }
            void scales(int v) override { scales_ = v; }
        } inpaint_;

    public:
        int mode() override { return mode_; }
        void mode(int v) override { mode_ = v; }
        float clip() override { return clip_; }
        void clip(float v) override { clip_ = v; }
        float strength() override { return strength_; }
        void strength(float v) override { strength_ = v; }
        float candidating() override { return candidating_; }
        void candidating(float v) override { candidating_ = v; }
        float combine() override { return combine_; }
        void combine(float v) override { combine_ = v; }

        Blend& blend() override { return blend_; }
        Inpaint& inpaint() override { return inpaint_; }
    };

    // =========================================================================
    // DemosaicImpl
    // =========================================================================

    class DemosaicImpl : public Demosaic
    {
        int method_ = 5;  // RCD

        class OptsImpl : public Opts
        {
            int greenEq_ = 0;
            int colorSmoothing_ = 0;
            int lmmseRefine_ = 1;
        public:
            int greenEq() override { return greenEq_; }
            void greenEq(int v) override { greenEq_ = v; }
            int colorSmoothing() override { return colorSmoothing_; }
            void colorSmoothing(int v) override { colorSmoothing_ = v; }
            int lmmseRefine() override { return lmmseRefine_; }
            void lmmseRefine(int v) override { lmmseRefine_ = v; }
        } opts_;

        class ThrsImpl : public Thrs
        {
            float median_ = 0.0f;
            float dual_ = 0.2f;
        public:
            float median() override { return median_; }
            void median(float v) override { median_ = v; }
            float dual() override { return dual_; }
            void dual(float v) override { dual_ = v; }
        } thrs_;

        class ChromaImpl : public Chroma
        {
            int enabled_ = 0;
            int iter_ = 8;
            float radius_ = 0.0f;
            float thrs_ = 0.40f;
            float boost_ = 0.0f;
            float center_ = 0.0f;
        public:
            int enabled() override { return enabled_; }
            void enabled(int v) override { enabled_ = v; }
            int iter() override { return iter_; }
            void iter(int v) override { iter_ = v; }
            float radius() override { return radius_; }
            void radius(float v) override { radius_ = v; }
            float thrs() override { return thrs_; }
            void thrs(float v) override { thrs_ = v; }
            float boost() override { return boost_; }
            void boost(float v) override { boost_ = v; }
            float center() override { return center_; }
            void center(float v) override { center_ = v; }
        } chroma_;

    public:
        int method() override { return method_; }
        void method(int v) override { method_ = v; }
        Opts& opts() override { return opts_; }
        Thrs& thrs() override { return thrs_; }
        Chroma& chroma() override { return chroma_; }
    };

    // =========================================================================
    // ExposureImpl
    // =========================================================================

    class ExposureImpl : public Exposure
    {
        int mode_ = 0;
        int compensateBias_ = 0;
        float black_ = 0.0f;
        float ev_ = 0.0f;
    public:
        int mode() override { return mode_; }
        void mode(int v) override { mode_ = v; }
        int compensateBias() override { return compensateBias_; }
        void compensateBias(int v) override { compensateBias_ = v; }
        float black() override { return black_; }
        void black(float v) override { black_ = v; }
        float ev() override { return ev_; }
        void ev(float v) override { ev_ = v; }
    };

    // =========================================================================
    // ChannelMixerImpl
    // =========================================================================

    class ChannelMixerImpl : public ChannelMixer
    {
        int adaptation_ = 0;  // LINEAR_BRADFORD
        float p_ = 1.0f;
        float gamut_ = 1.0f;
        int clip_ = 1;
        int applyGrey_ = 0;

        QuadImpl illuminant_{0.941238f, 1.040633f, 1.088791f, 0.0f};  // D65
        QuadImpl saturation_{0, 0, 0, 0};
        QuadImpl lightness_{0, 0, 0, 0};
        QuadImpl grey_{0, 0, 0, 0};

        class MixImpl : public Mix
        {
            QuadImpl red_{1, 0, 0, 0};
            QuadImpl green_{0, 1, 0, 0};
            QuadImpl blue_{0, 0, 1, 0};
        public:
            Quad& red() override { return red_; }
            Quad& green() override { return green_; }
            Quad& blue() override { return blue_; }
        } mix_;

    public:
        int adaptation() override { return adaptation_; }
        void adaptation(int v) override { adaptation_ = v; }
        float p() override { return p_; }
        void p(float v) override { p_ = v; }
        float gamut() override { return gamut_; }
        void gamut(float v) override { gamut_ = v; }
        int clip() override { return clip_; }
        void clip(int v) override { clip_ = v; }
        int applyGrey() override { return applyGrey_; }
        void applyGrey(int v) override { applyGrey_ = v; }

        Quad& illuminant() override { return illuminant_; }
        Quad& saturation() override { return saturation_; }
        Quad& lightness() override { return lightness_; }
        Quad& grey() override { return grey_; }
        Mix& mix() override { return mix_; }
    };

    // =========================================================================
    // ColorBalanceImpl
    // =========================================================================

    class ColorBalanceImpl : public ColorBalance
    {
        float contrast_ = 1.0f;
        float hueAngle_ = 0.0f;

        class GlobalImpl : public Global
        {
            float chroma_ = 0.0f;
            float vibrance_ = 0.0f;
            float saturation_ = 0.0f;
            float brilliance_ = 0.0f;
        public:
            float chroma() override { return chroma_; }
            void chroma(float v) override { chroma_ = v; }
            float vibrance() override { return vibrance_; }
            void vibrance(float v) override { vibrance_ = v; }
            float saturation() override { return saturation_; }
            void saturation(float v) override { saturation_ = v; }
            float brilliance() override { return brilliance_; }
            void brilliance(float v) override { brilliance_ = v; }
        } global_;

        class WeightImpl : public Weight
        {
            float shadows_ = 4.0f;
            float midtones_ = 8.0f;
            float highlights_ = 4.0f;
        public:
            float shadows() override { return shadows_; }
            void shadows(float v) override { shadows_ = v; }
            float midtones() override { return midtones_; }
            void midtones(float v) override { midtones_ = v; }
            float highlights() override { return highlights_; }
            void highlights(float v) override { highlights_ = v; }
        } weight_;

        class FulcrumImpl : public Fulcrum
        {
            float grey_ = 0.184499994f;
            float white_ = 1.0f;
            float maskGrey_ = 0.5f;
        public:
            float grey() override { return grey_; }
            void grey(float v) override { grey_ = v; }
            float white() override { return white_; }
            void white(float v) override { white_ = v; }
            float maskGrey() override { return maskGrey_; }
            void maskGrey(float v) override { maskGrey_ = v; }
        } fulcrum_;

        QuadImpl shadows_{0.999999881f, 1.0f, 1.0f, 1.0f};
        QuadImpl midtones_{1.000000119f, 1.0f, 1.0f, 1.0f};
        QuadImpl highlights_{0.999999881f, 1.0f, 1.0f, 1.0f};
        float midtonesY_ = 1.0f;

        QuadImpl chroma_{0, 0, 0, 0};
        QuadImpl saturation_{0, 0, 0, 0};
        QuadImpl brilliance_{0, 0, 0, 0};

    public:
        float contrast() override { return contrast_; }
        void contrast(float v) override { contrast_ = v; }
        float hueAngle() override { return hueAngle_; }
        void hueAngle(float v) override { hueAngle_ = v; }

        Global& global() override { return global_; }
        Weight& weight() override { return weight_; }
        Fulcrum& fulcrum() override { return fulcrum_; }

        Quad& shadows() override { return shadows_; }
        Quad& midtones() override { return midtones_; }
        Quad& highlights() override { return highlights_; }
        float midtonesY() override { return midtonesY_; }
        void midtonesY(float v) override { midtonesY_ = v; }

        Quad& chroma() override { return chroma_; }
        Quad& saturation() override { return saturation_; }
        Quad& brilliance() override { return brilliance_; }
    };

    // =========================================================================
    // FilmicImpl
    // =========================================================================

    class FilmicImpl : public Filmic
    {
        float contrast_ = 1.499999762f;
        float saturation_ = 0.0f;

        class SourceImpl : public Source
        {
            float grey_ = 0.184499994f;
            float black_ = -5.0f;
            float white_ = 0.0f;
            float dynamicRange_ = 8.199999809f;
        public:
            float grey() override { return grey_; }
            void grey(float v) override { grey_ = v; }
            float black() override { return black_; }
            void black(float v) override { black_ = v; }
            float white() override { return white_; }
            void white(float v) override { white_ = v; }
            float dynamicRange() override { return dynamicRange_; }
            void dynamicRange(float v) override { dynamicRange_ = v; }
        } source_;

        class DisplayImpl : public Display
        {
            float normalize_ = 9.436863899f;
            float outputPower_ = 3.416451693f;
        public:
            float normalize() override { return normalize_; }
            void normalize(float v) override { normalize_ = v; }
            float outputPower() override { return outputPower_; }
            void outputPower(float v) override { outputPower_ = v; }
        } display_;

        class SigmaImpl : public Sigma
        {
            float toe_ = 0.041306622f;
            float shoulder_ = 0.016918922f;
        public:
            float toe() override { return toe_; }
            void toe(float v) override { toe_ = v; }
            float shoulder() override { return shoulder_; }
            void shoulder(float v) override { shoulder_ = v; }
        } sigma_;

    public:
        float contrast() override { return contrast_; }
        void contrast(float v) override { contrast_ = v; }
        float saturation() override { return saturation_; }
        void saturation(float v) override { saturation_ = v; }

        Source& source() override { return source_; }
        Display& display() override { return display_; }
        Sigma& sigma() override { return sigma_; }
    };

    // =========================================================================
    // BilatImpl
    // =========================================================================

    class BilatImpl : public Bilat
    {
        int mode_ = 1;  // local_laplacian
        float detail_ = 0.0f;
        float midtone_ = 0.2f;

        class SigmaImpl : public Sigma
        {
            float r_ = 0.5f;
            float s_ = 0.5f;
        public:
            float r() override { return r_; }
            void r(float v) override { r_ = v; }
            float s() override { return s_; }
            void s(float v) override { s_ = v; }
        } sigma_;

    public:
        int mode() override { return mode_; }
        void mode(int v) override { mode_ = v; }
        float detail() override { return detail_; }
        void detail(float v) override { detail_ = v; }
        float midtone() override { return midtone_; }
        void midtone(float v) override { midtone_ = v; }

        Sigma& sigma() override { return sigma_; }
    };

    // =========================================================================
    // VibeImpl
    // =========================================================================

    class VibeImpl : public Vibe
    {
        Labs::Pipe& pipe_;

        HighlightsImpl highlights_;
        DemosaicImpl demosaic_;
        ExposureImpl exposure_;
        ChannelMixerImpl channelmixer_;
        ColorBalanceImpl colorbalance_;
        FilmicImpl filmic_;
        BilatImpl bilat_;

    public:
        explicit VibeImpl(Labs::Pipe& pipe) : pipe_(pipe) {}

        Highlights& highlights() override { return highlights_; }
        Demosaic& demosaic() override { return demosaic_; }
        Exposure& exposure() override { return exposure_; }
        ChannelMixer& channelmixer() override { return channelmixer_; }
        ColorBalance& colorbalance() override { return colorbalance_; }
        Filmic& filmic() override { return filmic_; }
        Bilat& bilat() override { return bilat_; }

        Text json() const override
        {
            std::ostringstream o;
            o << "{\n";

            // Highlights
            o << "  \"highlights\": {\n";
            o << "    \"mode\": " << const_cast<HighlightsImpl&>(highlights_).mode() << ",\n";
            o << "    \"clip\": " << const_cast<HighlightsImpl&>(highlights_).clip() << ",\n";
            o << "    \"strength\": " << const_cast<HighlightsImpl&>(highlights_).strength() << "\n";
            o << "  },\n";

            // Exposure
            o << "  \"exposure\": {\n";
            o << "    \"ev\": " << const_cast<ExposureImpl&>(exposure_).ev() << ",\n";
            o << "    \"black\": " << const_cast<ExposureImpl&>(exposure_).black() << "\n";
            o << "  },\n";

            // Filmic
            o << "  \"filmic\": {\n";
            o << "    \"contrast\": " << const_cast<FilmicImpl&>(filmic_).contrast() << ",\n";
            o << "    \"saturation\": " << const_cast<FilmicImpl&>(filmic_).saturation() << ",\n";
            o << "    \"source\": {\n";
            o << "      \"grey\": " << const_cast<FilmicImpl&>(filmic_).source().grey() << ",\n";
            o << "      \"black\": " << const_cast<FilmicImpl&>(filmic_).source().black() << ",\n";
            o << "      \"white\": " << const_cast<FilmicImpl&>(filmic_).source().white() << ",\n";
            o << "      \"dynamicRange\": " << const_cast<FilmicImpl&>(filmic_).source().dynamicRange() << "\n";
            o << "    }\n";
            o << "  },\n";

            // ColorBalance
            o << "  \"colorbalance\": {\n";
            o << "    \"contrast\": " << const_cast<ColorBalanceImpl&>(colorbalance_).contrast() << ",\n";
            o << "    \"global\": {\n";
            o << "      \"chroma\": " << const_cast<ColorBalanceImpl&>(colorbalance_).global().chroma() << ",\n";
            o << "      \"vibrance\": " << const_cast<ColorBalanceImpl&>(colorbalance_).global().vibrance() << ",\n";
            o << "      \"saturation\": " << const_cast<ColorBalanceImpl&>(colorbalance_).global().saturation() << "\n";
            o << "    }\n";
            o << "  },\n";

            // Bilat
            o << "  \"bilat\": {\n";
            o << "    \"mode\": " << const_cast<BilatImpl&>(bilat_).mode() << ",\n";
            o << "    \"detail\": " << const_cast<BilatImpl&>(bilat_).detail() << "\n";
            o << "  }\n";

            o << "}";
            return o.str();
        }

        void load(const Text& json) override
        {
            // TODO: parse JSON and set values
            (void)json;
        }

        void reset() override
        {
            highlights_ = HighlightsImpl();
            demosaic_ = DemosaicImpl();
            exposure_ = ExposureImpl();
            channelmixer_ = ChannelMixerImpl();
            colorbalance_ = ColorBalanceImpl();
            filmic_ = FilmicImpl();
            bilat_ = BilatImpl();
        }
    };

}  // namespace pqtr::Vibe

namespace pqtr
{
    std::unique_ptr<Vibe::Vibe> vibe(Labs::Pipe& pipe)
    {
        return std::make_unique<Vibe::VibeImpl>(pipe);
    }
}
