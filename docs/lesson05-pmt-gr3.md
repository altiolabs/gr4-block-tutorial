# Lesson 5: PMT and GNU Radio 3 interoperability

So far, each input sample has produced one output sample. Now we'll add two streams and group the sums into packets. Each packet will be one PMT (polymorphic type) value on a normal streaming port. Then we'll send it to a GNU Radio 3 application over ZeroMQ.

There are two checkpoints in this lesson: first prove the packet contents in a local graph, then check the transport with the supplied receiver. If the receiver reports a problem, you'll already know whether the packet calculation works.

Create `Packetizer.hpp` with `Packetizer<T>`, registered for `float` and `std::complex<float>`. Its inputs are sample streams, and its output is a stream of PMT values. PMT is a generic value representation: numbers, strings, vectors, maps/dictionaries, and nested values can all be useful payloads. It isn't itself a transport.

One output stream item will contain a whole vector, not just one sum. With `packet_size = 3`, three samples from each input become one PMT containing three sums. These PMTs travel through ordinary streaming ports and are scheduled like other stream items. They are not the application messages we'll introduce in Lesson 8.

Use `gr::pmt::Value` from `<gnuradio-4.0/Value.hpp>`. A numeric vector is stored as a `gr::Tensor<T>` inside the value:

```cpp
gr::pmt::Value payload(gr::Tensor<float>(gr::data_from,
                                        std::vector<float>{11, 22, 33}));
const auto* samples = payload.get_if<gr::Tensor<float>>();
```

Check the pointer before reading the tensor, or use `holds<gr::Tensor<float>>()` to check its type. A `gr::property_map` is `gr::pmt::Value::Map`, mapping strings to PMT values; we'll use it again for metadata.

## Aggregate several inputs

Start with two six-sample inputs and a packet size of three. Add corresponding samples, then group the sums:

```text
A:       1   2   3  |  4   5   6
B:      10  20  30  | 40  50  60
                    v
PMT 0: [11, 22, 33]    PMT 1: [44, 55, 66]
```

That is six samples consumed from each input and two items produced on the output. GR4 calls this an item-rate relationship, expressed with `Resampling`. Here it describes grouping samples into objects, not interpolating a waveform.

We also need a port collection so each contributing stream has its own input. Here is the complete `Packetizer.hpp`:

```cpp
#pragma once

#include <algorithm>
#include <complex>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Tensor.hpp>
#include <gnuradio-4.0/Value.hpp>

namespace gr::tutorial {

template<typename T>
struct Packetizer : gr::Block<Packetizer<T>, gr::Resampling<1024U, 1U, false>> {
    using Base = gr::Block<Packetizer<T>, gr::Resampling<1024U, 1U, false>>;
    using Base::Base;
    using Description = gr::Doc<"Sum input streams into PMT numeric vectors.">;

    std::vector<gr::PortIn<T>> in{2};
    gr::PortOut<gr::pmt::Value> out;
    gr::Annotated<gr::Size_t, "n_inputs", gr::Limits<1U, 8U>> n_inputs{2U};
    gr::Annotated<gr::Size_t, "packet_size", gr::Limits<1U, 1024U>> packet_size{1024U};

    GR_MAKE_REFLECTABLE(Packetizer, in, out, n_inputs, packet_size);

    void settingsChanged(const gr::property_map&,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("n_inputs")) {
            in.resize(static_cast<std::size_t>(n_inputs.value));
        }
        if (new_settings.contains("packet_size")) {
            this->input_chunk_size = packet_size.value;
            this->output_chunk_size = 1U;
        }
    }

    template<gr::InputSpanLike TInSpan>
    [[nodiscard]] gr::work::Status processBulk(const std::span<TInSpan>& inputs,
                                             std::span<gr::pmt::Value> output) const {
        const auto n = static_cast<std::size_t>(this->input_chunk_size.value);
        for (std::size_t packet = 0; packet < output.size(); ++packet) {
            const auto offset = packet * n;
            std::vector<T> sum(n);
            std::ranges::copy(std::span<const T>(inputs[0].data() + offset, n),
                              sum.begin());
            for (std::size_t channel = 1; channel < inputs.size(); ++channel) {
                const auto slice = std::span<const T>(inputs[channel].data() + offset, n);
                std::ranges::transform(sum, slice, sum.begin(), std::plus<T>{});
            }
            output[packet] = gr::pmt::Value(gr::Tensor<T>(gr::data_from, sum));
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Packetizer, [T], [float, std::complex<float>])
```

`in` is a collection of typed input ports, initialized with two entries. The reflected `n_inputs` setting accepts 1–8 inputs; `packet_size` accepts 1–1024 samples and defaults to 1024. `settingsChanged` resizes the collection and updates the rate. Configure both settings before wiring the graph; to change them later, stop and construct a new graph. We aren't resizing connected ports while processing.

In `Resampling<1024U, 1U, false>`, the first two parameters mean 1024 items from **each** synchronous input produce one output item. The final `false` makes the chunk sizes mutable so the callback can configure them; it doesn't make the ports asynchronous. We keep the output chunk size at one and configure the input size through `packet_size`.

`InputSpanLike` is a C++ concept: a requirement on a type, not a class we construct. In `template<gr::InputSpanLike TInSpan>`, it says each `TInSpan` must behave like a GR4 input span. Such a span gives access to samples through `data()`, indexing, and `size()`, and also has GR4 methods for tags and sample consumption. GR4 supplies the concrete span type when it calls `processBulk`. We need the template here because the input port collection arrives as an outer `std::span<TInSpan>` containing one GR4 input span per connected port.

The outer `inputs` span contains one input span per port: `inputs[0]` holds samples from `in#0`, and `inputs[1]` holds samples from `in#1`. Thus `inputs[1][0]` is the first sample from the second stream, whereas `output[0]` is a whole PMT packet. The input and output spans count different kinds of items.

If `output.size()` is K and packet size is N, the scheduler supplies K * N samples on each input. The outer loop handles each complete packet. Inside it, we copy the first stream's slice, add the other slices with a ranges transform, then copy the sum into a Tensor owned by the PMT value. The payload survives after the work-call spans go away.

All supplied groups are processed, so full-span accounting and `OK` still fit. A work call can contain several packets; it doesn't define the packet boundary. The function allocates payload storage, which is why it isn't `noexcept`.

Use no zero-padding or partial final packets. A final tail shorter than `packet_size` is dropped by the current default `IncompleteFinalUpdatePolicy<IncompleteFinalUpdateEnum::DROP>`. All contributing streams must be present; the shortest input limits the complete groups. Require equal-length sources in the demonstration and test the tail policy separately.

Connect the collection members by name after initializing `n_inputs`. The `#` suffix selects a zero-based member of the reflected `in` collection; it is not a separate C++ field:

```cpp
auto& packetizer = graph.emplaceBlock<gr::tutorial::Packetizer<float>>({
    {"n_inputs", gr::Size_t{2}}, {"packet_size", gr::Size_t{3}}
});
checked(graph.connect(source_a, "out", packetizer, "in#0"));
checked(graph.connect(source_b, "out", packetizer, "in#1"));
```

## Check the packets locally

Create `blocks/tutorial/test/qa_Packetizer.cpp`. Build the finite source and scheduler directly here, as in Lesson 3. Use the same `checked()` helper to report failed connections or scheduler operations:

The numeric recording sink used earlier isn't suitable for these generic PMT payloads. Add this small sink at file scope in your test. It copies each PMT into `values`, preserving the packets for inspection after the graph finishes. It needs no runtime registration because the test constructs it directly by C++ type.

```cpp
#include <boost/ut.hpp>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/testing/TagMonitors.hpp>
#include <gnuradio-4.0/tutorial/Packetizer.hpp>

template<typename Result>
void checked(Result&& result) {
    if (!result) throw gr::exception(result.error().message);
}

struct PmtSink : gr::Block<PmtSink> {
    gr::PortIn<gr::pmt::Value> in;
    std::vector<gr::pmt::Value> values;
    GR_MAKE_REFLECTABLE(PmtSink, in);

    gr::work::Status processBulk(std::span<const gr::pmt::Value> input) {
        values.insert(values.end(), input.begin(), input.end());
        return gr::work::Status::OK;
    }
};
```

Now add a test for the two packets above:

```cpp
int main() {
    using namespace boost::ut;
    "two inputs produce two packets"_test = [] {
        gr::Graph graph;
        auto& a = graph.emplaceBlock<gr::blocks::testing::TagSource<float>>({
            {"values", gr::Tensor<float>(gr::data_from, std::vector<float>{1, 2, 3, 4, 5, 6})},
            {"n_samples_max", gr::Size_t{6}}});
        auto& b = graph.emplaceBlock<gr::blocks::testing::TagSource<float>>({
            {"values", gr::Tensor<float>(gr::data_from, std::vector<float>{10, 20, 30, 40, 50, 60})},
            {"n_samples_max", gr::Size_t{6}}});
        auto& packetizer = graph.emplaceBlock<gr::tutorial::Packetizer<float>>({
            {"n_inputs", gr::Size_t{2}}, {"packet_size", gr::Size_t{3}}});
        auto& sink = graph.emplaceBlock<PmtSink>({});
        checked(graph.connect(a, "out", packetizer, "in#0"));
        checked(graph.connect(b, "out", packetizer, "in#1"));
        checked(graph.connect<"out", "in">(packetizer, sink));
        gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> scheduler;
        checked(scheduler.exchange(std::move(graph)));
        checked(scheduler.runAndWait());

        const std::vector<std::vector<float>> expected{{11, 22, 33}, {44, 55, 66}};
        expect(eq(sink.values.size(), expected.size())) << fatal;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto* samples = sink.values[i].get_if<gr::Tensor<float>>();
            expect(samples != nullptr) << fatal;
            expect(eq(samples->size(), expected[i].size()));
            expect(std::ranges::equal(*samples, expected[i]));
        }
    };
}
```

The `values` and `n_samples_max` settings make each source emit its six values once. The scheduler owns the graph after `exchange`, so keep it alive while inspecting `sink.values`. The first assertion counts packets, not numeric samples. The type check must pass before dereferencing `samples`; `fatal` stops the test if it fails. The remaining checks inspect the vector inside each PMT.

Also check one input, packet size one, a simple complex sum, and a seven-item input producing only two complete packets. Verify default settings and reject out-of-range settings. The [supplied test](../solutions/lesson05/blocks/tutorial/test/qa_Packetizer.cpp) shows those cases. Running through the scheduler checks the rate contract as well as the arithmetic.

Add the header to `GrTutorialBlocks_HDRS` in `blocks/tutorial/CMakeLists.txt`, and extend `test/ExpectedBlocks.hpp` with the Packetizer registration names, as you did for Square. Add these lines to `blocks/tutorial/test/CMakeLists.txt`:

```cmake
add_tutorial_test(qa_Packetizer)
target_link_libraries(qa_Packetizer PRIVATE gnuradio4::gr-testing
  gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
```

Build and run the local checks before introducing transport:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

## Send those objects to GR3

The current ZeroMQ blocks are in-tree in `gnuradio4-blocks`. Use `<gnuradio-4.0/zeromq/ZmqPushSink.hpp>`, namespace `gr::blocks::zeromq`, and link against the installed `gnuradio4::gr-zeromq` target.

The prepared `examples/zmq_packetizer.cpp` should run this graph:

```text
source A -> Gain<float> --+
                         +-> Packetizer<float> -> ZmqPushSink<gr::pmt::Value>
source B ----------------+
                                                          |
                                           GR3 PMT binary over ZeroMQ
                                                          |
                                                          v
                                  GR3: ZMQ PULL Message Source -> Message Debug
```

Use repeating source values `A = [1, 2, 3, 4, 5, 6]` and `B = [10, 20, 30, 40, 50, 60]`, with `packet_size = 3`. Gain doubles A, so GR3 should receive alternating vectors `[12, 24, 36]` and `[48, 60, 72]`. The supplied example paces the sources and runs until Ctrl-C, giving the receiver time to connect.

Configure the sink with `endpoint = "tcp://127.0.0.1:5558"`, `bind = true`, `timeout = 100`, `pass_tags = false`, and **`pmt_wire_format = "GR3"`**. It sends each PMT as one ZeroMQ message. The prepared GR3 assets are `gr3_grc/zmq_packet_receiver.grc` and its runnable Python companion `gr3_grc/zmq_packet_receiver.py`. They use GR3's **ZMQ PULL Message Source**, not its raw stream PULL Source. Its Python constructor is:

```python
zeromq.pull_msg_source("tcp://127.0.0.1:5558", 100, False)
```

Here `False` selects connect: the GR4 sink binds (listens) at the endpoint, and the GR3 receiver connects to that same address. Only one side binds. The sink serializes each GR4 PMT into bytes, and the GR3 Message Source deserializes those bytes into a GR3 PMT and emits it on its message output. This is where our GR4 stream of PMT objects becomes GR3 messages.

The Python companion connects `out` to the `store` input of `blocks.message_debug()`, checks `pmt.is_f32vector`, and reads `pmt.f32vector_elements`; you don't need to write a GR3 block. The GRC version connects to Message Debug's `print` input so you can see the incoming vectors in its console. The payloads are bare uniform vectors, not `(metadata, data)` PDU pairs.

Both frameworks saying “PMT” doesn't establish wire compatibility. With `pmt_wire_format = "GR3"`, the GR4 ZeroMQ block uses the **legacy GR3 PMT binary codec**: a one-dimensional `gr::Tensor<float>` becomes a GR3 f32 uniform vector. That's the representation we check end to end here. Complex-float tensors map to c32 uniform vectors, but arbitrary PMT values, nested structures, and tensor shapes aren't guaranteed to round-trip. Stay with the numeric vectors for this example; there's no codec to write in the OOT.

The supplied SDK defaults to `GR4_YAML_V1`, a different wire format. Set `{"pmt_wire_format", "GR3"}` explicitly in the sink's initial properties. Leaving the default can give you a running GR4 graph while the GR3 receiver cannot decode its messages.

Copy the supplied [publisher](../solutions/lesson05/blocks/tutorial/examples/zmq_packetizer.cpp) and [receiver assets](../solutions/lesson05/gr3_grc/) from the repository root:

```bash
cp solutions/lesson05/blocks/tutorial/examples/zmq_packetizer.cpp blocks/tutorial/examples/
cp -R solutions/lesson05/gr3_grc/. gr3_grc/
```

Add the publisher to `blocks/tutorial/examples/CMakeLists.txt`:

```cmake
add_executable(zmq_packetizer zmq_packetizer.cpp)
target_link_libraries(zmq_packetizer PRIVATE gr-tutorial gnuradio4::gr-zeromq
  gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
```

Build, test, and install as before. Before starting the publisher, check the receiver's Python environment:

```bash
python3 -c 'from gnuradio import blocks, gr, zeromq; import pmt; print(gr.version())'
```

Use a Python interpreter with GNU Radio 3 installed. If that command fails specifically with `No module named 'zmq'`, add `pyzmq` in a local environment that can see the existing GR3 installation:

```bash
python3 -m venv --system-site-packages build/gr3-python
build/gr3-python/bin/python -m pip install pyzmq
```

Then use `build/gr3-python/bin/python` for the receiver. This was needed with the supplied Homebrew Python 3.14 installation. In one activated shell, start the GR4 example:

```bash
./build/blocks/tutorial/examples/zmq_packetizer
```

In a second shell with the SDK activated and GNU Radio 3 Python bindings available, run the receiver from the repository root:

```bash
python3 gr3_grc/zmq_packet_receiver.py
```

If you created the local Python environment above, replace `python3` in this command with `build/gr3-python/bin/python`.

The receiver's acceptance check collects four messages with a bounded timeout, verifies f32 uniform-vector type and length three, and checks that the two expected vectors alternate. It may start on either vector. It prints the values and exits successfully only after those checks. Stop the GR4 example with Ctrl-C. The GRC asset provides the same receive path for inspection with Message Debug. Its generic “no flow control” warning does not require a Throttle block: this receiver uses messages, and the publisher paces the data.

Record the SDK and GR3 versions, GR3 codec, and received vectors with the result. This f32-vector check must pass before calling the tutorial's interoperability path tested; source inspection alone isn't that test. It may remain a manual check when GR3 isn't available to CTest, but Packetizer's deterministic graph test must still run.

Studio can show the GR4 portion using the same installed registrations. Construct the two-input Packetizer before wiring its inputs. Use the C++ example as the required interoperability check; no Studio data-view support for generic PMT payloads is assumed.

For GR4-only signal processing, `gr::Packet<T>` in `<gnuradio-4.0/DataSet.hpp>` may be more convenient. It has typed `signal_values`, a timestamp, and metadata fields. It is a specialized representation for packet-like signal data. `gr::pmt::Value` remains useful for generic structured values, and this lesson deliberately uses it for the GR3 interface.

Once GR3 receives the expected vectors, the path is working. Square remains the bulk block from Lesson 4; Packetizer doesn't incorporate stream tags into its payloads.

---

[Previous: Lesson 4](lesson04-bulk.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 6](lesson06-tags.md)
