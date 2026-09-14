#include "GraphHelpers.hpp"
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/ThresholdDetector.hpp>

using gr::tutorial::ThresholdDetector;
using gr::message::Command;

void checkMessages(std::span<const gr::Message> messages, const std::vector<float>& expected,
                   std::string_view service) {
    using namespace boost::ut;
    expect(eq(messages.size(), expected.size())) << fatal;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& message = messages[i];
        expect(message.cmd == Command::Notify);
        expect(message.serviceName == service);
        expect(message.endpoint == "threshold_crossing");
        expect(message.data.has_value()) << fatal;
        expect(eq(message.data->size(), 2UZ));
        expect(message.data->at("event") == gr::pmt::Value("threshold_crossing"));
        expect(message.data->at("value").holds<float>());
        expect(message.data->at("value") == gr::pmt::Value(expected[i]));
    }
}

void checkEvents(gr::MsgPortIn& receiver, const std::vector<float>& expected,
                 std::string_view service) {
    auto messages = receiver.streamReader().get();
    checkMessages(messages, expected, service);
    boost::ut::expect(messages.consume(messages.size()));
}

struct RecordingSink : gr::Block<RecordingSink> {
    using gr::Block<RecordingSink>::processMessages;
    gr::PortIn<float> in;
    gr::MsgPortIn events;
    std::vector<float> samples;
    std::vector<gr::Message> messages;
    GR_MAKE_REFLECTABLE(RecordingSink, in, events);
    gr::work::Status processBulk(std::span<const float> input) {
        samples.insert(samples.end(), input.begin(), input.end());
        return gr::work::Status::OK;
    }
    void processMessages(gr::MsgPortIn&, std::span<const gr::Message> input) {
        messages.insert(messages.end(), input.begin(), input.end());
    }
};

struct Harness {
    ThresholdDetector detector;
    gr::MsgPortOut commands;
    gr::MsgPortIn events;
    gr::PortOut<float> source;
    gr::PortIn<float> sink;
    Harness() {
        tutorial_test::require(commands.connect(detector.commands));
        tutorial_test::require(detector.events.connect(events));
        tutorial_test::require(source.connect(detector.in));
        tutorial_test::require(detector.out.connect(sink));
        detector.settings().init();
        std::ignore = detector.settings().applyStagedParameters();
        tutorial_test::require(detector.changeStateTo(gr::lifecycle::State::INITIALISED));
        tutorial_test::require(detector.changeStateTo(gr::lifecycle::State::RUNNING));
    }
    void work(const std::vector<float>& values) {
        using namespace boost::ut;
        {
            auto input = source.streamWriter().reserve(values.size());
            std::ranges::copy(values, input.begin());
            input.publish(values.size());
        }
        const auto result = detector.work(values.size());
        expect(result.status == gr::work::Status::OK);
        auto output = sink.streamReader().get();
        expect(std::ranges::equal(output, values));
        expect(output.consume(output.size()));
    }
    void stage(float threshold) {
        boost::ut::expect(detector.settings().setStaged({{"threshold", threshold}}).empty());
        std::ignore = detector.settings().applyStagedParameters();
    }
    void reset(std::string_view address = "") {
        gr::sendMessage<Command::Set>(commands, address, "reset", gr::property_map{});
        detector.processScheduledMessages();
        boost::ut::expect(boost::ut::eq(detector.commands.streamReader().available(), 0UZ));
    }
};

int main() {
    using namespace boost::ut;
    "finite graph emits exactly the two upward crossings"_test = [] {
        gr::Graph graph;
        const std::vector<float> values{0.2f, 0.7f, 1.1f, 1.3f, 0.8f, 1.4f};
        auto& source = tutorial_test::source(graph, values);
        auto& detector = graph.emplaceBlock<ThresholdDetector>({});
        auto& sink = graph.emplaceBlock<RecordingSink>({});
        tutorial_test::require(graph.connect<"events", "events">(detector, sink));
        tutorial_test::require(graph.connect<"out", "in">(source, detector));
        tutorial_test::require(graph.connect<"out", "in">(detector, sink));
        const auto scheduler = tutorial_test::run(std::move(graph));
        expect(std::ranges::equal(sink.samples, values));
        checkMessages(sink.messages, {1.1f, 1.4f}, detector.unique_name);
    };
    "reset dispatched between work calls without restarting"_test = [] {
        Harness harness;
        harness.work({1.1f, 1.3f});
        checkEvents(harness.events, {1.1f}, harness.detector.unique_name);
        harness.reset(harness.detector.unique_name);
        checkEvents(harness.events, {}, harness.detector.unique_name);
        expect(eq(harness.detector.threshold.value, 1.0f));
        harness.work({1.4f});
        checkEvents(harness.events, {1.4f}, harness.detector.unique_name);
    };
    "equality, first high, and crossing split across calls"_test = [] {
        Harness harness;
        harness.work({1.0f});
        checkEvents(harness.events, {1.0f}, harness.detector.unique_name);
        harness.work({1.2f, 0.5f});
        checkEvents(harness.events, {}, harness.detector.unique_name);
        harness.work({1.0f});
        checkEvents(harness.events, {1.0f}, harness.detector.unique_name);
    };
    "threshold updates preserve or reset crossing state"_test = [] {
        Harness harness;
        harness.work({1.1f});
        checkEvents(harness.events, {1.1f}, harness.detector.unique_name);
        harness.stage(1.0f);
        harness.work({1.3f});
        checkEvents(harness.events, {}, harness.detector.unique_name);
        harness.stage(1.2f);
        harness.work({1.4f});
        checkEvents(harness.events, {1.4f}, harness.detector.unique_name);
    };
    "reset accepts wildcard and block name; ignores other commands"_test = [] {
        Harness harness;
        harness.work({2.0f});
        checkEvents(harness.events, {2.0f}, harness.detector.unique_name);
        harness.reset("unrelated block");
        gr::sendMessage<Command::Notify>(harness.commands, "", "reset", gr::property_map{});
        gr::sendMessage<Command::Set>(harness.commands, "", "other", gr::property_map{});
        gr::sendMessage<Command::Set>(harness.commands, "", "reset", gr::property_map{{"unexpected", true}});
        gr::sendMessage<Command::Set>(harness.commands, "", "reset", gr::Error("invalid body"));
        harness.detector.processScheduledMessages();
        harness.work({2.0f});
        checkEvents(harness.events, {}, harness.detector.unique_name);
        harness.reset();
        harness.work({2.0f});
        checkEvents(harness.events, {2.0f}, harness.detector.unique_name);
        harness.reset(harness.detector.name.value);
        harness.work({2.0f});
        checkEvents(harness.events, {2.0f}, harness.detector.unique_name);
    };
}
