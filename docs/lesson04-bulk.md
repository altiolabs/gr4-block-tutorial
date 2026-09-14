# Lesson 4: Bulk processing

Let's keep working on Square. The calculation doesn't need bulk processing, but it gives us a useful comparison before the next block needs a different rate.

With `processOne`, we describe the calculation for one sample and GR4 supplies the surrounding loop. With `processBulk`, we receive a batch of samples in one call and write the loop ourselves. This is useful when an algorithm works on buffers or when we want to pass a whole batch to a DSP library.

## When bulk is useful

- **Grouping or changing the item rate:** In Lesson 5, Packetizer takes several samples from each input stream and produces one packet object. Its work function needs to see the input spans and output space together to form complete packets.
- **Calling a buffer based algorithm:** If a DSP library accepts an input buffer and writes an output buffer, `processBulk` can pass the spans to that operation in one call. A scalar `processOne` function has no batch to pass along.

Square needs neither ability. We use it here so the new work interface is the only change; for a simple independent calculation, `processOne` remains the shorter choice. A bulk call is just a scheduler supplied chunk, not automatically one packet or one DSP frame. The block must define any boundaries it needs.

Switching to bulk doesn't automatically make a block faster: GR4 already optimizes `processOne`, including SIMD where supported.

We are replacing the `processOne` from the previous lesson with this complete function. Add `<algorithm>` and `<span>` to `Square.hpp`:

```cpp
[[nodiscard]] gr::work::Status processBulk(std::span<const T> input,
                                         std::span<T> output) const noexcept {
    std::ranges::transform(input, output.begin(), [this](T value) {
        return _gain_linear * value * value;
    });
    return gr::work::Status::OK;
}
```

The ranges transform applies the same calculation to each item in the span. Remove the old `processOne`; Square now has only `processBulk`. The types, setting, callback, and cache stay as they were, so the DSP behavior is unchanged.

For this synchronous one-to-one block, GR4 supplies equally sized input and output spans. With the plain `std::span` form we process all of both spans, and the framework handles full-span consumption and publication after the call. Returning `OK` is a status, not a sample count. Don't add separate calls to the ports' readers or writers inside this function.

The spans are views valid for this work call. Don't store them or pointers into them in the block. Also, the scheduler can provide arbitrary chunks: a work call is not a frame, a packet, a complete signal, or any other application boundary. Today that makes no difference to a square. It will matter when the output depends on earlier samples.

The direct test now supplies storage for both the input and output. Add `<array>` and `<span>` to `test/qa_Square.cpp`, and replace the default-value test inside `main()` with:

```cpp
"default bulk square"_test = [] {
    gr::tutorial::Square<float> square;
    const std::array input{1.0f, -2.0f, 3.5f};
    std::array<float, 3> output{};
    const std::array expected{1.0f, 4.0f, 12.25f};

    const auto status = square.processBulk(
        std::span<const float>{input}, std::span<float>{output});

    expect(status == gr::work::Status::OK);
    expect(output == expected);
};
```

This uses the same `using namespace boost::ut;` as the earlier tests. Each span refers to an existing array without copying it. The input view is read-only, and the output view lets Square write into `output`. In this direct call, we must provide equally sized spans ourselves. The return value tells us whether processing succeeded; the calculated samples are in the output array. We check both.

Update the remaining tests to call `processBulk` too, keeping the settings and complex assertions from Lesson 3. Keep applying staged settings before each affected call and use the same float tolerances for the dB conversion. The [Lesson 4 test](../solutions/lesson04/blocks/tutorial/test/qa_Square.cpp) shows a small helper that wraps a single input and output in one-item spans, so those existing checks can stay concise.

Then test several chunk sizes. Process the same signal once as a whole span and again as several subspans, including a one-item span, and compare the combined outputs. A direct empty-span call should return `OK` without writing anything. These checks establish that Square's result doesn't depend on how the scheduler divides up the work.

The `square_graph.cpp` from Lesson 3 needs no source changes. Square still has the same float ports and `gain_db` setting, so the graph is wired and configured exactly as before. When rebuilt, GR4 automatically calls `processBulk` to do the work. Run both the updated direct tests and that existing flowgraph:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R 'qa_Square|qa_Tutorial'
./build/blocks/tutorial/examples/square_graph
cmake --install build
```

The flowgraph should still print `10`, `40`, and `122.5` and exit successfully. At the end of `lesson04`, Square uses `processBulk`; Gain still uses its scalar/SIMD `processOne`. Blocks using either interface can work together in the same graph.

---

[Previous: Lesson 3](lesson03-settings.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 5](lesson05-pmt-gr3.md)
