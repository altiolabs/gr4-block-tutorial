# Lesson 2: Templates, SIMD, and registration

Go back to `Gain.hpp`. Multiplying by two is useful for more than floats. We can template the sample type, then let the same calculation accept either a scalar or a SIMD value. Here is what Gain looks like now:

```cpp
#pragma once

#include <cstdint>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/meta/utils.hpp>

namespace gr::tutorial {

template<typename T>
struct Gain : gr::Block<Gain<T>> {
    using gr::Block<Gain<T>>::Block;
    using Description = gr::Doc<"Multiply each input sample by two.">;

    gr::PortIn<T> in;
    gr::PortOut<T> out;
    T factor = T{2};
    GR_MAKE_REFLECTABLE(Gain, in, out);

    template<gr::meta::t_or_simd<T> V>
    [[nodiscard]] constexpr V processOne(const V& input) const noexcept {
        return static_cast<V>(input * factor);
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Gain, [T], [float, std::int16_t, std::int32_t, std::uint8_t])
```

The first change is `template<typename T>`: the ports now carry `T`, and the CRTP base becomes `Block<Gain<T>>`. A scalar-only version of the function would be:

```cpp
[[nodiscard]] constexpr T processOne(T input) const noexcept {
    return static_cast<T>(input * T{2});
}
```

The complete version above takes that one step further. `gr::meta::t_or_simd<T>` allows `V` to be the scalar type or a compatible SIMD value. Multiplication by `T{2}` works for both. The cast returns the declared type even when scalar integer promotion produces an `int`. GR4 can select vector processing where the sample type and build support it, while keeping the scalar path.

The registration marker creates a runtime factory for each listed type, substituting it for `[T]`. Update existing uses to `Gain<float>` and rebuild; it still multiplies by two.

GR4 normalizes runtime type names to portable spellings:

| C++ type | Runtime name |
| --- | --- |
| `gr::tutorial::Gain<float>` | `gr::tutorial::Gain<float32>` |
| `gr::tutorial::Gain<std::int16_t>` | `gr::tutorial::Gain<int16>` |
| `gr::tutorial::Gain<std::int32_t>` | `gr::tutorial::Gain<int32>` |
| `gr::tutorial::Gain<std::uint8_t>` | `gr::tutorial::Gain<uint8>` |

Use the C++ spelling in code and the normalized name for registry lookups. These four specializations replace the original non-template `gr::tutorial::Gain` registration.

Keep each registration macro on one physical line; the current generator doesn't parse multiline declarations.

Extend `qa_Gain.cpp` to test at least `float` and `std::int16_t`, with nonzero positive and negative inputs and doubled outputs. Keep integer test values representable after multiplication. This block doesn't add clipping or saturation; ordinary C++ integer arithmetic and conversion rules apply.

Also test the new `processOne` template with a float SIMD value. The completed test uses:

```cpp
"float SIMD lanes"_test = [] {
    gr::tutorial::Gain<float> gain;
    using V = gr::meta::simdize<float>;
    V input([](auto i) { return static_cast<float>(i) - 2.5f; });
    const V output = gain.processOne(input);
    for (std::size_t i = 0; i < V::size(); ++i) {
        expect(eq(output[i], 2.0f * input[i]));
    }
};
```

`simdize<float>` from `<gnuradio-4.0/meta/utils.hpp>` is a value containing several float lanes. The constructor gives each lane a different value, so the check can catch a wrong result in any lane. `gain.processOne(input)` instantiates the same function template for this SIMD type; the loop checks each doubled lane with `operator[]`. `V::size()` makes the test work without assuming a particular lane count. This proves that the function accepts and calculates with the vector type; it does not promise a particular machine instruction.

Now check that GR4 can find and create the registered blocks. Add `test/qa_TutorialAvailableBlocks.cpp`, include `<gnuradio-4.0/GrTutorialBlocks.hpp>`, and initialize the registry:

```cpp
gr::blocklib::initGrTutorialBlocks(gr::globalBlockRegistry());
auto& registry = gr::globalBlockRegistry();
```

Check that `registry.contains("gr::tutorial::Gain<float32>")` is true and `registry.create("gr::tutorial::Gain<float32>", {})` returns a block. Repeat for the other three types. The [completed test](../solutions/lesson02/blocks/tutorial/test/qa_TutorialAvailableBlocks.cpp) shows how to check them using the list in `ExpectedBlocks.hpp`.

Add the test and its library link to `blocks/tutorial/test/CMakeLists.txt`:

```cmake
add_tutorial_test(qa_TutorialAvailableBlocks)
target_link_libraries(qa_TutorialAvailableBlocks PRIVATE gnuradio4::GrTutorialBlocksShared)
```

Two ready-made programs are in `solutions/lesson02/blocks/tutorial/examples/`:

- [tutorial_registry.cpp](../solutions/lesson02/blocks/tutorial/examples/tutorial_registry.cpp) checks and prints the four registered Gain types.
- [tutorial_plugin_check.cpp](../solutions/lesson02/blocks/tutorial/examples/tutorial_plugin_check.cpp) checks that GR4 can load the tutorial plugin and create those same types from it.

The scaffold's `blocks/tutorial/plugin.cpp` supplies the plugin entry point. Use these checks as supplied; we'll run them below. From the repository root, copy the helper files and their example build list into place, along with the external consumer used later in this lesson:

```bash
cp solutions/lesson02/blocks/tutorial/examples/* blocks/tutorial/examples/
cp solutions/lesson02/blocks/tutorial/test/ExpectedBlocks.hpp blocks/tutorial/test/
cp -R solutions/lesson02/test_external .
```

As later lessons add blocks, add their normalized runtime names to `ExpectedBlocks.hpp` so the checks cover them too.

## Install it and look in Studio

Build and test, then install into the active SDK:

```bash
cmake -S . -B build
cmake --build build -j 2
ctest --test-dir build --output-on-failure
cmake --install build
./build/blocks/tutorial/examples/tutorial_registry
./build/blocks/tutorial/examples/tutorial_plugin_check
gr4-studio
```

Run the plugin check with the activation script's installed plugin paths, so it checks the installation rather than the build directory.

The plugin loader reads `GNURADIO4_PLUGIN_DIRECTORIES`. The installed library contributes its registrations to the process hosting the graph. Studio's desktop launcher starts a local `gr4cp_server`; Studio gets its block catalog from that server's `/blocks` API. It doesn't scan your tutorial headers or build folder.

Search the block catalog for `tutorial` or `Gain`, select the float variant, and place it on the graph canvas. Check the type ID against `gr::tutorial::Gain<float32>`. Close and relaunch Studio after reinstalling a changed plugin so its local server loads the new library. With a remote Studio connection, the OOT must be installed in the remote server's environment instead.

<!-- Studio screenshot: installed tutorial Gain in the catalog and on the canvas, with the float32 type ID visible. -->

The supplied `test_external/consumer` is a separate CMake project that finds `gnuradio4Tutorial`, links `gnuradio4::gr-tutorial`, includes `<gnuradio-4.0/tutorial/Gain.hpp>`, and checks a doubled float using the installed header. Run it without adding the tutorial source directory to its includes:

```bash
cmake -S test_external/consumer -B build-consumer -G Ninja -DCMAKE_CXX_COMPILER="$CXX"
cmake --build build-consumer
./build-consumer/tutorial_consumer
```

If that works and Studio shows Gain, both ways of using the OOT are working: an external C++ application can include the block, and a runtime host can discover a registered specialization. Gain keeps this fixed-factor behavior for the rest of the tutorial.

---

[Previous: Lesson 1](lesson01-gain.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 3](lesson03-settings.md)
