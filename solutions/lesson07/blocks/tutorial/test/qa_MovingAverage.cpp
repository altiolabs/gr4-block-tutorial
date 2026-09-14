#include "GraphHelpers.hpp"
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/MovingAverage.hpp>

using gr::tutorial::MovingAverage;

// Real GR4 spans verify publication/consumption, including output capacity zero.
struct Harness {
    MovingAverage<float> block;
    gr::PortOut<float> source;
    gr::PortIn<float> sink;
    Harness() {
        tutorial_test::require(source.connect(block.in));
        tutorial_test::require(block.out.connect(sink));
        block.settings().init();
        std::ignore = block.settings().applyStagedParameters();
        block.start();
    }
    void push(const std::vector<float>& values) {
        auto data = source.streamWriter().reserve(values.size());
        std::ranges::copy(values, data.begin());
        data.publish(values.size());
    }
    std::vector<float> call(std::size_t input_count, std::size_t output_capacity,
                            std::size_t expected_consumed, std::size_t expected_produced) {
        using namespace boost::ut;
        const auto before = block.in.streamReader().available();
        {
            auto input = block.in.get<gr::SpanReleasePolicy::ProcessNone>(input_count);
            auto output = block.out.reserve<gr::SpanReleasePolicy::ProcessNone>(output_capacity);
            expect(block.processBulk(input, output) == gr::work::Status::OK);
        }
        expect(eq(before - block.in.streamReader().available(), expected_consumed));
        expect(eq(sink.streamReader().available(), expected_produced));
        auto result = sink.streamReader().get();
        std::vector<float> values(result.begin(), result.end());
        expect(result.consume(result.size()));
        return values;
    }
    void stage(const gr::property_map& settings) {
        boost::ut::expect(block.settings().setStaged(settings).empty());
        std::ignore = block.settings().applyStagedParameters();
    }
};

template<typename T>
void graphCheck(const std::vector<T>& input, const std::vector<T>& expected, gr::Size_t window) {
    gr::Graph graph;
    auto& source = tutorial_test::source(graph, input);
    auto& average = graph.emplaceBlock<MovingAverage<T>>({{"window_size", window}});
    auto& sink = graph.emplaceBlock<tutorial_test::Sink<T>>({});
    tutorial_test::require(graph.connect<"out", "in">(source, average));
    tutorial_test::require(graph.connect<"out", "in">(average, sink));
    const auto scheduler = tutorial_test::run(std::move(graph));
    boost::ut::expect(std::ranges::equal(sink._samples, expected));
}

int main() {
    using namespace boost::ut;
    "work boundary independence"_test = [] {
        const std::vector<float> expected{2, 3, 4, 5};
        Harness whole;
        whole.push({1, 2, 3, 4, 5, 6});
        expect(whole.call(6, 6, 6, 4) == expected);
        Harness chunks;
        chunks.push({1, 2});
        expect(chunks.call(2, 2, 2, 0).empty());
        chunks.push({3, 4, 5});
        auto output = chunks.call(3, 3, 3, 3);
        expect(output == std::vector<float>{2, 3, 4});
        chunks.push({6});
        auto tail = chunks.call(1, 1, 1, 1);
        output.insert(output.end(), tail.begin(), tail.end());
        expect(output == expected);
        Harness singles;
        std::vector<float> single_output;
        for (int i = 1; i <= 6; ++i) {
            singles.push({static_cast<float>(i)});
            auto result = singles.call(1, 1, 1, i < 3 ? 0UZ : 1UZ);
            single_output.insert(single_output.end(), result.begin(), result.end());
        }
        expect(single_output == expected);
    };
    "limited capacity retains unprocessed input"_test = [] {
        Harness harness;
        harness.push({1, 2, 3, 4, 5, 6});
        expect(harness.call(6, 0, 2, 0).empty());
        expect(harness.call(4, 1, 1, 1) == std::vector<float>{2});
        expect(harness.call(3, 0, 0, 0).empty());
        expect(harness.call(3, 3, 3, 3) == std::vector<float>{3, 4, 5});
        expect(harness.call(0, 0, 0, 0).empty());
    };
    "changed settings reset; same and unrelated settings preserve"_test = [] {
        Harness harness;
        harness.push({1, 2, 3});
        expect(harness.call(3, 3, 3, 1) == std::vector<float>{2});
        harness.stage({{"window_size", gr::Size_t{3}}});
        harness.stage({{"name", "still averaging"}});
        harness.push({4});
        expect(harness.call(1, 1, 1, 1) == std::vector<float>{3});
        harness.stage({{"window_size", gr::Size_t{2}}});
        harness.push({10});
        expect(harness.call(1, 1, 1, 0).empty());
        harness.push({20});
        expect(harness.call(1, 1, 1, 1) == std::vector<float>{15});
        harness.block.start();
        harness.push({30});
        expect(harness.call(1, 1, 1, 0).empty());
    };
    "finite graph completion and exact counts"_test = [] {
        graphCheck<float>({1, 2, 3, 4, 5, 6}, {2, 3, 4, 5}, 3U);
        graphCheck<float>({1, 2, 3, 4, 5, 6}, {1.5f, 2.5f, 3.5f, 4.5f, 5.5f}, 2U);
        graphCheck<float>({1, 2, 3}, {1, 2, 3}, 1U);
        graphCheck<float>({1, 2}, {}, 3U);
        using C = std::complex<float>;
        graphCheck<C>({{1, 2}, {2, 4}, {3, 6}, {4, 8}}, {{2, 4}, {3, 6}}, 3U);
    };
    "window limits"_test = [] {
        Harness harness;
        expect(eq(harness.block.window_size.value, 3U));
        for (const gr::Size_t value : {0U, 1025U}) {
            std::ignore = harness.block.settings().setStaged({{"window_size", value}});
            std::ignore = harness.block.settings().applyStagedParameters();
            expect(eq(harness.block.window_size.value, 3U));
        }
    };
}
