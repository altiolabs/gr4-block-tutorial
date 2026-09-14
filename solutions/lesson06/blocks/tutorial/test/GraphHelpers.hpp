#pragma once
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/testing/TagMonitors.hpp>

namespace tutorial_test {
template<typename T>
using Source = gr::blocks::testing::TagSource<T>;
template<typename T>
using Sink = gr::blocks::testing::TagSink<T, gr::blocks::testing::ProcessFunction::USE_PROCESS_BULK>;

inline void require(auto result) {
    if (!result) throw gr::exception(result.error().message);
}

inline auto run(gr::Graph&& graph) {
    auto scheduler = std::make_unique<gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded>>();
    require(scheduler->exchange(std::move(graph)));
    require(scheduler->runAndWait());
    return scheduler; // Keep the graph and its recording sinks alive for assertions.
}

template<typename T>
auto& source(gr::Graph& graph, const std::vector<T>& values) {
    return graph.emplaceBlock<Source<T>>({
        {"values", gr::Tensor<T>(gr::data_from, values)},
        {"n_samples_max", static_cast<gr::Size_t>(values.size())}});
}
}
