#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/Square.hpp>
#include <array>

int main() {
    using namespace boost::ut;
    using gr::tutorial::Square;
    "default square"_test = [] {
        Square<float> square;
        expect(eq(square.processOne(1.0f), 1.0f));
        expect(eq(square.processOne(-2.0f), 4.0f));
        expect(eq(square.processOne(3.5f), 12.25f));
    };
    "configured and staged gain updates"_test = [] {
        Square<float> square(gr::property_map{{"gain_db", 20.0f}});
        square.settings().init();
        std::ignore = square.settings().applyStagedParameters();
        const std::array input{1.0f, -2.0f, 3.5f};
        const std::array expected{10.0f, 40.0f, 122.5f};
        for (std::size_t i = 0; i < input.size(); ++i)
            expect(std::abs(square.processOne(input[i]) - expected[i]) < 0.001f);
        expect(square.settings().setStaged({{"gain_db", 0.0f}}).empty());
        std::ignore = square.settings().applyStagedParameters();
        expect(std::abs(square.processOne(-2.0f) - 4.0f) < 0.001f);
        expect(square.settings().setStaged({{"name", "renamed"}}).empty());
        std::ignore = square.settings().applyStagedParameters();
        expect(std::abs(square.processOne(3.5f) - 12.25f) < 0.001f);
    };
    "complex square is not magnitude squared"_test = [] {
        Square<std::complex<float>> square;
        expect(std::abs(square.processOne({1.0f, 2.0f}) - std::complex<float>{-3.0f, 4.0f}) < 0.001f);
    };
}
