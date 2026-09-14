#pragma once

#include <complex>
#include <cstddef>
#include <vector>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

template<typename T>
struct MovingAverage : gr::Block<MovingAverage<T>, gr::NoTagPropagation> {
    using Base = gr::Block<MovingAverage<T>, gr::NoTagPropagation>;
    using Base::Base;
    using Description = gr::Doc<"Average a full window of recent samples.">;

    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::Annotated<gr::Size_t, "window_size", gr::Limits<1U, 1024U>> window_size{3U};
    GR_MAKE_REFLECTABLE(MovingAverage, in, out, window_size);

    void start() { resetState(); }

    void settingsChanged(const gr::property_map& old_settings,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("window_size") &&
            old_settings.at("window_size") != new_settings.at("window_size")) {
            resetState();
        }
    }

    [[nodiscard]] gr::work::Status processBulk(gr::InputSpanLike auto& input,
                                             gr::OutputSpanLike auto& output) {
        const auto window = _history.size();
        std::size_t n_consumed = 0;
        std::size_t n_produced = 0;
        while (n_consumed < input.size()) {
            const bool will_produce = _fill + 1 >= window;
            if (will_produce && n_produced == output.size()) {
                break;
            }

            const T sample = input[n_consumed++];
            if (_fill == window) {
                _sum -= _history[_position];
            } else {
                ++_fill;
            }
            _history[_position] = sample;
            _sum += sample;
            _position = (_position + 1) % window;

            if (_fill == window) {
                output[n_produced++] = _sum / static_cast<float>(window);
            }
        }
        const bool consumed = input.consume(n_consumed);
        output.publish(n_produced);
        return consumed ? gr::work::Status::OK : gr::work::Status::ERROR;
    }

private:
    std::vector<T> _history = std::vector<T>(3);
    T _sum{};
    std::size_t _position{0};
    std::size_t _fill{0};

    void resetState() {
        _history.assign(static_cast<std::size_t>(window_size.value), T{});
        _sum = T{};
        _position = 0;
        _fill = 0;
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::MovingAverage, [T], [float, std::complex<float>])
