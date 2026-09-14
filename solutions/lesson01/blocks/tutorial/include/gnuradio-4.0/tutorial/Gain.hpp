#pragma once

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

struct Gain : gr::Block<Gain> {
    using gr::Block<Gain>::Block;
    using Description = gr::Doc<"Multiply each input sample by two.">;

    gr::PortIn<float> in;
    gr::PortOut<float> out;
    float factor = 2.0f;
    GR_MAKE_REFLECTABLE(Gain, in, out);

    [[nodiscard]] constexpr float processOne(float input) const noexcept {
        return input * factor;
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Gain)
