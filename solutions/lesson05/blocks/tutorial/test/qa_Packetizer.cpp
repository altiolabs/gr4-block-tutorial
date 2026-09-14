#include <boost/ut.hpp>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/testing/TagMonitors.hpp>
#include <gnuradio-4.0/tutorial/Packetizer.hpp>

template<typename Result>
void checked(Result&& result) {
    if (!result) throw gr::exception(result.error().message);
}

// PMTs are stream items here; use a tiny recording sink without numeric-only
// metadata helpers so every payload is retained after scheduler completion.
struct PmtSink : gr::Block<PmtSink> {
    gr::PortIn<gr::pmt::Value> in;
    std::vector<gr::pmt::Value> values;
    GR_MAKE_REFLECTABLE(PmtSink, in);
    gr::work::Status processBulk(std::span<const gr::pmt::Value> input) {
        values.insert(values.end(), input.begin(), input.end());
        return gr::work::Status::OK;
    }
};

template<typename T>
void check(const std::vector<std::vector<T>>& channels, gr::Size_t packet_size,
           const std::vector<std::vector<T>>& expected) {
    using namespace boost::ut;
    gr::Graph graph;
    auto& packetizer = graph.emplaceBlock<gr::tutorial::Packetizer<T>>({
        {"n_inputs", static_cast<gr::Size_t>(channels.size())}, {"packet_size", packet_size}});
    for (std::size_t i = 0; i < channels.size(); ++i) {
        auto& source = graph.emplaceBlock<gr::blocks::testing::TagSource<T>>({
            {"values", gr::Tensor<T>(gr::data_from, channels[i])},
            {"n_samples_max", static_cast<gr::Size_t>(channels[i].size())}});
        checked(graph.connect(source, "out", packetizer, std::format("in#{}", i)));
    }
    auto& sink = graph.emplaceBlock<PmtSink>({});
    checked(graph.connect<"out", "in">(packetizer, sink));
    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> scheduler;
    checked(scheduler.exchange(std::move(graph)));
    checked(scheduler.runAndWait());
    expect(eq(sink.values.size(), expected.size())) << fatal;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto* tensor = sink.values[i].template get_if<gr::Tensor<T>>();
        expect(tensor != nullptr) << fatal;
        expect(eq(tensor->size(), expected[i].size()));
        expect(std::ranges::equal(*tensor, expected[i]));
    }
}

int main() {
    using namespace boost::ut;
    "two inputs and multiple packets"_test = [] {
        check<float>({{1, 2, 3, 4, 5, 6}, {10, 20, 30, 40, 50, 60}}, 3U, {{11, 22, 33}, {44, 55, 66}});
    };
    "one input and packet size one"_test = [] {
        check<float>({{1, 2, 3}}, 1U, {{1}, {2}, {3}});
    };
    "incomplete tail is dropped"_test = [] {
        check<float>({{1, 2, 3, 4, 5, 6, 7}, {10, 20, 30, 40, 50, 60, 70}}, 3U, {{11, 22, 33}, {44, 55, 66}});
        check<float>({{1, 2}}, 3U, {});
    };
    "complex tensor payload"_test = [] {
        using C = std::complex<float>;
        check<C>({{C{1, 2}, C{3, 4}}, {C{10, 20}, C{30, 40}}}, 2U, {{C{11, 22}, C{33, 44}}});
    };
    "defaults and setting limits"_test = [] {
        gr::tutorial::Packetizer<float> block;
        block.settings().init();
        std::ignore = block.settings().applyStagedParameters();
        expect(eq(block.n_inputs.value, 2U));
        expect(eq(block.packet_size.value, 1024U));
        expect(eq(block.in.size(), 2UZ));
        expect(eq(block.input_chunk_size.value, 1024U));
        for (const auto& [key, value] : std::vector<std::pair<const char*, gr::Size_t>>{
                 {"n_inputs", 0U}, {"n_inputs", 9U}, {"packet_size", 0U}, {"packet_size", 1025U}}) {
            std::ignore = block.settings().setStaged({{key, value}});
            std::ignore = block.settings().applyStagedParameters();
            expect(eq(block.n_inputs.value, 2U));
            expect(eq(block.packet_size.value, 1024U));
        }
    };
}
