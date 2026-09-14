#pragma once

#include <algorithm>
#include <complex>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Tensor.hpp>
#include <gnuradio-4.0/Value.hpp>

namespace gr::tutorial {

template<typename T>
struct Packetizer : gr::Block<Packetizer<T>, gr::Resampling<1024U, 1U, false>> {
    using Base = gr::Block<Packetizer<T>, gr::Resampling<1024U, 1U, false>>;
    using Base::Base;
    using Description = gr::Doc<"Sum input streams into PMT numeric vectors.">;

    std::vector<gr::PortIn<T>> in{2};
    gr::PortOut<gr::pmt::Value> out;
    gr::Annotated<gr::Size_t, "n_inputs", gr::Limits<1U, 8U>> n_inputs{2U};
    gr::Annotated<gr::Size_t, "packet_size", gr::Limits<1U, 1024U>> packet_size{1024U};

    GR_MAKE_REFLECTABLE(Packetizer, in, out, n_inputs, packet_size);

    void settingsChanged(const gr::property_map&,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("n_inputs")) {
            in.resize(static_cast<std::size_t>(n_inputs.value));
        }
        if (new_settings.contains("packet_size")) {
            this->input_chunk_size = packet_size.value;
            this->output_chunk_size = 1U;
        }
    }

    template<gr::InputSpanLike TInSpan>
    [[nodiscard]] gr::work::Status processBulk(const std::span<TInSpan>& inputs,
                                             std::span<gr::pmt::Value> output) const {
        const auto n = static_cast<std::size_t>(this->input_chunk_size.value);
        for (std::size_t packet = 0; packet < output.size(); ++packet) {
            const auto offset = packet * n;
            std::vector<T> sum(n);
            std::ranges::copy(std::span<const T>(inputs[0].data() + offset, n),
                              sum.begin());
            for (std::size_t channel = 1; channel < inputs.size(); ++channel) {
                const auto slice = std::span<const T>(inputs[channel].data() + offset, n);
                std::ranges::transform(sum, slice, sum.begin(), std::plus<T>{});
            }
            output[packet] = gr::pmt::Value(gr::Tensor<T>(gr::data_from, sum));
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Packetizer, [T], [float, std::complex<float>])
