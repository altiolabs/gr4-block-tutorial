#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/testing/TagMonitors.hpp>
#include <gnuradio-4.0/tutorial/Square.hpp>
#include <cmath>
#include <iostream>

template<typename Result>
void checked(Result&& result) {
    if (!result) throw gr::exception(result.error().message);
}

int main() {
    using namespace gr::blocks;
    gr::Graph graph;
    auto& source = graph.emplaceBlock<testing::TagSource<float>>({
        {"values", gr::Tensor<float>(gr::data_from, std::vector<float>{1, -2, 3.5f})},
        {"n_samples_max", gr::Size_t{3}}});
    auto& square = graph.emplaceBlock<gr::tutorial::Square<float>>({{"gain_db", 20.0f}});
    auto& sink = graph.emplaceBlock<testing::TagSink<float,
        testing::ProcessFunction::USE_PROCESS_BULK>>({});
    checked(graph.connect<"out", "in">(source, square));
    checked(graph.connect<"out", "in">(square, sink));
    gr::scheduler::Simple scheduler;
    checked(scheduler.exchange(std::move(graph)));
    checked(scheduler.runAndWait());
    const std::vector<float> expected{10, 40, 122.5f};
    if (sink._samples.size() != expected.size()) return 1;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (std::abs(sink._samples[i] - expected[i]) > 0.001f) return 1;
        std::cout << sink._samples[i] << '\n';
    }
}
