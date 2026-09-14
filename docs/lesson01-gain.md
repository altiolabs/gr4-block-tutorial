# Lesson 1: Your first GNU Radio 4 block

Create `Gain.hpp` under the public header directory. Let's give our first block something simple and visible to do such as multiply the input by 2:

```text
input -> Gain -> output = 2 * input
```

We will use a concrete `float` block for now. The factor is fixed at `2.0f`; there is no configurable gain yet.

Here is the complete first version of `Gain.hpp`:

```cpp
#pragma once

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>

namespace gr::tutorial {

struct Gain : gr::Block<Gain> {
    using gr::Block<Gain>::Block;
    using Description = gr::Doc<"Multiply each input sample by two.">;

    gr::PortIn<float> in;
    gr::PortOut<float> out;
    float factor = 2.0f;
    GR_MAKE_REFLECTABLE(Gain, in, out);

    [[nodiscard]] constexpr float processOne(float input) const noexcept {
        return input * factor;
    }
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::Gain)
```

`Gain` derives from `Block<Gain>` so the framework can inspect the concrete block and supply its scheduler hooks. The `using` declaration inherits the base constructors. There isn't much hidden in the processing function: it takes one input item and returns one output item.

The ports declare the streaming interface. Their types are known to the compiler, so a float input isn't just a buffer that we hope contains floats. Reflection gives GR4 access to those members and their names. The `Description` supplies documentation with the block definition which gets compiled into the block as we will see later.

For this one-to-one operation, `processOne` is the simplest work function. GR4 supplies the loop around it in an optimized manner. Keep it `const` because processing a sample doesn't change this block's state, and `noexcept` because this multiplication doesn't throw. We'll make use of that distinction later.

The `GR_REGISTER_BLOCK` line marks the class for runtime registration. Creating the header, though, isn't enough for runtime discovery.

We have to do a small bit of CMake work to put the header in the registration list so it will install usable plugins cleanly:

```cmake
set(GrTutorialBlocks_HDRS
    include/gnuradio-4.0/tutorial/Gain.hpp)

# The scaffold supplies this object target and the shared library around it.
gr_generate_block_instantiations(
  GrTutorialBlocksObject
  HEADERS ${GrTutorialBlocks_HDRS}
  MODULE_NAME_BASE GrTutorialBlocks)
```

The macro is a marker for the build's registration generator. The generator writes and compiles the concrete registration code - including the header in some unrelated executable doesn't perform that step. We'll look at the runtime result next. In the scaffold, edit the existing header list; the generator call is already supplied.

Create `test/qa_Gain.cpp`, using Boost.UT, and call `processOne` directly on a `Gain`. Check all three pairs:

```text
input:   1.0  -2.0  3.5
output:  2.0  -4.0  7.0
```

This test doesn't need a graph. It's checking a small deterministic transformation, and a direct call makes a wrong multiplication easy to find. The complete file can be this small:

```cpp
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/Gain.hpp>

int main() {
    using namespace boost::ut;
    "multiply by two"_test = [] {
        gr::tutorial::Gain gain;
        expect(eq(gain.processOne(1.0f), 2.0f));
        expect(eq(gain.processOne(-2.0f), -4.0f));
        expect(eq(gain.processOne(3.5f), 7.0f));
    };
}
```

Add `add_tutorial_test(qa_Gain)` to `blocks/tutorial/test/CMakeLists.txt`, then check the whole step:

```bash
cmake -S . -B build
cmake --build build -j 2
ctest --test-dir build --output-on-failure -R qa_Gain
```

Registration code is generated at configure time; Ninja also requests a reconfigure when a listed header changes. That's the end state for `lesson01`: a float-only Gain, its test, and the header/test build-list entries. Next we'll change the type list and watch the generated registrations change with it.

---

[Previous: Lesson 0](lesson00-setup.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 2](lesson02-templates.md)
