#pragma once

#include <cmath>
#include <complex>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

template<typename T>
struct Square : gr::Block<Square<T>> {
    using gr::Block<Square<T>>::Block;
    using Description = gr::Doc<"Square the input and apply a gain in dB.">;

    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::Annotated<float, "gain_db", gr::Doc<"Gain applied after squaring">,
                  gr::Unit<"dB">> gain_db{0.0f};

    GR_MAKE_REFLECTABLE(Square, in, out, gain_db);

    void settingsChanged(const gr::property_map&,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("gain_db")) {
            _gain_linear = std::pow(10.0f, gain_db.value / 20.0f);
        }
    }

    [[nodiscard]] T processOne(T input) const noexcept {
        return _gain_linear * input * input;
    }

private:
    float _gain_linear{1.0f};
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Square, [T], [float, std::complex<float>])
