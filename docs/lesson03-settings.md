# Lesson 3: Settings and derived state

Create a new `Square.hpp`. Keep Gain as it is. Square will use `processOne` initially and compute:

```text
y = gain_linear * x * x
gain_linear = 10^(gain_db / 20)
```

We'll support `float` and `std::complex<float>`, leaving integer rounding and saturation out of this block. Here is `Square.hpp`, with the setting and the calculation together:

```cpp
#pragma once

#include <cmath>
#include <complex>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

template<typename T>
struct Square : gr::Block<Square<T>> {
    using gr::Block<Square<T>>::Block;
    using Description = gr::Doc<"Square the input and apply a gain in dB.">;

    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::Annotated<float, "gain_db", gr::Doc<"Gain applied after squaring">,
                  gr::Unit<"dB">> gain_db{0.0f};

    GR_MAKE_REFLECTABLE(Square, in, out, gain_db);

    void settingsChanged(const gr::property_map&,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("gain_db")) {
            _gain_linear = std::pow(10.0f, gain_db.value / 20.0f);
        }
    }

    [[nodiscard]] T processOne(T input) const noexcept {
        return _gain_linear * input * input;
    }

private:
    float _gain_linear{1.0f};
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Square, [T], [float, std::complex<float>])
```

A setting starts as a member exposed through reflection. `gain_db` is a real float even when the stream is complex. The annotation adds documentation and units; it doesn't convert dB for us. At the default 0 dB, the multiplier is one. For complex input, square means `x * x`, not magnitude squared: `(1 + 2i)^2` is `-3 + 4i`.

`settingsChanged` receives the old settings and the applied update. The member already has its new value when the callback runs. That update isn't necessarily a complete settings map, so we check for `"gain_db"` before recalculating. There's no need to call back into `settings()` from the callback.

The private `_gain_linear` member is cached derived state, not another setting - so we don't add it into the reflection list. Doing the power calculation once per setting change saves repeating it for every sample. Its initializer also gives a default-constructed block the correct 0 dB behavior.

The inherited constructors accept initial properties. In a direct test, configure and initialize the block like this, supplying the gain as a number:

```cpp
Square<float> square(gr::property_map{{"gain_db", 6.0206f}});
square.settings().init();
std::ignore = square.settings().applyStagedParameters();
```

Those last two calls are for a direct unit test outside Graph/Scheduler. The constructor records the initial properties; initialization applies them. Merely constructing a block and immediately calling `processOne` doesn't run the graph initialization path. `graph.emplaceBlock<Square<float>>({{"gain_db", 6.0206f}})` handles block initialization as part of graph construction.

For a later change in a direct test, check that `square.settings().setStaged({{"gain_db", 0.0f}})` returns an empty map, then call `applyStagedParameters()` before processing more samples. The returned map identifies properties that weren't accepted as settings; invalid types can also raise an error. In a running graph, use the settings interface and let the runtime apply staged changes at its processing boundary. Assigning `square.gain_db` directly doesn't notify the cache.

Create `test/qa_Square.cpp`. Check default outputs `1, 4, 12.25` for inputs `1, -2, 3.5`. With `gain_db = 20.0f`, the multiplier is ten and the outputs are `10, 40, 122.5`. Check the complex example at 0 dB, and change an initialized block from 20 dB back to 0 dB through staged settings. Use a reasonable float tolerance rather than exact equality for the conversion. Do not call `settingsChanged` manually to make a test pass.

## Our first C++ flowgraph

So far, our tests have called a block's processing function directly. Now we'll let GR4 move samples through connected blocks and call their processing functions for us. A flowgraph describes the blocks and their connections; a scheduler runs it.

We'll use three blocks:

```text
TagSource<float> → Square<float> → TagSink<float>
  [1, -2, 3.5]     gain_db = 20    [10, 40, 122.5]
```

`TagSource` and `TagSink` are supplied GR4 testing helpers. The source produces known samples, and the sink records the results so we can inspect them. Despite their names, we don't need to supply any tags for this example. `TagSource<float>` defaults to bulk processing. This SDK requires an explicit processing mode for `TagSink`; Square still uses its `processOne` function.

Create `blocks/tutorial/examples/square_graph.cpp` with the following program. The [Lesson 3 example](../solutions/lesson03/blocks/tutorial/examples/square_graph.cpp) is also available to copy.

```cpp
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/testing/TagMonitors.hpp>
#include <gnuradio-4.0/tutorial/Square.hpp>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

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
```

Let's walk through what happens. Each `graph.emplaceBlock<Type>(settings)` creates and initializes a block inside the graph, then returns a reference to it. The graph owns the blocks; `auto&` lets us refer to them when making connections. Here we specify the C++ types directly, so we don't need to look them up in the runtime registry.

The properties passed to `emplaceBlock` set up each block before it runs. Square receives `gain_db = 20.0f`, which applies the setting and prepares its cached multiplier. The source receives its sample values as a `gr::Tensor<float>`. It repeats those values until `n_samples_max` is reached, so setting that limit to three makes this a finite example. The empty `{}` gives the sink its default settings, which include recording received samples.

The two `connect<"out", "in">` calls wire the named output port of the first block to the named input port of the second. These names match the reflected port members in the block definitions. All three blocks use float samples at these connections. Each call returns `std::expected`. The small `checked()` helper throws with the reported error when a connection, scheduler exchange, or run fails.

Once the graph is wired, `scheduler.exchange(std::move(graph))` transfers it to the scheduler. `runAndWait()` starts processing and waits for completion, arranging work as input samples and output space become available. We never call Square's `processOne` ourselves. After the source finishes its three samples and the downstream blocks finish processing them, the call returns. Both scheduler calls return results that we check for errors too.

The scheduler still owns the blocks when we inspect `sink._samples`, so the sink reference remains valid here. We check the sample count and values, then print them. This proves that settings, connections, processing, and completion work together in a running graph.

Add the executable to `blocks/tutorial/examples/CMakeLists.txt`. `gnuradio4::gr-testing` provides the source and sink helpers:

```cmake
add_executable(square_graph square_graph.cpp)
target_link_libraries(square_graph PRIVATE gr-tutorial gnuradio4::gr-testing
  gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
if(ENABLE_TESTING)
  add_test(NAME square_graph COMMAND square_graph)
  set_tests_properties(square_graph PROPERTIES TIMEOUT 30)
endif()
```

Build and run it from the repository root:

```bash
cmake -S . -B build
cmake --build build -j 2
ctest --test-dir build --output-on-failure
./build/blocks/tutorial/examples/square_graph
```

The program should print `10`, `40`, and `122.5` on separate lines and exit successfully. It also runs as the `square_graph` CTest check, with a timeout to catch a graph that fails to finish.

Install the updated module with `cmake --install build` and reopen Studio. Add Square's float variant and select it. The block inspector should show `gain_db` with default `0`; change the graph property to `20` and use it on the next graph run. Studio also has a runtime-settings view for a linked, running session. Editing a saved graph property and changing a running block are distinct operations. Don't assume a graph edit immediately updates a live session. The unit annotation is available in GR4 metadata, but the current Studio parameter model doesn't guarantee a displayed dB suffix.

<!-- Studio screenshot: Square selected, gain_db default and edited graph property visible. -->

The `lesson03` snapshot adds Square, its QA test and finite graph example, and their registration/build entries. The block still has `processOne`; there is no bulk implementation yet.

---

[Previous: Lesson 2](lesson02-templates.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 4](lesson04-bulk.md)
