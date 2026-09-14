# Lesson 0: Setup and build

Run commands from the repository root, not from `docs/`. Continue using the same working tree as you move through the lessons.

GNU Radio is built separately and installed into an SDK prefix. The [GNU Radio 4 repository](https://github.com/gnuradio/gnuradio4) describes the SDK setup. This OOT consumes the installed SDK; the source checkouts are API references, not subprojects of the tutorial.

Use a C++23 compiler and standard library compatible with the SDK, CMake 3.27 or newer, Ninja, Python 3, and Git. The SDK should include core, the block registration generator, standard blocks, and Studio. Lesson 5 also needs the installed ZeroMQ blocks and cppzmq/libzmq development dependencies. The interoperability check additionally needs a GNU Radio 3 Python environment with its ZeroMQ module.

First activate the **already-built** SDK. For the supplied sibling SDK on macOS, run this from the tutorial repository root:

```bash
source /path/to/gnuradio4/build/full/activate.sh
```

For a Linux SDK, use its `build/full/activate.sh` and the compiler used to build it. The SDK's `build/<profile>/projects/gnuradio4-core/CMakeCache.txt` records `CMAKE_CXX_COMPILER`. Activation sets library and package paths, but does **not** select the compiler. Set `CXX` before the first configure of each build directory. On macOS, the system Apple Clang is not the Homebrew LLVM compiler used by this SDK.

The build handles Boost.UT, the header-only test library, automatically. It uses an installed CMake package or header when available; otherwise it downloads the same pinned revision used by `gnuradio4-blocks` and `gr4-incubator` into the build directory. The first configure needs network access if Boost.UT is not installed. No GNU Radio sources are downloaded or rebuilt.

For an offline workshop, configure each working directory in advance, or provide an installed Boost.UT. `-DGR_USE_FETCHCONTENT_DEPS=OFF` disables downloads; `-DBOOST_UT_INCLUDE_DIR=/path/to/include` remains an optional override for a directory containing `boost/ut.hpp`. With `-DENABLE_TESTING=OFF`, Boost.UT is not needed or downloaded.

Stay in the repository root for the workshop. Configure and build the scaffold:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$GR4_PREFIX" \
  -DENABLE_TESTING=ON -DENABLE_EXAMPLES=ON
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

The empty scaffold has nothing to compile and no tests yet; `ctest` reporting “No tests were found” is expected here. The first test appears in Lesson 1. `-j 2` keeps compiler memory use modest; increase it if your machine has room. If you previously built the completed tutorial in this checkout, use a fresh build directory to avoid leftover binaries from later lessons.

For an instructor check of all eight completed lessons, assemble the solutions in a separate directory:

```bash
python3 scripts/prepare_lesson.py 8 build-completed
cd build-completed
```

Run the same configure, build, and test commands there; the completed OOT has nine CTest checks. Use lesson `0` instead of `8` if you want a separate empty practice directory. The helper refuses to overwrite an existing directory. Return to the repository root to begin Lesson 1; preparing a solution does not change your workshop code.

The activation script sets `GR4_PREFIX` to the SDK's install prefix, adds it to the package search path, and sets runtime library and plugin paths. Source it in each shell used for the tutorial, including the shell that launches Studio. If CMake can't find GNU Radio, check the activated environment before adding paths to source trees. There is no need to configure or build the GNU Radio workspace during this tutorial.

## OOT structure

All public block headers live in `blocks/tutorial/include/gnuradio-4.0/tutorial`. Use `namespace gr::tutorial` throughout. Tests go in `blocks/tutorial/test`, and C++ flowgraphs go in `blocks/tutorial/examples`. The prepared GR3 application goes in `gr3_grc`.

The support build uses `find_package(gnuradio4 CONFIG REQUIRED)` and `find_package(GnuRadioBlockLib CONFIG REQUIRED)`. Graph tests and examples also use `find_package(gnuradio4Blocks CONFIG REQUIRED)`. The public header target is `gnuradio4::gr-tutorial`; the generated runtime library is `gnuradio4::GrTutorialBlocksShared`. Both are exported by the OOT package `gnuradio4Tutorial`.

The supplied CMake files handle registration generation, CTest, and package exports. Installation uses `GNUInstallDirs`: the runtime library goes under `${CMAKE_INSTALL_LIBDIR}/gnuradio-4/plugins` and headers under `${CMAKE_INSTALL_INCLUDEDIR}/gnuradio-4.0`. You won't need to write that plumbing for each block.

For each new block, add its header to the list in `blocks/tutorial/CMakeLists.txt` and its QA target to `blocks/tutorial/test/CMakeLists.txt`. Add examples to `blocks/tutorial/examples/CMakeLists.txt` when requested. Below, `test/...` and `examples/...` are relative to `blocks/tutorial/`. Build and run the tests after each change. Once installation is introduced, repeat that check against the installed module too.

The reference snapshots at `solutions/lesson01/` through `solutions/lesson08/` preserve only files touched in that lesson, including build and test changes, at their repository-relative paths. They are cumulative overlays, not standalone projects. To resume from a completed lesson, run, for example, `python3 scripts/prepare_lesson.py 3 build-lesson03` from the repository root, then configure and build in `build-lesson03`. That gives you Gain and the original `processOne` Square, ready for Lesson 4. [Using the solutions](../solutions/README.md) explains how to copy individual answers into your working directory.

The teaching sequence is: a simple calculation → reusable types and discovery → settings → bulk work → a changed rate and PMT payloads → tags → persistent state → messages. For a first pass, concentrate on each block's calculation and its main test. The supplied plugin checks, graph helpers, and GR3 receiver are support code you can use as written; you don't need to type them during the workshop.

---

[Previous: Workshop overview](../README.md) | [Lesson list](../README.md#lessons) | [Next: Lesson 1](lesson01-gain.md)
