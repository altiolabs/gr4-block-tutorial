# Lesson 6: Stream tags

The PMT objects gave us a place to put any arbitrary data and/or metadata. Sometimes the samples should remain a stream and we just want to attach information to a particular sample. That's what tags are for.

A tag consists of a stream position and a property map. For example, attach `{"tutorial.input": 42}` to the fourth sample of a six-sample stream:

```text
stream index:  0   1   2   3   4   5
sample value:  1   2   3   4   5   6
                          ^
                          {"tutorial.input": 42}
```

Indices are zero-based: index 3 identifies the sample whose value is 4, not the sample whose value is 3. The tag accompanies that sample; it doesn't insert another sample or change its value. A downstream block can use it to interpret the stream, for example as a burst boundary or a measurement annotation. Our `tutorial.input` key is just an application-defined label.

The sample processing in `TagForwarder.hpp` is a float copy. Our goal is to copy every incoming tag unchanged and add our own `tutorial.marker` tag at the first output sample of each run. We use `gr::NoTagPropagation` to turn off automatic forwarding and take responsibility for forwarding the maps ourselves. This makes the tag handling explicit in the exercise.

Create `blocks/tutorial/include/gnuradio-4.0/tutorial/TagForwarder.hpp`:

```cpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

struct TagForwarder : gr::Block<TagForwarder, gr::NoTagPropagation> {
    using Base = gr::Block<TagForwarder, gr::NoTagPropagation>;
    using Base::Base;
    using Description = gr::Doc<"Copy samples and manually forward their tags.">;

    gr::PortIn<float> in;
    gr::PortOut<float> out;
    GR_MAKE_REFLECTABLE(TagForwarder, in, out);

    void start() { _marker_emitted = false; }

    [[nodiscard]] gr::work::Status processBulk(gr::InputSpanLike auto& input,
                                             gr::OutputSpanLike auto& output) {
        std::ranges::copy(input, output.begin());
        if (!_marker_emitted && !output.empty()) {
            output.publishTag(gr::property_map{{"tutorial.marker", true}}, 0UZ);
            _marker_emitted = true;
        }
        for (const auto& [relative_index, map_ref] : input.tags(input.size())) {
            if (relative_index >= 0) {
                output.publishTag(map_ref.get(), static_cast<std::size_t>(relative_index));
            }
        }
        return gr::work::Status::OK;
    }

private:
    bool _marker_emitted{false};
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::TagForwarder)
```

## Read and publish tags alongside samples

In Lesson 4, `std::span` gave us access to the samples. Here we also need GR4's tag methods, so the parameters accept types satisfying `InputSpanLike` and `OutputSpanLike`. The `auto&` parameters let GR4 supply its concrete span types. They still support iteration, indexing, `size()`, and `begin()`, so the sample copy is unchanged; they additionally provide methods such as `tags(...)` and `publishTag(...)`.

`input.tags(input.size())` selects tags before the end of the input sample span. Each entry gives us a relative index and a reference wrapper around a property map. The structured binding names these `relative_index` and `map_ref`; `map_ref.get()` retrieves the referenced map so we can pass it to the output unchanged.

The sample copy uses full-span accounting as before: we copy every supplied sample and return `OK`. With this block's accounting, input tag consumption follows sample consumption; don't separately consume the tag buffer here.

## Keep positions correct across work calls

Tags in a buffer use absolute stream indices, but `input.tags(...)` reports offsets relative to the first sample of the current span. The normal scheduling path splits work at tag boundaries. Suppose our six samples arrive in two calls:

| Work call | Absolute sample indices | Tag at absolute index 3 |
| --- | --- | --- |
| First | 0, 1, 2 | Not forwarded in this call |
| Second | 3, 4, 5 | Appears at relative offset 0 |

`output.publishTag(map, offset)` also uses an offset relative to its current span. In the second call, publishing at offset 0 attaches the tag to output stream index 3. Since this block copies one-to-one, we can reuse the input offset directly. A block that changes the number of samples would need to decide how tag positions should map to its output.

The upper bound in `tags(until)` is exclusive. In the first call, `input.size()` is 3, so a tag at relative index 3 belongs to the next span, not this one. This is why the loop uses `tags(input.size())` rather than the unbounded `tags()` view.

## Emit the marker once per run

GR4 invokes `start()` when the block starts running. It resets `_marker_emitted`, which remains a member of the block between work calls. The first call with output samples publishes the marker at offset 0 and sets the flag; later calls just copy samples and their tags. An empty call leaves the flag clear because there is no output sample to mark yet.

The constructor's initial `false` is enough for a newly created block, but not for an existing block that starts another run. Resetting the flag in `start()` handles that case. In a normal graph, let the lifecycle call this hook; you don't call it before every work call.

A few API details are worth keeping in mind once that path is clear:

- The general tag API can report negative offsets for tags carried from earlier work. We check the sign before converting to `size_t`. With this block's ordinary tag-boundary scheduling, the tags we forward have offsets from zero through `input.size() - 1`.
- Default forwarding handles configured auto-forward keys and may substitute a block's current setting value. It isn't a promise to copy every arbitrary user key unchanged. `NoTagPropagation` disables that forwarding, but doesn't disable lifecycle/end-of-stream handling or all settings processing.
- For `processOne`, the related APIs are `inputTagsPresent()`, `mergedInputTag()`, and `this->publishTag(map)`. Stay with the span APIs here: the current `mergedInputTag()` API is restricted to `processOne` blocks.

## Check samples, positions, and restart behavior

Create `blocks/tutorial/test/qa_TagForwarder.cpp`. Copy [GraphHelpers.hpp](../solutions/lesson06/blocks/tutorial/test/GraphHelpers.hpp) into `blocks/tutorial/test/` to reuse the finite-source and scheduler setup you wrote out in Lesson 5:

```bash
cp solutions/lesson06/blocks/tutorial/test/GraphHelpers.hpp blocks/tutorial/test/
```

The helper's `tutorial_test` namespace contains `source` for a finite source, `Sink` for a numeric recording sink, `require` for result checks, and `run` for the scheduler. The graph is:

```text
TagSource<float> -> TagForwarder -> TagSink<float>
```

Unlike Lesson 5's PMT sink, this sink records numeric samples in `_samples` and tags in `_tags`. Set the source's `_tags` before running the graph. For the six values above, the expected application tags at the sink are:

```text
index 0: {"tutorial.marker": true}
index 3: {"tutorial.input": 42}
```

This test checks both the unchanged samples and the exact tag maps and positions:

```cpp
#include "GraphHelpers.hpp"
#include <cstdint>
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/TagForwarder.hpp>

int main() {
    using namespace boost::ut;
    "samples and tags survive different chunk sizes"_test = [] {
        for (const std::size_t chunk : {1UZ, 2UZ, 6UZ}) {
            gr::Graph graph;
            const std::vector<float> values{1, 2, 3, 4, 5, 6};
            auto& source = tutorial_test::source(graph, values);
            source.out.max_samples = chunk;
            source._tags = {gr::Tag{3UZ, {{"tutorial.input", std::int64_t{42}}}}};
            auto& forwarder = graph.emplaceBlock<gr::tutorial::TagForwarder>({});
            auto& sink = graph.emplaceBlock<tutorial_test::Sink<float>>({});
            tutorial_test::require(graph.connect<"out", "in">(source, forwarder));
            tutorial_test::require(graph.connect<"out", "in">(forwarder, sink));
            const auto scheduler = tutorial_test::run(std::move(graph));

            expect(sink._samples == values);
            std::vector<gr::Tag> application_tags;
            for (const auto& tag : sink._tags) {
                if (tag.map.contains("tutorial.marker") || tag.map.contains("tutorial.input")) {
                    application_tags.push_back(tag);
                } else {
                    expect(tag.map.contains(gr::tag::END_OF_STREAM.shortKey()));
                }
            }
            expect(eq(application_tags.size(), 2UZ)) << fatal;
            expect(eq(application_tags[0].index, 0UZ));
            expect(application_tags[0].map == gr::property_map{{"tutorial.marker", true}});
            expect(eq(application_tags[1].index, 3UZ));
            expect(application_tags[1].map == gr::property_map{{"tutorial.input", std::int64_t{42}}});
        }
    };
}
```

Keep the returned scheduler alive while reading the sink, just as in Lesson 5. The graph owns the sink, and the scheduler owns the graph.

`source.out.max_samples` caps the source's work chunks; it doesn't force every downstream call to have that exact size. Testing caps of one, two, and six makes sure the result doesn't depend on receiving all six samples in one call. A marker published on every call would produce extra tags and fail the count assertion.

End-of-stream is a lifecycle signal that lets the finite graph finish, not an extra tutorial annotation. The loop separates our application tags from it and rejects unexpected non-EOS tags. The exact map comparisons also catch extra keys attached to our tags. We deliberately supply no additional metadata in this test.

Each iteration above constructs a fresh block. That checks chunk independence, but cannot prove that `start()` resets an existing block's marker. Add the separate `"empty work and restart rearm marker"` test from the [supplied solution](../solutions/lesson06/blocks/tutorial/test/qa_TagForwarder.cpp). It keeps the same forwarder instance, calls its `start()` hook, gives it an empty work call followed by one sample, and checks for one marker. It repeats this sequence on that same instance. This focused test invokes the hook directly to check its reset behavior; the graph test above exercises normal scheduling. Together they catch a marker emitted once per call, lost on empty work, or not re-armed for a later run.

Add `include/gnuradio-4.0/tutorial/TagForwarder.hpp` to `GrTutorialBlocks_HDRS` in `blocks/tutorial/CMakeLists.txt`. Add `"gr::tutorial::TagForwarder"` to the names in `blocks/tutorial/test/ExpectedBlocks.hpp`, and add these lines to `blocks/tutorial/test/CMakeLists.txt`:

```cmake
add_tutorial_test(qa_TagForwarder)
target_link_libraries(qa_TagForwarder PRIVATE gnuradio4::gr-testing
  gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
```

Build and run the checks:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Install as before and run `tutorial_registry` and `tutorial_plugin_check` to check the new registration.

A tag's map contains the same generic value types we used in Lesson 5, but the tag adds a stream position. A setting configures a block. Metadata inside a PMT object is part of that object's payload. None of these automatically turns into either of the others. Packetizer remains unchanged and makes no promise to incorporate incoming tags into its PMT vectors.

---

[Previous: Lesson 5](lesson05-pmt-gr3.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 7](lesson07-stateful.md)
