// pipe_impl_minimal.hpp
// Minimal PIMPL implementation for pipe.hpp interface
// HEAD: Decode RAW → scene-linear RGB
// BODY: Passthrough (modules not yet implemented)
// TAIL: Convert to sRGB and save PNG

#pragma once

#include <pipe.hpp>
#include <sink.hpp>
#include <link.hpp>
#include <hold.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace pipe
{
    namespace impl
    {
        // Minimal Data implementation
        class DataImpl : public Data
        {
        public:
            DataImpl(const Info& info, const View& view) : info_(info), view_(view) {}

            Info info() override { return info_; }
            View view() override { return view_; }

        private:
            Info info_;
            View view_;
        };

        // Forward declare for circular dependency
        class TailImpl;

        // Minimal Body implementation (passthrough, no modules yet)
        class BodyImpl : public Body
        {
        public:
            explicit BodyImpl(const View& view, const Info& info)
                : view_(view), info_(info) {}

            Data data() override
            {
                return DataImpl(info_, view_);
            }

            // Empty implementations for module methods (not yet implemented)
            Link add(Name name) override
            {
                throw std::runtime_error("Body::add() not yet implemented");
            }

            Link get(Name name) override
            {
                throw std::runtime_error("Body::get() not yet implemented");
            }

            List& all() override
            {
                throw std::runtime_error("Body::all() not yet implemented");
            }

            Tail tail() override;

        private:
            View view_;
            Info info_;
        };

        // Minimal Tail implementation
        class TailImpl : public Tail
        {
        public:
            explicit TailImpl(const View& view) : view_(view) {}

            void save() override
            {
                // For now, just convert to 8-bit sRGB
                // Proper output transform will be implemented later
                cv::UMat output8bit;

                // Apply simple gamma correction (linear → sRGB approximation)
                // TODO: Proper sRGB OETF
                cv::pow(view_, 1.0/2.2, output8bit);

                // Convert to 8-bit
                output8bit.convertTo(output8bit, CV_8UC3, 255.0);

                // For now, we don't actually write - just prepared
                // Output will be handled by caller
                processedOutput_ = output8bit;
            }

            View getOutput() const { return processedOutput_; }

        private:
            View view_;
            View processedOutput_;
        };

        inline Tail BodyImpl::tail()
        {
            return TailImpl(view_);
        }

        // Minimal Head implementation
        class HeadImpl : public Head
        {
        public:
            explicit HeadImpl(pqtr::Hold<pqtr::Sink> sink)
                : sink_(std::move(sink))
            {
                decode();
            }

            Data data() override
            {
                return DataImpl(info_, view_);
            }

            Body body() override
            {
                return BodyImpl(view_, info_);
            }

        private:
            void decode()
            {
                // Create Link from Sink for decoder
                pqtr::Link link = pqtr::make::sink(sink_.operator->());

                // TODO: Integrate opt/raws decoder here
                // For now, placeholder
                info_["decoder"] = "sony_arw2";
                info_["status"] = "placeholder";
                info_["width"] = "0";
                info_["height"] = "0";

                // Create 1x1 placeholder image
                view_ = cv::UMat(1, 1, CV_32FC3, cv::Scalar(0.5, 0.5, 0.5));
            }

            pqtr::Hold<pqtr::Sink> sink_;
            View view_;
            Info info_;
        };

        // Minimal Pipe implementation
        class PipeImpl : public Pipe
        {
        public:
            pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) override
            {
                return pqtr::Hold<Head>(new HeadImpl(std::move(sink)));
            }
        };

    } // namespace impl

    // Factory function to create Pipe instance
    inline Pipe* createPipe()
    {
        return new impl::PipeImpl();
    }

} // namespace pipe
