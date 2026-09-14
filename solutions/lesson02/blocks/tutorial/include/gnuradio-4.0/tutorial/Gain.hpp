#pragma once

#include <cstdint>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/meta/utils.hpp>

namespace gr::tutorial {

template<typename T>
struct Gain : gr::Block<Gain<T>> {
    using gr::Block<Gain<T>>::Block;
    using Description = gr::Doc<"Multiply each input sample by two.">;

    gr::PortIn<T> in;
    gr::PortOut<T> out;
    T factor = T{2};
    GR_MAKE_REFLECTABLE(Gain, in, out);

    template<gr::meta::t_or_simd<T> V>
    [[nodiscard]] constexpr V processOne(const V& input) const noexcept {
        return static_cast<V>(input * factor);
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Gain, [T], [float, std::int16_t, std::int32_t, std::uint8_t])
