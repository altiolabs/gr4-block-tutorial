#pragma once

#include <algorithm>
#include <cstddef>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

struct TagForwarder : gr::Block<TagForwarder, gr::NoTagPropagation> {
    using Base = gr::Block<TagForwarder, gr::NoTagPropagation>;
    using Base::Base;
    using Description = gr::Doc<"Copy samples and manually forward their tags.">;

    gr::PortIn<float> in;
    gr::PortOut<float> out;
    GR_MAKE_REFLECTABLE(TagForwarder, in, out);

    void start() { _marker_emitted = false; }

    [[nodiscard]] gr::work::Status processBulk(gr::InputSpanLike auto& input,
                                             gr::OutputSpanLike auto& output) {
        std::ranges::copy(input, output.begin());
        if (!_marker_emitted && !output.empty()) {
            output.publishTag(gr::property_map{{"tutorial.marker", true}}, 0UZ);
            _marker_emitted = true;
        }
        for (const auto& [relative_index, map_ref] : input.tags(input.size())) {
            if (relative_index >= 0) {
                output.publishTag(map_ref.get(), static_cast<std::size_t>(relative_index));
            }
        }
        return gr::work::Status::OK;
    }

private:
    bool _marker_emitted{false};
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::TagForwarder)
