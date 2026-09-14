# Lesson 7: Stateful processing

Now let's make the output depend on earlier samples. `MovingAverage.hpp` averages the most recent `window_size` inputs, waiting for a full window before producing the first result:

```text
window_size = 3
input:        1 2 3 4 5 6
output:           2 3 4 5
```

No zero padding and no partial-window averages. For an input of length L and window W, produce `max(0, L - W + 1)` outputs. Window one is pass-through. A stream shorter than the window produces no output; end of stream doesn't flush a padded window.

For window three, the first result is `(1 + 2 + 3) / 3 = 2`. The next sample slides the window forward: `(2 + 3 + 4) / 3 = 3`. These windows overlap. Unlike Lesson 5's Packetizer, we aren't dividing the input into disjoint groups of three.

Lesson 6 already kept a marker flag between calls. Now the state is part of the calculation itself: the block must remember earlier samples even when the scheduler hands it a new input span. Two things matter independently: getting the average right and telling GR4 exactly how many samples we consumed and produced.

Create `blocks/tutorial/include/gnuradio-4.0/tutorial/MovingAverage.hpp` with the following block:

```cpp
#pragma once

#include <complex>
#include <cstddef>
#include <vector>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

template<typename T>
struct MovingAverage : gr::Block<MovingAverage<T>, gr::NoTagPropagation> {
    using Base = gr::Block<MovingAverage<T>, gr::NoTagPropagation>;
    using Base::Base;
    using Description = gr::Doc<"Average a full window of recent samples.">;

    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::Annotated<gr::Size_t, "window_size", gr::Limits<1U, 1024U>> window_size{3U};
    GR_MAKE_REFLECTABLE(MovingAverage, in, out, window_size);

    void start() { resetState(); }

    void settingsChanged(const gr::property_map& old_settings,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("window_size") &&
            old_settings.at("window_size") != new_settings.at("window_size")) {
            resetState();
        }
    }

    [[nodiscard]] gr::work::Status processBulk(gr::InputSpanLike auto& input,
                                             gr::OutputSpanLike auto& output) {
        const auto window = _history.size();
        std::size_t n_consumed = 0;
        std::size_t n_produced = 0;
        while (n_consumed < input.size()) {
            const bool will_produce = _fill + 1 >= window;
            if (will_produce && n_produced == output.size()) {
                break;
            }

            const T sample = input[n_consumed++];
            if (_fill == window) {
                _sum -= _history[_position];
            } else {
                ++_fill;
            }
            _history[_position] = sample;
            _sum += sample;
            _position = (_position + 1) % window;

            if (_fill == window) {
                output[n_produced++] = _sum / static_cast<float>(window);
            }
        }
        const bool consumed = input.consume(n_consumed);
        output.publish(n_produced);
        return consumed ? gr::work::Status::OK : gr::work::Status::ERROR;
    }

private:
    std::vector<T> _history = std::vector<T>(3);
    T _sum{};
    std::size_t _position{0};
    std::size_t _fill{0};

    void resetState() {
        _history.assign(static_cast<std::size_t>(window_size.value), T{});
        _sum = T{};
        _position = 0;
        _fill = 0;
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::MovingAverage, [T], [float, std::complex<float>])
```

## Keep the sliding window between calls

The ring, sum, next position, and fill count are block members. Only the accounting counters are local to the call:

| Member | What it remembers |
| --- | --- |
| `_history` | Storage for the most recent window of samples |
| `_sum` | Sum of the samples currently in that window |
| `_position` | Slot to write next; once full, this holds the oldest sample |
| `_fill` | Number of valid samples, capped at the window length |

Once the window fills, we subtract the oldest sample before adding its replacement. For `[1, 2, 3]`, the sum is 6. Accepting 4 replaces 1, so the new sum is `6 - 1 + 4 = 9`, and the next average is 3. The modulo operation wraps `_position` back to the start of the storage; it doesn't restart the averaging window.

The sum has type `T`, and division by the real window length works for both float and complex input. An explicit loop makes these state changes easier to follow than a ranges expression. Resetting these members inside `processBulk` would lose history at every work boundary and give different answers for different chunk sizes.

## Report what was actually consumed and produced

There is no fixed N-to-one decimation here: after startup, every input produces an output. The base chunk sizes stay at one. `input.consume(n_consumed)` and `output.publish(n_produced)` override full-span accounting, including publishing zero while history fills. We still return `OK` after accepting useful samples into history; `INSUFFICIENT_INPUT_ITEMS` would describe that progress incorrectly.

For example, a fresh window of three can accept `[1, 2]` and produce nothing. That call reports two consumed and zero produced. A later call with `[3, 4, 5]` reports three consumed and three produced: `[2, 3, 4]`. `OK` is a status, not a count; the span methods tell GR4 which input items may be released and which output items are valid. Without the explicit publication count, unwritten output slots could be treated as results.

The capacity check happens before accepting a sample that would produce a result. Even if output space is limited, we neither overwrite the output span nor consume a result's input prematurely. Normal synchronous scheduling supplies matched capacities, but the loop obeys both bounds. To see why the order matters, consider the focused test with six available inputs and no output space:

1. A fresh block can consume 1 and 2 into history without producing anything.
2. It must stop before consuming 3, because that sample would complete a window and require an output slot.
3. When one output slot becomes available, it consumes 3 and publishes 2. Samples 4, 5, and 6 remain available for later calls.

This is backpressure: lack of downstream space limits how far processing can advance. The block doesn't need a separate queue for the unconsumed input; those samples remain in the input buffer. The test harness deliberately controls span capacities to exercise this case without relying on the scheduler to create it.

## Reset history only when intended

`window_size` defaults to three and accepts 1–1024. `start()` clears state for a new run. The settings callback resets it only when the window actually changes, requiring W fresh samples before the next output. Reapplying the same value or changing an unrelated setting preserves history. Use the settings path so changes take effect at a work boundary, without concurrent writes to processing state.

For example, after averaging `[1, 2, 3]`, setting the window to three again must leave those samples in history: the next input, 4, should produce 3. Changing the window to two deliberately starts over. Input 10 alone then produces nothing, and input 20 completes the new window and produces 15. We choose this reset policy rather than trying to reinterpret the old history at a new size.

We've disabled automatic tag propagation because removing the startup outputs changes sample positions. This block drops ordinary input metadata; designing a history-aware tag mapping would be another problem. GR4 still handles end of stream. The first output doesn't have the same absolute stream index as its latest input sample.

For window three, output index 0 is the average completed by input index 2. Reusing an input tag's offset as in Lesson 6 would not express that relationship correctly.

## Test the graph and the work boundaries

Start `blocks/tutorial/test/qa_MovingAverage.cpp` with a finite graph test using the `GraphHelpers.hpp` copied in Lesson 6:

```cpp
#include "GraphHelpers.hpp"
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/MovingAverage.hpp>

int main() {
    using namespace boost::ut;
    "finite stream produces four averages"_test = [] {
        gr::Graph graph;
        auto& source = tutorial_test::source<float>(graph, {1, 2, 3, 4, 5, 6});
        auto& average = graph.emplaceBlock<gr::tutorial::MovingAverage<float>>({
            {"window_size", gr::Size_t{3}}});
        auto& sink = graph.emplaceBlock<tutorial_test::Sink<float>>({});
        tutorial_test::require(graph.connect<"out", "in">(source, average));
        tutorial_test::require(graph.connect<"out", "in">(average, sink));
        const auto scheduler = tutorial_test::run(std::move(graph));

        const std::vector<float> expected{2, 3, 4, 5};
        expect(eq(sink._samples.size(), expected.size()));
        expect(std::ranges::equal(sink._samples, expected));
    };
}
```

As before, keep the returned scheduler alive while inspecting its sink. The finite source also checks that the graph can finish despite producing fewer outputs than inputs.

The next test must control work boundaries rather than just source values:

```text
logical input:       1 2 3 4 5 6
possible calls:     [1 2] [3 4 5] [6]
outputs per call:     []   [2 3 4] [5]
```

Compare it with one call containing all six samples and with six one-item calls. All three must yield `[2, 3, 4, 5]`. Check the number of outputs actually published, including zero during startup. Also limit output capacity and verify that input which couldn't be processed is retained for the next call.

Use the `Harness` from the [supplied test](../solutions/lesson07/blocks/tutorial/test/qa_MovingAverage.cpp) for these focused checks. It connects real GR4 ports, initializes settings, and calls `start()` once. Its `push` method puts samples into the input buffer. Its `call(input_count, output_capacity, expected_consumed, expected_produced)` method creates real spans, invokes `processBulk`, verifies the buffer counts, and returns the published samples.

The harness uses `SpanReleasePolicy::ProcessNone` so the block's explicit `consume` and `publish` calls determine the counts. Its inner scope releases the work spans before inspecting the buffers. Plain `std::span` arrays can't verify these buffer effects. Copy the harness and its focused tests into your QA file, keeping one `main()` and adding the test cases inside it; you don't need to write this plumbing during the workshop.

Extend the graph cases to cover default window three, window two (`[1.5, 2.5, 3.5, 4.5, 5.5]`), window one, a too-short stream, and complex data. The supplied `graphCheck` helper avoids repeating the graph setup. Use the same harness instance for the changed-versus-unchanged settings checks above, and separately verify that `start()` clears it. Reject zero and values above 1024. The supplied QA file contains all these cases if you need a reference.

## Build and check the registration

Add `include/gnuradio-4.0/tutorial/MovingAverage.hpp` to `GrTutorialBlocks_HDRS` in `blocks/tutorial/CMakeLists.txt`. Add these runtime names to `blocks/tutorial/test/ExpectedBlocks.hpp`:

```text
gr::tutorial::MovingAverage<float32>
gr::tutorial::MovingAverage<complex<float32>>
```

Add the test target to `blocks/tutorial/test/CMakeLists.txt`:

```cmake
add_tutorial_test(qa_MovingAverage)
target_link_libraries(qa_MovingAverage PRIVATE gnuradio4::gr-testing
  gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
```

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
cmake --install build
./build/blocks/tutorial/examples/tutorial_registry
./build/blocks/tutorial/examples/tutorial_plugin_check
```

Restart Studio after installation and check that it exposes `window_size` with default three. A plot is optional; the chunk-boundary test is the evidence that the block works.

<!-- Studio screenshot: MovingAverage selected with window_size exposed. -->

A work call still isn't a signal boundary, even when it happens to contain exactly one window.

---

[Previous: Lesson 6](lesson06-tags.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 8](lesson08-messages.md)
