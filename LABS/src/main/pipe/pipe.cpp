// pipe.cpp - Pipe implementation
//
// Simple link chain: Pipe manages Links, each transforms Flow.
// Flow = Data (GPU context) + Info (metadata tree)

#include <pipe.hpp>

namespace pipe {

// ============================================================
// PipeImpl - Link chain
// ============================================================

class PipeImpl : public Pipe {
    List<Hold<Link>> m_links;

public:
    Pipe& link(Hold<Link> lk) override {
        m_links.push_back(std::move(lk));
        return *this;
    }

    Flow flow(Flow in) override {
        Flow current = std::move(in);
        for (auto& lk : m_links) {
            current = lk->flow(std::move(current));
        }
        return current;
    }

    size_t size() const override {
        return m_links.size();
    }

    Link& link(size_t i) override {
        return *m_links[i];
    }
};

// ============================================================
// Factory
// ============================================================

Hold<Pipe> make() {
    return Hold<Pipe>(new PipeImpl());
}

} // namespace pipe
