#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/tutorial/Gain.hpp>
#include <gnuradio-4.0/tutorial/Packetizer.hpp>
#include <gnuradio-4.0/zeromq/ZmqPushSink.hpp>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
#include <pthread.h>

// Small demonstration source: repeat six values at a human-readable pace.
struct PacedSource : gr::Block<PacedSource> {
    gr::PortOut<float> out;
    gr::Tensor<float> values;
    std::size_t position{0};
    GR_MAKE_REFLECTABLE(PacedSource, out, values);
    gr::work::Status processBulk(gr::OutputSpanLike auto& output) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto count = std::min(output.size(), 6UZ);
        for (std::size_t i = 0; i < count; ++i) {
            output[i] = values[position];
            position = (position + 1) % values.size();
        }
        output.publish(count);
        return gr::work::Status::OK;
    }
};

template<typename Result>
void checked(Result&& result) {
    if (!result) throw gr::exception(result.error().message);
}

int main() {
    using namespace gr::blocks;
    // Block signals before creating worker threads. The stop thread receives
    // them synchronously, so Ctrl-C cannot interrupt a ZeroMQ socket poll.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0)
        throw gr::exception("Could not configure shutdown signals");
    gr::Graph graph;
    auto& a = graph.emplaceBlock<PacedSource>({{"values", gr::Tensor<float>(gr::data_from, std::vector<float>{1, 2, 3, 4, 5, 6})}});
    auto& b = graph.emplaceBlock<PacedSource>({{"values", gr::Tensor<float>(gr::data_from, std::vector<float>{10, 20, 30, 40, 50, 60})}});
    auto& gain = graph.emplaceBlock<gr::tutorial::Gain<float>>({});
    auto& packetizer = graph.emplaceBlock<gr::tutorial::Packetizer<float>>({
        {"n_inputs", gr::Size_t{2}}, {"packet_size", gr::Size_t{3}}});
    auto& sink = graph.emplaceBlock<zeromq::ZmqPushSink<gr::pmt::Value>>({
        {"endpoint", "tcp://127.0.0.1:5558"}, {"bind", true},
        {"timeout", 100}, {"pass_tags", false}, {"pmt_wire_format", "GR3"}});
    checked(graph.connect<"out", "in">(a, gain));
    checked(graph.connect(gain, "out", packetizer, "in#0"));
    checked(graph.connect(b, "out", packetizer, "in#1"));
    checked(graph.connect<"out", "in">(packetizer, sink));
    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> scheduler;
    checked(scheduler.exchange(std::move(graph)));
    std::jthread stop_on_signal([&](std::stop_token stop) {
        // Wake sigwait if the graph returns or throws before a user signal.
        // Unlike sigtimedwait, these POSIX calls are also available on macOS.
        std::stop_callback wake(stop, [thread = pthread_self()] {
            pthread_kill(thread, SIGTERM);
        });
        int received = 0;
        if (sigwait(&signals, &received) == 0 && !stop.stop_requested()) {
            scheduler.requestStop();
        }
    });
    std::cout << "GR3 PMT vectors on tcp://127.0.0.1:5558; Ctrl-C to stop" << std::endl;
    checked(scheduler.runAndWait());
}
