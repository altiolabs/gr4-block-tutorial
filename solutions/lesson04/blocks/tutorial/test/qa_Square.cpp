#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/Square.hpp>
#include <array>

template<typename T>
T process(gr::tutorial::Square<T>& square, T input) {
    T output{};
    boost::ut::expect(square.processBulk(std::span<const T>(&input, 1),
                                        std::span<T>(&output, 1)) == gr::work::Status::OK);
    return output;
}

int main() {
    using namespace boost::ut;
    using gr::tutorial::Square;
    "bulk chunks and empty span"_test = [] {
        Square<float> square;
        const std::array input{1.0f, -2.0f, 3.5f, 4.0f, -5.0f, 6.0f};
        std::array<float, 6> whole{}, chunks{};
        expect(square.processBulk(input, whole) == gr::work::Status::OK);
        for (const auto [offset, count] : std::array{std::pair{0UZ, 2UZ}, std::pair{2UZ, 3UZ}, std::pair{5UZ, 1UZ}}) {
            expect(square.processBulk(std::span(input).subspan(offset, count),
                                      std::span(chunks).subspan(offset, count)) == gr::work::Status::OK);
        }
        expect(whole == chunks);
        expect(square.processBulk({}, {}) == gr::work::Status::OK);
        expect(whole == chunks);
    };
    "default square"_test = [] {
        Square<float> square;
        expect(eq(process(square, 1.0f), 1.0f));
        expect(eq(process(square, -2.0f), 4.0f));
        expect(eq(process(square, 3.5f), 12.25f));
    };
    "configured and staged gain updates"_test = [] {
        Square<float> square(gr::property_map{{"gain_db", 20.0f}});
        square.settings().init();
        std::ignore = square.settings().applyStagedParameters();
        const std::array input{1.0f, -2.0f, 3.5f};
        const std::array expected{10.0f, 40.0f, 122.5f};
        for (std::size_t i = 0; i < input.size(); ++i)
            expect(std::abs(process(square, input[i]) - expected[i]) < 0.001f);
        expect(square.settings().setStaged({{"gain_db", 0.0f}}).empty());
        std::ignore = square.settings().applyStagedParameters();
        expect(std::abs(process(square, -2.0f) - 4.0f) < 0.001f);
        expect(square.settings().setStaged({{"name", "renamed"}}).empty());
        std::ignore = square.settings().applyStagedParameters();
        expect(std::abs(process(square, 3.5f) - 12.25f) < 0.001f);
    };
    "complex square is not magnitude squared"_test = [] {
        Square<std::complex<float>> square;
        expect(std::abs(process(square, std::complex<float>{1.0f, 2.0f}) - std::complex<float>{-3.0f, 4.0f}) < 0.001f);
    };
}
