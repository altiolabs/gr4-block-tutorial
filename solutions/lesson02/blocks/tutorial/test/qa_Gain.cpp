#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/Gain.hpp>

int main() {
    using namespace boost::ut;
    "scalar gain"_test = []<typename T> {
        gr::tutorial::Gain<T> gain;
        expect(eq(gain.processOne(T{1}), T{2}));
        expect(eq(gain.processOne(T{-2}), T{-4}));
        expect(eq(gain.processOne(T{3}), T{6}));
    } | std::tuple<float, std::int16_t, std::int32_t>{};
    "fractional float and unsigned"_test = [] {
        gr::tutorial::Gain<float> gain;
        expect(eq(gain.processOne(3.5f), 7.0f));
        gr::tutorial::Gain<std::uint8_t> byte_gain;
        expect(eq(byte_gain.processOne(std::uint8_t{12}), std::uint8_t{24}));
    };
    "float SIMD lanes"_test = [] {
        gr::tutorial::Gain<float> gain;
        using V = gr::meta::simdize<float>;
        V input([](auto i) { return static_cast<float>(i) - 2.5f; });
        const V output = gain.processOne(input);
        for (std::size_t i = 0; i < V::size(); ++i) {
            expect(eq(output[i], 2.0f * input[i]));
        }
    };
}
