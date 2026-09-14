# Lesson 8: Messages and control interaction

One more way of interacting with a block: a discrete event that another part of the application can act on. Our float-only `ThresholdDetector.hpp` passes the stream through unchanged and emits an event when it crosses from below threshold to at or above threshold:

```text
threshold: 1.0
input:     0.2  0.7  1.1  1.3  0.8  1.4
events:               ^              ^
```

The output stream still contains all six input samples. The two events are additional notifications, not replacement stream items. A receiver might log them or update an application display. Unlike a tag, an event here doesn't promise an exact position in the output stream.

We also want a reset command that re-arms the detector without changing its threshold. That gives our block three paths to keep straight:

```text
command sender --------------+
                            v commands
sample source --in--> ThresholdDetector --out--> sample sink
                            | events
                            v
                       event receiver
```

The sample ports carry floats; the two application message ports carry `gr::Message`. These connections are inside the GR4 application. There is no ZeroMQ connection or GR3 codec in this lesson.

Create `blocks/tutorial/include/gnuradio-4.0/tutorial/ThresholdDetector.hpp` with the following block:

```cpp
#pragma once

#include <span>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Message.hpp>

namespace gr::tutorial {

struct ThresholdDetector : gr::Block<ThresholdDetector> {
    using gr::Block<ThresholdDetector>::Block;
    using gr::Block<ThresholdDetector>::processMessages;
    using Description = gr::Doc<"Notify on upward threshold crossings.">;

    gr::PortIn<float> in;
    gr::PortOut<float> out;
    gr::Annotated<float, "threshold"> threshold{1.0f};
    gr::MsgPortIn commands;
    gr::MsgPortOut events;
    GR_MAKE_REFLECTABLE(ThresholdDetector, in, out, threshold, commands, events);

    void start() { _above = false; }

    void settingsChanged(const gr::property_map& old_settings,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("threshold") &&
            old_settings.at("threshold") != new_settings.at("threshold")) {
            _above = false;
        }
    }

    [[nodiscard]] float processOne(float input) {
        const bool above = input >= threshold.value;
        if (above && !_above) {
            gr::sendMessage<gr::message::Command::Notify>(
                events, this->unique_name, "threshold_crossing",
                gr::property_map{{"event", "threshold_crossing"}, {"value", input}});
        }
        _above = above;
        return input;
    }

    void processMessages(gr::MsgPortIn& /*port*/,
                         std::span<const gr::Message> messages) {
        for (const auto& message : messages) {
            if (!message.serviceName.empty() &&
                message.serviceName != this->name &&
                message.serviceName != this->unique_name) {
                continue;
            }
            if (message.cmd == gr::message::Command::Set &&
                message.endpoint == "reset" &&
                message.data.has_value() && message.data->empty()) {
                _above = false;
            }
        }
    }

private:
    bool _above{false};
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::ThresholdDetector)
```

## Detect a crossing, not every high sample

`_above` remembers the previous sample's comparison. Starting with `_above = false`, the six example samples give us this sequence:

| Input | At or above 1.0? | Previous `_above` | Event? |
| --- | --- | --- | --- |
| 0.2 | No | false | No |
| 0.7 | No | false | No |
| 1.1 | Yes | false | Yes, with value 1.1 |
| 1.3 | Yes | true | No |
| 0.8 | No | true | No; re-arms the detector |
| 1.4 | Yes | false | Yes, with value 1.4 |

The assignment `_above = above` happens after the event check. If it happened first, `above && !_above` could never be true. Equality counts as high because the comparison is `>=`. A first sample at or above threshold also produces an event: our startup policy treats the detector as armed, even without an earlier low sample.

Changing the threshold clears this state at the settings boundary; reapplying the same value preserves it. Use finite values in the tests. Comparisons with NaN are false, leaving the detector armed for the next qualifying finite sample.

We're using `processOne` again, this time non-const and scalar. GR4's non-const path processes samples in order, which is what the crossing state needs; it doesn't accept Gain's SIMD signature. Bulk processing didn't replace `processOne`. We've also left off `noexcept` because constructing and sending an event can allocate.

## Understand the event envelope and body

The application ports carry `gr::Message`, not bare PMT values or GR3 handler callbacks. A message has a command, service name, endpoint, request ID, and `data` of type `std::expected<gr::property_map, gr::Error>`. The successful body is a map of generic PMT values. At a crossing, `sendMessage<Notify>` publishes the event name and observed float value on `events`.

For the first event above, the fields we care about are:

| Field | Value in this example |
| --- | --- |
| `cmd` | `gr::message::Command::Notify`: a notification |
| `serviceName` | The detector's `unique_name`, identifying this instance |
| `endpoint` | `"threshold_crossing"`, the kind of notification |
| `data` | A successful map: `{"event": "threshold_crossing", "value": 1.1f}` |

The endpoint belongs to the message envelope; `event` and `value` are keys in its body. We deliberately repeat the event name in the body so the recorded payload is easy to inspect. The helper fills in the message envelope for us. We aren't using request IDs or implementing a request/reply exchange here.

Check `data.has_value()` before reading that map: `data` can hold an error instead. A successful empty map is different from an error, which matters for the reset command below. The event promises no timestamp or sample index; use a tag when exact stream alignment matters. Buffering is finite, so connect a receiver and keep this demonstration small. In this SDK, `sendMessage` can drop a message when the port buffer is full; this example isn't a reliable-delivery protocol.

## Send reset through the application command port

The `using` declaration preserves the base `processMessages` overload for GR4's built-in `msgIn` control path. Without it, defining our own overload would hide the inherited name in C++. Our `commands` port has a different type from `MsgPortInBuiltin`: it is an application port we own, not a replacement for the framework's settings and lifecycle controls.

Its handler accepts a `Set` message at endpoint `"reset"` with a successful empty body, addressed to this block or to the empty-name wildcard. The port connection delivers the message; our handler checks `serviceName` against `name` and `unique_name` to decide whether to act on it. Other addresses, commands, endpoints, nonempty bodies, and error bodies are ignored. Reset emits no event or acknowledgement and leaves the threshold alone. The next at/above sample can emit an event even if the signal never went below threshold.

The scheduler dispatches these input messages through `processScheduledMessages()`. With this `std::span<const gr::Message>` handler it consumes the delivered message batch after the handler returns. Don't override the built-in control port or manually drain it, and don't call a handler from an unrelated thread while processing samples.

For a focused test with a detector instance named `detector`, connect a command port before sending. As with stream connections, check the result:

```cpp
gr::MsgPortOut command_source;
tutorial_test::require(command_source.connect(detector.commands));
gr::sendMessage<gr::message::Command::Set>(
    command_source, detector.unique_name, "reset", gr::property_map{});
```

Sending queues the command; it doesn't call the handler immediately. In a running graph, the scheduler dispatches it. In a controlled test without a scheduler, call `detector.processScheduledMessages()` between work calls to exercise that dispatch path. Don't call it concurrently with a scheduler running the same block. Threshold remains a setting; we aren't using messages to invent another setting mechanism.

## Record both outputs in a graph test

Create `blocks/tutorial/test/qa_ThresholdDetector.cpp`, reusing `GraphHelpers.hpp` from Lesson 6. The numeric `TagSink` doesn't have our application event port, so add this test-only sink. It records samples and messages independently; it doesn't assume that a message handler call lines up with a particular sample span.

```cpp
#include "GraphHelpers.hpp"
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/ThresholdDetector.hpp>

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
```

No runtime registration is needed for a sink constructed directly by C++ type. Add the graph test below. There are three connections: two for samples and one for events. Both message ports are named `events`, but one is an output on the detector and the other is an input on the sink.

```cpp
int main() {
    using namespace boost::ut;
    "finite graph records two crossing events"_test = [] {
        gr::Graph graph;
        const std::vector<float> values{0.2f, 0.7f, 1.1f, 1.3f, 0.8f, 1.4f};
        auto& source = tutorial_test::source(graph, values);
        auto& detector = graph.emplaceBlock<gr::tutorial::ThresholdDetector>({});
        auto& sink = graph.emplaceBlock<RecordingSink>({});
        tutorial_test::require(graph.connect<"out", "in">(source, detector));
        tutorial_test::require(graph.connect<"out", "in">(detector, sink));
        tutorial_test::require(graph.connect<"events", "events">(detector, sink));
        const auto scheduler = tutorial_test::run(std::move(graph));

        expect(std::ranges::equal(sink.samples, values));
        const std::vector<float> expected{1.1f, 1.4f};
        expect(eq(sink.messages.size(), expected.size())) << fatal;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto& message = sink.messages[i];
            expect(message.cmd == gr::message::Command::Notify);
            expect(message.serviceName == detector.unique_name);
            expect(message.endpoint == "threshold_crossing");
            expect(message.data.has_value()) << fatal;
            expect(eq(message.data->size(), 2UZ));
            expect(message.data->contains("event") && message.data->contains("value")) << fatal;
            expect(message.data->at("event") == gr::pmt::Value("threshold_crossing"));
            expect(message.data->at("value").holds<float>());
            expect(message.data->at("value") == gr::pmt::Value(expected[i]));
        }
    };
}
```

`tutorial_test::run` uses the single-threaded Simple scheduler and returns it to keep the graph alive during assertions. Checking the message count alone isn't enough: the envelope and body checks establish what was sent and which block sent it. The `fatal` checks guard accesses that would otherwise index a missing message or read a missing body/key. These expected floats are copies of the input literals, not results of an approximate calculation.

## Verify reset without restarting the detector

Use the `Harness`, `checkMessages`, and `checkEvents` helpers in the [supplied test](../solutions/lesson08/blocks/tutorial/test/qa_ThresholdDetector.cpp) for deterministic command tests. The harness connects sample ports and message ports, initializes the detector, and moves it into the running lifecycle state once. `work(values)` feeds a sample batch through the framework's work method; `reset(address)` queues a command and dispatches it before the next batch. `checkEvents` verifies and consumes the recorded messages so each assertion covers only new events.

Follow this sequence on the same detector instance:

1. Process `[1.1, 1.3]` and read exactly one event, with value 1.1.
2. Send and dispatch reset. Read no new event and check that `threshold` is still 1.0.
3. Process `[1.4]` and read a new event with value 1.4, even though there was no intervening low sample.

Don't call `start()` or construct a new detector between those steps: either would re-arm it and hide a broken command handler. Reset takes effect at dispatch, not at a promised sample offset. The supplied test also verifies that the command buffer was consumed after dispatch.

Add the supplied cases for equality to threshold, a first high sample, a crossing split across calls, and changed-versus-unchanged staged threshold updates. Check that the wildcard and block-name addresses are accepted, while unrelated addresses and malformed commands leave the detector's state unchanged. Copy these helper definitions and test cases into your QA file without adding a second `RecordingSink` or `main()`.

## Build and check the final module

Add `include/gnuradio-4.0/tutorial/ThresholdDetector.hpp` to `GrTutorialBlocks_HDRS` in `blocks/tutorial/CMakeLists.txt`. Add `"gr::tutorial::ThresholdDetector"` to `blocks/tutorial/test/ExpectedBlocks.hpp`. Add the QA target to `blocks/tutorial/test/CMakeLists.txt`:

```cmake
add_tutorial_test(qa_ThresholdDetector)
target_link_libraries(qa_ThresholdDetector PRIVATE gnuradio4::gr-testing
  gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
```

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
cmake --install build
./build/blocks/tutorial/examples/tutorial_registry
./build/blocks/tutorial/examples/tutorial_plugin_check
```

The completed module has nine CTest checks. The plugin checks should now cover 12 registered variants across six blocks. Studio message visualization isn't required validation; the recording-sink test checks the actual messages.

## What we've built

We now have four distinct mechanisms in the blocks:

| Mechanism | What we used it for |
| --- | --- |
| Stream | Moving samples or PMT objects through typed ports |
| Setting | Configuring gain, packet size, window size, and threshold |
| Tag | Attaching metadata to a particular stream position |
| Message | Sending a threshold event or a reset command without stream timing |

PMT is the generic value representation used where needed, including settings and metadata maps. It doesn't decide whether something travels as a stream item, a tag value, or a message body. A threshold crossing could reasonably be a tag or a message depending on who needs it. Here the stream keeps moving and another part of the application gets an event to act on.

Run the complete CTest suite, reinstall, rerun the installed plugin and external-consumer checks, and repeat the GR3 receiver check. Each of the six blocks should now have its intended final form: fixed Gain, bulk Square with cached gain, PMT Packetizer, manual TagForwarder, persistent MovingAverage, and ThresholdDetector with application messages. The eight snapshots retain the points where those forms changed.

For the final installed checks, copy Lesson 8's external-consumer updates, then configure and run against the installation using your activated SDK and compiler:

```bash
cp -R solutions/lesson08/test_external/. test_external/
cmake -S test_external/consumer -B build-consumer -G Ninja -DCMAKE_CXX_COMPILER="$CXX"
cmake --build build-consumer
./build-consumer/tutorial_consumer
./build-consumer/tutorial_registry_consumer
python3 test_external/check_studio.py
```

The consumer checks installed headers and runtime registrations from a separate CMake project. `check_studio.py` checks all 12 tutorial variants and their setting defaults through the same `/blocks` API Studio uses. It starts and stops its own local server; placing blocks and editing properties in the Studio GUI remains a hands-on check. Repeat the [Lesson 5 receiver check](lesson05-pmt-gr3.md#send-those-objects-to-gr3) to verify that the earlier interoperability path still works. See [validation notes](../VALIDATION.md) for the SDK and checks used in the workshop rehearsal.

---

[Previous: Lesson 7](lesson07-stateful.md) | [Lesson list](../README.md#lessons) | [Next: Workshop overview](../README.md)
