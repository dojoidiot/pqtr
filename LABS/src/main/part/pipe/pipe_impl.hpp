// pipe_impl.hpp
// PIMPL implementation for pipe.hpp interface

#pragma once

#include <pipe.hpp>
#include <sink.hpp>
#include <link.hpp>
#include <hold.hpp>
#include <opencv2/core.hpp>
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace pipe
{
    namespace impl
    {
        // Forward declarations
        class PipeImpl;
        class HeadImpl;
        class BodyImpl;
        class TailImpl;
        class LinkImpl;
        class DataImpl;

        // Implementation of Data
        class DataImpl : public Data
        {
        public:
            DataImpl(Info info, View view) : info_(info), view_(view) {}

            Info info() override { return info_; }
            View view() override { return view_; }

        private:
            Info info_;
            View view_;
        };

        // Implementation of Task (base for all processing units)
        class TaskImpl : public Body::Task
        {
        public:
            View run(View view) override
            {
                // Default: passthrough
                return view;
            }

            bool set() override
            {
                return set_;
            }

        protected:
            bool set_ = false;
        };

        // Implementation of Link
        class LinkImpl : public Body::Link
        {
        public:
            explicit LinkImpl(Name name) : name_(name) {}

            Name name() override { return name_; }

            // Module accessors (TODO: implement actual modules)
            Geometric geometric() override;
            ColorCorrection colorCorrection() override;
            ToneMapping toneMapping() override;
            GlobalColor globalColor() override;
            SelectiveColour selectiveColour() override;
            Detail detail() override;

            View run(View view) override
            {
                // Run all modules in sequence
                // TODO: implement module chain
                return view;
            }

            bool set() override
            {
                // Check if any module has been modified
                // TODO: check all modules
                return false;
            }

        private:
            Name name_;
            // TODO: add module instances
        };

        // Implementation of Body
        class BodyImpl : public Body
        {
        public:
            explicit BodyImpl(pqtr::Hold<pqtr::Sink> sink, View view, Info info)
                : sink_(std::move(sink)), currentView_(view), info_(info) {}

            Data data() override
            {
                return DataImpl(info_, currentView_);
            }

            Link add(Name name) override
            {
                auto link = std::make_shared<LinkImpl>(name);
                links_.push_back(link);
                return *link;
            }

            Link get(Name name) override
            {
                for (auto& link : links_)
                {
                    if (link->name() == name)
                    {
                        return *link;
                    }
                }
                throw std::runtime_error("Link not found: " + name);
            }

            class ListImpl : public List
            {
            public:
                explicit ListImpl(const std::vector<std::shared_ptr<LinkImpl>>& links)
                    : links_(links), index_(0) {}

                Link get() override
                {
                    if (has())
                    {
                        return *links_[index_++];
                    }
                    throw std::runtime_error("No more links");
                }

                bool has() override
                {
                    return index_ < links_.size();
                }

            private:
                const std::vector<std::shared_ptr<LinkImpl>>& links_;
                size_t index_;
            };

            List& all() override
            {
                list_ = std::make_unique<ListImpl>(links_);
                return *list_;
            }

            Tail tail() override;

        private:
            pqtr::Hold<pqtr::Sink> sink_;
            View currentView_;
            Info info_;
            std::vector<std::shared_ptr<LinkImpl>> links_;
            std::unique_ptr<ListImpl> list_;
        };

        // Implementation of Tail
        class TailImpl : public Tail
        {
        public:
            explicit TailImpl(View view, pqtr::Hold<pqtr::Sink> outputSink)
                : view_(view), outputSink_(std::move(outputSink)) {}

            void save() override
            {
                // Convert linear RGB to sRGB and save as PNG
                // TODO: Implement output transform and PNG encoding
            }

        private:
            View view_;
            pqtr::Hold<pqtr::Sink> outputSink_;
        };

        // Implementation of Head
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
                // Transfer ownership of sink to body
                return BodyImpl(std::move(sink_), view_, info_);
            }

        private:
            void decode()
            {
                // Decode RAW data from sink
                // TODO: Implement RAW decoding (integrate opt/raws decoder)
                // For now, create a placeholder
                info_["decoder"] = "sony_arw2";
                info_["status"] = "not_implemented";

                // Create empty view as placeholder
                view_ = cv::UMat(1, 1, CV_32FC3);
            }

            pqtr::Hold<pqtr::Sink> sink_;
            View view_;
            Info info_;
        };

        // Implementation of Pipe
        class PipeImpl : public Pipe
        {
        public:
            pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) override
            {
                return pqtr::Hold<Head>(new HeadImpl(std::move(sink)));
            }
        };

    } // namespace impl
} // namespace pipe
